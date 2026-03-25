#include "World.h"
#include <random>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <fstream>

static constexpr int   NUM_REGULAR_PLATES = 10;
static constexpr float POLAR_CX           = 0.5f;
static constexpr float POLAR_RX           = 0.25f;  // half-width (normalized)
static constexpr float POLAR_RY           = 0.10f;  // half-height (normalized)
static constexpr float WARP_FREQ          = 1.8f;
static constexpr float WARP_AMP           = 0.08f;
static constexpr float AR                 = 2.0f;   // width/height aspect ratio

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

World::World(int width, int height, int tileSize)
    : m_width(width), m_height(height), m_tileSize(tileSize)
{
    m_tiles.resize(width * height);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void World::generate(unsigned int seed)
{
    m_seed = seed;
    m_stats = {};

    auto t0 = std::chrono::high_resolution_clock::now();

    m_warpX.SetSeed(static_cast<int>(seed));
    m_warpX.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_warpX.SetFrequency(WARP_FREQ);

    m_warpY.SetSeed(static_cast<int>(seed) + 1);
    m_warpY.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_warpY.SetFrequency(WARP_FREQ);

    m_strengthNoise.SetSeed(static_cast<int>(seed) + 2);
    m_strengthNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    m_strengthNoise.SetFrequency(0.005f);

    generatePlates();
    assignTilePlates();
    buildBoundaryChains();
    computeElevation();
    bakePlateTexture();
    bakeBoundaryOverlay();
    bakeHeightTexture();

    auto t1 = std::chrono::high_resolution_clock::now();
    m_stats.genTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    // Plate stats
    m_stats.plateCount = static_cast<int>(m_plates.size());
    for (const auto& p : m_plates)
    {
        if (p.oceanic) ++m_stats.plateOceanic;
        else           ++m_stats.plateContinental;
    }

    // Chain stats
    m_stats.chainTotal = static_cast<int>(m_chains.size());
    for (const auto& c : m_chains)
    {
        switch (c.type)
        {
            case BoundaryType::Convergent: ++m_stats.chainConvergent; break;
            case BoundaryType::Divergent:  ++m_stats.chainDivergent;  break;
            case BoundaryType::Transform:  ++m_stats.chainTransform;  break;
        }
    }
}

void World::draw(sf::RenderWindow& window)
{
    if (m_renderMode == RenderMode::Plates)
    {
        if (m_plateSprite)
            window.draw(*m_plateSprite);
    }
    else
    {
        if (m_heightSprite)
            window.draw(*m_heightSprite);
    }

    if (showBoundaries && m_boundarySprite)
        window.draw(*m_boundarySprite);
}

// ---------------------------------------------------------------------------
// Plate generation
// ---------------------------------------------------------------------------

void World::generatePlates()
{
    m_plates.clear();
    std::mt19937 rng(m_seed);
    std::uniform_real_distribution<float> distPos(0.05f, 0.95f);
    std::uniform_real_distribution<float> distAngle(0.f, 6.2832f);
    std::uniform_real_distribution<float> distSpeed(0.3f, 1.0f);
    std::bernoulli_distribution           distOceanic(0.45);

    // Regular plates
    for (int i = 0; i < NUM_REGULAR_PLATES; ++i)
    {
        float angle = distAngle(rng);
        Plate p;
        p.cx      = distPos(rng);
        p.cy      = distPos(rng);
        p.dx      = std::cos(angle);
        p.dy      = std::sin(angle);
        p.speed   = distSpeed(rng);
        p.oceanic = distOceanic(rng);
        p.isPolar = false;
        m_plates.push_back(p);
    }

    // North polar cap — oceanic, no drift
    {
        Plate p;
        p.cx = POLAR_CX; p.cy = 0.f;
        p.dx = 0.f; p.dy = 0.f; p.speed = 0.f;
        p.oceanic = true; p.isPolar = true;
        m_plates.push_back(p);
    }

    // South polar cap — continental, no drift
    {
        Plate p;
        p.cx = POLAR_CX; p.cy = 1.f;
        p.dx = 0.f; p.dy = 0.f; p.speed = 0.f;
        p.oceanic = false; p.isPolar = true;
        m_plates.push_back(p);
    }
}

// ---------------------------------------------------------------------------
// Voronoi plate query (domain-warped, AR-corrected)
// ---------------------------------------------------------------------------

int World::nearestPlate(float nx, float ny) const
{
    // Domain warp
    float wx = nx + WARP_AMP * m_warpX.GetNoise(nx, ny);
    float wy = ny + WARP_AMP * m_warpY.GetNoise(nx, ny);

    // Polar caps override by elliptic membership
    const int northIdx = NUM_REGULAR_PLATES;
    const int southIdx = NUM_REGULAR_PLATES + 1;

    auto inPolar = [&](float cy) {
        float ex = (wx - POLAR_CX) / POLAR_RX;
        float ey = (wy - cy)       / POLAR_RY;
        return (ex * ex + ey * ey) <= 1.f;
    };

    if (inPolar(0.f)) return northIdx;
    if (inPolar(1.f)) return southIdx;

    // Nearest regular plate (AR-corrected distance)
    int   best = 0;
    float bestD = 1e9f;
    for (int i = 0; i < NUM_REGULAR_PLATES; ++i)
    {
        float dx = (wx - m_plates[i].cx) * AR;
        float dy = (wy - m_plates[i].cy);
        float d  = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void World::assignTilePlates()
{
    for (int y = 0; y < m_height; ++y)
    for (int x = 0; x < m_width;  ++x)
    {
        float nx = static_cast<float>(x) / static_cast<float>(m_width);
        float ny = static_cast<float>(y) / static_cast<float>(m_height);
        tileAt(x, y).plateId = nearestPlate(nx, ny);
    }
}

// ---------------------------------------------------------------------------
// Boundary chain building
// ---------------------------------------------------------------------------

void World::buildBoundaryChains()
{
    m_chains.clear();

    // Key: (minId, maxId) → chain index
    std::unordered_map<uint64_t, int> chainMap;

    auto edgeKey = [](int a, int b) -> uint64_t {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(a) << 32) | static_cast<uint32_t>(b);
    };

    const int stride = m_tileSize + 1;

    // Walk horizontal edges (between row y and y+1)
    for (int y = 0; y < m_height - 1; ++y)
    for (int x = 0; x < m_width;      ++x)
    {
        int idA = tileAt(x, y).plateId;
        int idB = tileAt(x, y + 1).plateId;
        if (idA == idB) continue;

        uint64_t key = edgeKey(idA, idB);
        if (chainMap.find(key) == chainMap.end())
        {
            chainMap[key] = static_cast<int>(m_chains.size());
            m_chains.push_back({});
            m_chains.back().plateA = std::min(idA, idB);
            m_chains.back().plateB = std::max(idA, idB);
        }

        BoundaryEdge e;
        e.pos = sf::Vector2f(
            static_cast<float>(x * stride + m_tileSize / 2),
            static_cast<float>(y * stride + m_tileSize));
        e.tangent = sf::Vector2f(1.f, 0.f);
        e.normal  = sf::Vector2f(0.f, 1.f);
        m_chains[chainMap[key]].edges.push_back(e);
    }

    // Walk vertical edges (between column x and x+1)
    for (int y = 0; y < m_height;     ++y)
    for (int x = 0; x < m_width - 1;  ++x)
    {
        int idA = tileAt(x,     y).plateId;
        int idB = tileAt(x + 1, y).plateId;
        if (idA == idB) continue;

        uint64_t key = edgeKey(idA, idB);
        if (chainMap.find(key) == chainMap.end())
        {
            chainMap[key] = static_cast<int>(m_chains.size());
            m_chains.push_back({});
            m_chains.back().plateA = std::min(idA, idB);
            m_chains.back().plateB = std::max(idA, idB);
        }

        BoundaryEdge e;
        e.pos = sf::Vector2f(
            static_cast<float>(x * stride + m_tileSize),
            static_cast<float>(y * stride + m_tileSize / 2));
        e.tangent = sf::Vector2f(0.f, 1.f);
        e.normal  = sf::Vector2f(1.f, 0.f);
        m_chains[chainMap[key]].edges.push_back(e);
    }

    // Classify each chain
    for (auto& chain : m_chains)
    {
        const Plate& pA = m_plates[chain.plateA];
        const Plate& pB = m_plates[chain.plateB];

        // Relative velocity of B with respect to A
        float rvx = pB.dx * pB.speed - pA.dx * pA.speed;
        float rvy = pB.dy * pB.speed - pA.dy * pA.speed;

        // Average normal across the chain (first edge's normal is representative)
        float nx = chain.edges.empty() ? 1.f : chain.edges[0].normal.x;
        float ny = chain.edges.empty() ? 0.f : chain.edges[0].normal.y;

        chain.compression = rvx * nx + rvy * ny;

        if      (chain.compression >  0.15f) chain.type = BoundaryType::Convergent;
        else if (chain.compression < -0.15f) chain.type = BoundaryType::Divergent;
        else                                 chain.type = BoundaryType::Transform;
    }
}

// ---------------------------------------------------------------------------
// Bake plate texture
// ---------------------------------------------------------------------------

void World::bakePlateTexture()
{
    const int stride = m_tileSize + 1;
    const int imgW   = m_width  * stride;
    const int imgH   = m_height * stride;

    sf::Image img(sf::Vector2u(imgW, imgH), sf::Color(15, 15, 15));

    // Tile plate colors
    for (int y = 0; y < m_height; ++y)
    for (int x = 0; x < m_width;  ++x)
    {
        sf::Color col = plateColor(tileAt(x, y).plateId);
        for (int dy = 0; dy < m_tileSize; ++dy)
        for (int dx = 0; dx < m_tileSize; ++dx)
            img.setPixel(sf::Vector2u(x * stride + dx, y * stride + dy), col);
    }

    m_plateTexture.loadFromImage(img);
    m_plateSprite.emplace(m_plateTexture);
}

// ---------------------------------------------------------------------------
// Boundary overlay
// ---------------------------------------------------------------------------

void World::bakeBoundaryOverlay()
{
    const int stride = m_tileSize + 1;
    const int imgW   = m_width  * stride;
    const int imgH   = m_height * stride;

    // Fully transparent background
    sf::Image img(sf::Vector2u(imgW, imgH), sf::Color(0, 0, 0, 0));

    for (const auto& chain : m_chains)
    {
        sf::Color col = boundaryColor(chain.type);

        for (const auto& e : chain.edges)
        {
            const int cx = static_cast<int>(e.pos.x);
            const int cy = static_cast<int>(e.pos.y);
            const bool horizontal = (e.tangent.x != 0.f);

            if (horizontal)
            {
                // Band spans full tile width, 3px tall
                for (int dx = -(m_tileSize / 2); dx <= m_tileSize / 2; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                {
                    int px = cx + dx, py = cy + dy;
                    if (px >= 0 && px < imgW && py >= 0 && py < imgH)
                        img.setPixel(sf::Vector2u(px, py), col);
                }
            }
            else
            {
                // Band spans full tile height, 3px wide
                for (int dy = -(m_tileSize / 2); dy <= m_tileSize / 2; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    int px = cx + dx, py = cy + dy;
                    if (px >= 0 && px < imgW && py >= 0 && py < imgH)
                        img.setPixel(sf::Vector2u(px, py), col);
                }
            }
        }
    }

    m_boundaryTexture.loadFromImage(img);
    m_boundarySprite.emplace(m_boundaryTexture);
}

// ---------------------------------------------------------------------------
// Phase 1 — Tectonic elevation
// ---------------------------------------------------------------------------

static constexpr float SIGMA_PERP  = 3.5f;
static constexpr float SIGMA_ALONG = 12.0f;
static constexpr int   RAD_PERP    = 11;
static constexpr int   RAD_ALONG   = 30;

void World::computeElevation()
{
    // Reset elevations
    for (auto& t : m_tiles) t.elevation = 0.f;

    // Precompute 1D bell lookup tables
    float bell_perp [RAD_PERP  + 1];
    float bell_along[RAD_ALONG + 1];
    for (int i = 0; i <= RAD_PERP;  ++i)
        bell_perp [i] = std::exp(-(float)(i * i) / (SIGMA_PERP  * SIGMA_PERP));
    for (int i = 0; i <= RAD_ALONG; ++i)
        bell_along[i] = std::exp(-(float)(i * i) / (SIGMA_ALONG * SIGMA_ALONG));

    const float stride = static_cast<float>(m_tileSize + 1);

    for (const auto& chain : m_chains)
    {
        const float compression = chain.compression;
        if (std::abs(compression) < 1e-4f) continue;

        for (const auto& e : chain.edges)
        {
            // Edge centre in tile-space
            const float etx = e.pos.x / stride;
            const float ety = e.pos.y / stride;

            // Per-edge strength variation (low-freq noise)
            const float noise = m_strengthNoise.GetNoise(e.pos.x, e.pos.y);
            const float strength = compression * (1.f + noise * 0.35f);

            const bool horizontal = (e.tangent.x != 0.f);
            const int xlo = static_cast<int>(etx) - (horizontal ? RAD_ALONG : RAD_PERP);
            const int xhi = static_cast<int>(etx) + (horizontal ? RAD_ALONG : RAD_PERP);
            const int ylo = static_cast<int>(ety) - (horizontal ? RAD_PERP  : RAD_ALONG);
            const int yhi = static_cast<int>(ety) + (horizontal ? RAD_PERP  : RAD_ALONG);

            for (int ty = std::max(0, ylo); ty <= std::min(m_height - 1, yhi); ++ty)
            for (int tx = std::max(0, xlo); tx <= std::min(m_width  - 1, xhi); ++tx)
            {
                const float dx = static_cast<float>(tx) - etx;
                const float dy = static_cast<float>(ty) - ety;

                const float d_perp  = dx * e.normal.x  + dy * e.normal.y;
                const float d_along = dx * e.tangent.x + dy * e.tangent.y;

                const int ip = std::min(static_cast<int>(std::abs(d_perp)),  RAD_PERP);
                const int ia = std::min(static_cast<int>(std::abs(d_along)), RAD_ALONG);

                const float bell = bell_perp[ip] * bell_along[ia];
                tileAt(tx, ty).elevation += bell * strength;
            }
        }
    }

    // Normalize [min, max] → [0, 1]
    float emin = m_tiles[0].elevation, emax = m_tiles[0].elevation;
    for (const auto& t : m_tiles)
    {
        if (t.elevation < emin) emin = t.elevation;
        if (t.elevation > emax) emax = t.elevation;
    }
    const float range = emax - emin;
    if (range > 1e-6f)
        for (auto& t : m_tiles)
            t.elevation = (t.elevation - emin) / range;
}

void World::bakeHeightTexture()
{
    const int stride = m_tileSize + 1;
    const int imgW   = m_width  * stride;
    const int imgH   = m_height * stride;

    sf::Image img(sf::Vector2u(imgW, imgH), sf::Color(15, 15, 15));

    for (int y = 0; y < m_height; ++y)
    for (int x = 0; x < m_width;  ++x)
    {
        const float e = tileAt(x, y).elevation;
        const uint8_t v = static_cast<uint8_t>(e * 255.f);
        const sf::Color col(v, v, v);
        for (int dy = 0; dy < m_tileSize; ++dy)
        for (int dx = 0; dx < m_tileSize; ++dx)
            img.setPixel(sf::Vector2u(x * stride + dx, y * stride + dy), col);
    }

    m_heightTexture.loadFromImage(img);
    m_heightSprite.emplace(m_heightTexture);
}

// ---------------------------------------------------------------------------
// CSV export
// ---------------------------------------------------------------------------

void World::exportCSV(const std::string& path) const
{
    std::ofstream f(path);
    f << "x,y,plateId,oceanic,isPolar,elevation\n";
    for (int y = 0; y < m_height; ++y)
    for (int x = 0; x < m_width;  ++x)
    {
        const Tile&  t = tileAt(x, y);
        const Plate& p = m_plates[t.plateId];
        f << x << ',' << y << ','
          << t.plateId << ','
          << (p.oceanic ? 1 : 0) << ','
          << (p.isPolar ? 1 : 0) << ','
          << t.elevation << '\n';
    }
}

// ---------------------------------------------------------------------------
// Hover info
// ---------------------------------------------------------------------------

World::HoverInfo World::getHoverInfo(sf::Vector2f worldPos) const
{
    const int stride = m_tileSize + 1;
    const int tx     = static_cast<int>(worldPos.x) / stride;
    const int ty     = static_cast<int>(worldPos.y) / stride;

    if (tx < 0 || tx >= m_width || ty < 0 || ty >= m_height)
        return {};

    const Tile&  tile = tileAt(tx, ty);
    const Plate& p    = m_plates[tile.plateId];

    HoverInfo info;
    info.valid     = true;
    info.tileX     = tx;
    info.tileY     = ty;
    info.plateId   = tile.plateId;
    info.oceanic   = p.oceanic;
    info.isPolar   = p.isPolar;
    info.elevation = tile.elevation;
    return info;
}

// ---------------------------------------------------------------------------
// Color helpers
// ---------------------------------------------------------------------------

sf::Color World::plateColor(int id)
{
    static const sf::Color palette[] = {
        sf::Color( 80, 120, 200),  // 0  oceanic blue
        sf::Color(180, 140,  80),  // 1  continental tan
        sf::Color( 60, 160,  90),  // 2  green
        sf::Color(200,  80,  80),  // 3  red
        sf::Color(160,  80, 200),  // 4  purple
        sf::Color(200, 160,  60),  // 5  gold
        sf::Color( 80, 200, 200),  // 6  cyan
        sf::Color(200, 120,  60),  // 7  orange
        sf::Color(120, 200,  80),  // 8  lime
        sf::Color(200,  60, 140),  // 9  pink
        sf::Color(100, 140, 220),  // 10 north polar (oceanic)
        sf::Color(220, 220, 240),  // 11 south polar (continental)
    };
    const int n = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    return palette[id % n];
}

sf::Color World::boundaryColor(BoundaryType type)
{
    switch (type)
    {
        case BoundaryType::Convergent: return sf::Color(255, 100,  30);  // orange
        case BoundaryType::Divergent:  return sf::Color( 50, 220, 255);  // cyan
        case BoundaryType::Transform:  return sf::Color(220, 255,  50);  // yellow-green
    }
    return sf::Color::White;
}
