#include "World.h"
#include "FastNoiseLite.h"
#include <algorithm>
#include <cmath>
#include <random>

World::World(int width, int height, int tileSize, unsigned int seed)
    : width(width), height(height), tileSize(tileSize), seed(seed)
{
    tiles.resize(width * height);
}

void World::regenerate(unsigned int newSeed)
{
    seed = newSeed;
    initialize();
}

Tile& World::tileAt(int x, int y)
{
    return tiles[y * width + x];
}

const Tile& World::getTile(int x, int y) const
{
    return tiles[y * width + x];
}

int World::getWidth()    const { return width; }
int World::getHeight()   const { return height; }
int World::getTileSize() const { return tileSize; }

// ---------------------------------------------------------------------------
// Stage 1 — Blue-noise plate placement

void World::generatePlates()
{
    m_plates.clear();

    const int   PW  = width  / PLATE_SCALE;
    const int   PH  = height / PLATE_SCALE;
    const int   N   = 12;
    const int   nOc = 8;
    const float minDist = std::sqrt(static_cast<float>(PW * PH) / (3.14159f * N)) * 0.85f;

    std::mt19937 rng(seed + 9999);
    std::uniform_real_distribution<float> distX(1.f, static_cast<float>(PW - 1));
    std::uniform_real_distribution<float> distY(1.f, static_cast<float>(PH - 1));
    std::uniform_real_distribution<float> distAngle(0.f, 6.28318f);

    // Poisson disk dart-throwing
    int attempts = 0;
    while (static_cast<int>(m_plates.size()) < N && attempts < 10000)
    {
        ++attempts;
        float cx = distX(rng);
        float cy = distY(rng);

        bool tooClose = false;
        for (const auto& p : m_plates)
        {
            float dx = cx - p.centerX, dy = cy - p.centerY;
            if (dx * dx + dy * dy < minDist * minDist) { tooClose = true; break; }
        }
        if (tooClose) continue;

        float angle = distAngle(rng);
        TectonicPlate plate;
        plate.centerX = cx;
        plate.centerY = cy;
        plate.driftX  = std::cos(angle);
        plate.driftY  = std::sin(angle);
        plate.oceanic = static_cast<int>(m_plates.size()) < nOc;
        m_plates.push_back(plate);
    }

    // Fallback: fill any remaining slots without distance constraint
    while (static_cast<int>(m_plates.size()) < N)
    {
        float angle = distAngle(rng);
        TectonicPlate plate;
        plate.centerX = distX(rng);
        plate.centerY = distY(rng);
        plate.driftX  = std::cos(angle);
        plate.driftY  = std::sin(angle);
        plate.oceanic = static_cast<int>(m_plates.size()) < nOc;
        m_plates.push_back(plate);
    }

    std::shuffle(m_plates.begin(), m_plates.end(), rng);
}

// ---------------------------------------------------------------------------
// Stage 2 — Noise-warped Voronoi on plate grid

void World::buildPlateGrid()
{
    const int PW = width  / PLATE_SCALE;
    const int PH = height / PLATE_SCALE;
    m_plateGrid.assign(PW * PH, 0);

    FastNoiseLite noiseX, noiseY;
    noiseX.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noiseX.SetFrequency(0.2f);
    noiseX.SetSeed(static_cast<int>(seed) + 10);
    noiseY.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noiseY.SetFrequency(0.2f);
    noiseY.SetSeed(static_cast<int>(seed) + 11);

    const float WARP_AMP = 2.0f;
    const int   N = static_cast<int>(m_plates.size());

    for (int gy = 0; gy < PH; ++gy)
        for (int gx = 0; gx < PW; ++gx)
        {
            float fx = static_cast<float>(gx);
            float fy = static_cast<float>(gy);
            float wx = fx + noiseX.GetNoise(fx, fy) * WARP_AMP;
            float wy = fy + noiseY.GetNoise(fx, fy) * WARP_AMP;

            int   best  = 0;
            float bestD = std::numeric_limits<float>::max();
            for (int i = 0; i < N; ++i)
            {
                float dx = wx - m_plates[i].centerX;
                float dy = wy - m_plates[i].centerY;
                float d  = dx * dx + dy * dy;
                if (d < bestD) { bestD = d; best = i; }
            }
            m_plateGrid[gy * PW + gx] = best;
        }
}

// ---------------------------------------------------------------------------
// Stages 3 & 4 — Boundary classification + anisotropic influence map

namespace {
    enum class BoundaryType { None, Convergent, Divergent, Transform };
    struct CellBoundary {
        BoundaryType type     = BoundaryType::None;
        float        strength = 0.f;
        float        tangentX = 0.f;  // unit vector along the boundary
        float        tangentY = 0.f;
    };
}

void World::buildInfluenceMap(std::vector<float>& posInfluence, std::vector<float>& negInfluence)
{
    const int PW = width  / PLATE_SCALE;
    const int PH = height / PLATE_SCALE;

    // Stage 3: classify each boundary cell
    std::vector<CellBoundary> boundaries(PW * PH);
    const int ddx[] = { 1, -1, 0, 0 };
    const int ddy[] = { 0,  0, 1,-1 };
    const float THRESHOLD = 0.1f;

    for (int gy = 0; gy < PH; ++gy)
        for (int gx = 0; gx < PW; ++gx)
        {
            const int myId = m_plateGrid[gy * PW + gx];
            CellBoundary best;

            for (int d = 0; d < 4; ++d)
            {
                int nx = gx + ddx[d], ny = gy + ddy[d];
                if (nx < 0 || nx >= PW || ny < 0 || ny >= PH) continue;
                const int otherId = m_plateGrid[ny * PW + nx];
                if (otherId == myId) continue;

                const TectonicPlate& pa = m_plates[myId];
                const TectonicPlate& pb = m_plates[otherId];

                // relative velocity of pb relative to pa, projected onto boundary normal
                float relX = pb.driftX - pa.driftX;
                float relY = pb.driftY - pa.driftY;

                // Use the actual plate-center-to-center direction as the boundary normal,
                // not the grid-neighbor direction. This prevents axis-aligned ridges.
                float normX = pb.centerX - pa.centerX;
                float normY = pb.centerY - pa.centerY;
                float nlen  = std::sqrt(normX * normX + normY * normY);
                if (nlen > 0.f) { normX /= nlen; normY /= nlen; }

                float dot   = relX * normX + relY * normY;

                BoundaryType btype;
                float strength;

                if (dot > THRESHOLD)
                {
                    btype    = BoundaryType::Convergent;
                    strength = dot;
                    if (!pa.oceanic && !pb.oceanic) strength *= 1.4f;
                    else if (pa.oceanic && pb.oceanic) strength *= 0.5f;
                    else                               strength *= 0.9f;
                }
                else if (dot < -THRESHOLD)
                {
                    btype    = BoundaryType::Divergent;
                    strength = -dot;
                    if (pa.oceanic && pb.oceanic) strength *= 0.6f;  // wide mid-ocean ridge
                    else                          strength *= 0.4f;  // narrow rift valley
                }
                else
                {
                    btype    = BoundaryType::Transform;
                    strength = 0.05f;
                }

                if (strength > best.strength)
                {
                    best.type     = btype;
                    best.strength = strength;
                    // tangent runs along the boundary, perpendicular to the normal
                    best.tangentX = -normY;
                    best.tangentY =  normX;
                }
            }
            boundaries[gy * PW + gx] = best;
        }

    // Stage 4: anisotropic spread to tile resolution
    posInfluence.assign(width * height, 0.f);
    negInfluence.assign(width * height, 0.f);

    const int   R_GRID      = 5;
    const float S_CONV_PERP = 2.5f;  // convergent: narrow across ridge
    const float S_CONV_LONG = 22.f;  // convergent: long along ridge
    const float S_DIV_PERP  = 2.0f;
    const float S_DIV_LONG  = 16.f;

    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
        {
            const int gcx = tx / PLATE_SCALE;
            const int gcy = ty / PLATE_SCALE;
            float bestPos = 0.f, bestNeg = 0.f;

            for (int dgy = -R_GRID; dgy <= R_GRID; ++dgy)
                for (int dgx = -R_GRID; dgx <= R_GRID; ++dgx)
                {
                    int gx = gcx + dgx, gy = gcy + dgy;
                    if (gx < 0 || gx >= PW || gy < 0 || gy >= PH) continue;

                    const CellBoundary& b = boundaries[gy * PW + gx];
                    if (b.type == BoundaryType::None) continue;

                    float cx = gx * PLATE_SCALE + PLATE_SCALE * 0.5f;
                    float cy = gy * PLATE_SCALE + PLATE_SCALE * 0.5f;
                    float dx = static_cast<float>(tx) - cx;
                    float dy = static_cast<float>(ty) - cy;

                    // project offset onto tangent (along ridge) and perpendicular (across ridge)
                    float along = dx * b.tangentX + dy * b.tangentY;
                    float perp  = dx * (-b.tangentY) + dy * b.tangentX;

                    float w;
                    if (b.type == BoundaryType::Convergent)
                    {
                        w = b.strength
                            * std::exp(-along * along / (2.f * S_CONV_LONG * S_CONV_LONG))
                            * std::exp(-perp  * perp  / (2.f * S_CONV_PERP * S_CONV_PERP));
                        bestPos = std::max(bestPos, w);
                    }
                    else if (b.type == BoundaryType::Divergent)
                    {
                        w = b.strength
                            * std::exp(-along * along / (2.f * S_DIV_LONG * S_DIV_LONG))
                            * std::exp(-perp  * perp  / (2.f * S_DIV_PERP * S_DIV_PERP));
                        bestNeg = std::max(bestNeg, w);
                    }
                }
            posInfluence[ty * width + tx] = bestPos;
            negInfluence[ty * width + tx] = bestNeg;
        }
}

// ---------------------------------------------------------------------------
// Main generation pipeline

void World::initialize()
{
    const int PW = width  / PLATE_SCALE;
    const int PH = height / PLATE_SCALE;

    // Stages 1 & 2
    generatePlates();
    buildPlateGrid();

    // Stages 3 & 4
    std::vector<float> posInfluence, negInfluence;
    buildInfluenceMap(posInfluence, negInfluence);

    // Detail and moisture noise (continent noise removed)
    FastNoiseLite detailNoise;
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(5);
    detailNoise.SetFrequency(0.03f);
    detailNoise.SetSeed(static_cast<int>(seed) + 2);

    FastNoiseLite moistureNoise;
    moistureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    moistureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    moistureNoise.SetFractalOctaves(4);
    moistureNoise.SetFrequency(0.02f);
    moistureNoise.SetSeed(static_cast<int>(seed) + 3);

    // Stage 5 — elevation synthesis
    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
        {
            Tile& tile = tileAt(tx, ty);

            const int plateId = m_plateGrid[(ty / PLATE_SCALE) * PW + (tx / PLATE_SCALE)];
            tile.plateId = plateId;

            const TectonicPlate& plate = m_plates[plateId];
            float fx = static_cast<float>(tx);
            float fy = static_cast<float>(ty);

            float baseElev      = plate.oceanic ? 0.08f : 0.45f;
            float mountainBoost = posInfluence[ty * width + tx] * 0.35f;
            float riftDrop      = negInfluence[ty * width + tx] * 0.45f;
            float detail        = (detailNoise.GetNoise(fx, fy) + 1.f) * 0.5f;

            tile.elevation = std::clamp(baseElev + mountainBoost - riftDrop + detail * 0.28f, 0.f, 1.f);
            tile.moisture  = (moistureNoise.GetNoise(fx, fy) + 1.f) * 0.5f;
        }

    // Box blur on elevation (5×5)
    {
        std::vector<float> blurred(width * height);
        const int R = 2;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                float sum = 0.f; int count = 0;
                for (int dy = -R; dy <= R; ++dy)
                    for (int dx = -R; dx <= R; ++dx)
                    {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height)
                        { sum += tileAt(nx, ny).elevation; ++count; }
                    }
                blurred[y * width + x] = sum / count;
            }
        for (int i = 0; i < width * height; ++i)
            tiles[i].elevation = blurred[i];
    }

    applyErosion();

    // Sink landmasses smaller than threshold to remove tiny islands
    {
        const int MIN_LANDMASS = 300;
        const int ddx[] = { 1, -1, 0, 0 };
        const int ddy[] = { 0,  0, 1,-1 };
        std::vector<bool> visited(width * height, false);
        std::vector<int>  component;
        component.reserve(4096);

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                const int idx = y * width + x;
                if (visited[idx] || tiles[idx].elevation < 0.40f) { visited[idx] = true; continue; }

                component.clear();
                component.push_back(idx);
                visited[idx] = true;

                for (int qi = 0; qi < static_cast<int>(component.size()); ++qi)
                {
                    const int cur = component[qi];
                    const int cx = cur % width, cy = cur / width;
                    for (int d = 0; d < 4; ++d)
                    {
                        int nx = cx + ddx[d], ny = cy + ddy[d];
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                        const int nidx = ny * width + nx;
                        if (visited[nidx]) continue;
                        visited[nidx] = true;
                        if (tiles[nidx].elevation >= 0.40f)
                            component.push_back(nidx);
                    }
                }

                if (static_cast<int>(component.size()) < MIN_LANDMASS)
                    for (int i : component)
                        tiles[i].elevation = 0.30f;
            }
    }

    // Classify terrain
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            Tile& tile = tileAt(x, y);
            float latTemp    = 1.f - std::abs(2.f * y / (height - 1) - 1.f);
            tile.temperature = std::clamp(latTemp - tile.elevation * 0.10f, 0.f, 1.f);
            tile.terrain = classifyTerrain(tile.elevation, tile.temperature, tile.moisture);
            tile.color   = terrainColor(tile.terrain);

            // Mountain greyscale gradient: dark grey at e=0.82, white at e=1.0
            if (tile.terrain == TerrainType::Mountain ||
                tile.terrain == TerrainType::SnowCap  ||
                tile.terrain == TerrainType::BarePeak)
            {
                float t = std::clamp((tile.elevation - 0.82f) / 0.18f, 0.f, 1.f);
                auto  g = static_cast<uint8_t>(85 + t * 170);
                tile.color = sf::Color(g, g, g);
            }
        }

    bakeToTexture();
    computeStats();
}

// ---------------------------------------------------------------------------
// Erosion

void World::applyErosion()
{
    const float MAX_DIFF = 0.20f;
    const int   PASSES   = 4;
    const int   ddx[]    = { 1, -1, 0, 0 };
    const int   ddy[]    = { 0,  0, 1,-1 };

    std::vector<float> delta(width * height);

    for (int pass = 0; pass < PASSES; ++pass)
    {
        std::fill(delta.begin(), delta.end(), 0.f);

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                float e = tileAt(x, y).elevation;
                for (int d = 0; d < 4; ++d)
                {
                    int nx = x + ddx[d], ny = y + ddy[d];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                    float diff = e - tileAt(nx, ny).elevation;
                    if (diff > MAX_DIFF)
                    {
                        float transfer = (diff - MAX_DIFF) * 0.25f;
                        delta[y * width + x]   -= transfer;
                        delta[ny * width + nx] += transfer;
                    }
                }
            }

        for (int i = 0; i < width * height; ++i)
            tiles[i].elevation = std::clamp(tiles[i].elevation + delta[i], 0.f, 1.f);
    }
}

// ---------------------------------------------------------------------------
// Stats

void World::computeStats()
{
    const int total = width * height;
    const float invTotal = 100.f / total;

    int landCount     = 0;
    int mountainCount = 0;
    int tropicalCount = 0;
    int polarCount    = 0;

    for (const Tile& t : tiles)
    {
        if (t.elevation >= 0.40f) ++landCount;

        if (t.terrain == TerrainType::Mountain ||
            t.terrain == TerrainType::SnowCap  ||
            t.terrain == TerrainType::BarePeak)
            ++mountainCount;

        if (t.terrain == TerrainType::TropicalForest)
            ++tropicalCount;

        if (t.terrain == TerrainType::Tundra      ||
            t.terrain == TerrainType::IceShore     ||
            t.terrain == TerrainType::FrozenOcean  ||
            t.terrain == TerrainType::FrozenHills)
            ++polarCount;
    }

    // BFS connected-components for landmass count and largest landmass
    std::vector<bool> visited(total, false);
    int   landmassCount   = 0;
    int   largestSize     = 0;
    std::vector<int> queue;
    queue.reserve(4096);

    const int ddx[] = { 1, -1, 0, 0 };
    const int ddy[] = { 0,  0, 1,-1 };

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const int idx = y * width + x;
            if (visited[idx] || tiles[idx].elevation < 0.40f) continue;

            // BFS from (x, y)
            ++landmassCount;
            int size = 0;
            queue.clear();
            queue.push_back(idx);
            visited[idx] = true;

            for (int qi = 0; qi < static_cast<int>(queue.size()); ++qi)
            {
                const int cur = queue[qi];
                ++size;
                const int cx = cur % width, cy = cur / width;
                for (int d = 0; d < 4; ++d)
                {
                    int nx = cx + ddx[d], ny = cy + ddy[d];
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
                    const int nidx = ny * width + nx;
                    if (visited[nidx] || tiles[nidx].elevation < 0.40f) continue;
                    visited[nidx] = true;
                    queue.push_back(nidx);
                }
            }
            largestSize = std::max(largestSize, size);
        }

    m_stats.landPercent            = landCount     * invTotal;
    m_stats.oceanPercent           = (total - landCount) * invTotal;
    m_stats.landmassCount          = landmassCount;
    m_stats.largestLandmassPercent = largestSize   * invTotal;
    m_stats.mountainPercent        = mountainCount * invTotal;
    m_stats.tropicalPercent        = tropicalCount * invTotal;
    m_stats.polarPercent           = polarCount    * invTotal;
}

// ---------------------------------------------------------------------------
// Rendering

void World::setViewMode(ViewMode mode)
{
    m_viewMode = mode;
    bakeToTexture();
}

sf::Color World::elevationColor(float v)
{
    uint8_t c = static_cast<uint8_t>(std::clamp(v, 0.f, 1.f) * 255.f);
    return sf::Color(c, c, c);
}

sf::Color World::temperatureColor(float v)
{
    v = std::clamp(v, 0.f, 1.f);
    return sf::Color(
        static_cast<uint8_t>(v * 255.f),
        0,
        static_cast<uint8_t>((1.f - v) * 255.f));
}

sf::Color World::moistureColor(float v)
{
    v = std::clamp(v, 0.f, 1.f);
    return sf::Color(
        static_cast<uint8_t>((1.f - v) * 139.f),
        static_cast<uint8_t>(v * 160.f + (1.f - v) * 90.f),
        static_cast<uint8_t>(v * 128.f));
}

sf::Color World::plateColor(int id)
{
    static const sf::Color palette[] = {
        sf::Color(220,  60,  60),
        sf::Color( 60, 140, 220),
        sf::Color( 60, 200,  80),
        sf::Color(220, 180,  40),
        sf::Color(180,  60, 220),
        sf::Color( 40, 200, 200),
        sf::Color(220, 120,  40),
        sf::Color(140, 220,  60),
        sf::Color(220,  60, 160),
        sf::Color( 60,  80, 200),
        sf::Color(160, 100,  40),
        sf::Color(100, 200, 180),
    };
    return palette[id % 12];
}

void World::bakeToTexture()
{
    const int stride = tileSize + 1;

    m_worldTexture.resize({
        static_cast<unsigned>(width  * stride),
        static_cast<unsigned>(height * stride)
    });
    m_worldTexture.clear(sf::Color(20, 20, 20));

    sf::RectangleShape rect(sf::Vector2f(
        static_cast<float>(tileSize), static_cast<float>(tileSize)));

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const Tile& t = tileAt(x, y);
            sf::Color c;
            switch (m_viewMode)
            {
                case ViewMode::Elevation:    c = elevationColor(t.elevation);     break;
                case ViewMode::Temperature:  c = temperatureColor(t.temperature); break;
                case ViewMode::Moisture:     c = moistureColor(t.moisture);       break;
                case ViewMode::Plates:       c = plateColor(t.plateId);           break;
                default:                     c = t.color;                         break;
            }
            rect.setPosition(sf::Vector2f(
                static_cast<float>(x * stride),
                static_cast<float>(y * stride)));
            rect.setFillColor(c);
            m_worldTexture.draw(rect);
        }

    m_worldTexture.display();
    m_worldSprite.emplace(m_worldTexture.getTexture());
}

void World::draw(sf::RenderWindow& window)
{
    if (m_worldSprite)
        window.draw(*m_worldSprite);
}
