#include "World.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <random>
#include <unordered_map>

World::World(int width, int height, int tileSize, unsigned int seed)
    : width(width), height(height), tileSize(tileSize), seed(seed)
{
    tiles.resize(width * height);
    m_font.openFromFile("C:/Windows/Fonts/arial.ttf");
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
// Noise initialisation (called from generatePlates)

void World::initNoise()
{
    // Domain warp — low-medium frequency for organic plate shapes
    m_warpX.SetSeed(static_cast<int>(seed) + 10);
    m_warpX.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_warpX.SetFrequency(4.f);    // ~4 large-scale bends across [0,1]

    m_warpY.SetSeed(static_cast<int>(seed) + 11);
    m_warpY.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_warpY.SetFrequency(4.f);

    // Fine-grained noise used to jitter cells near a Voronoi edge
    m_jitterNoise.SetSeed(static_cast<int>(seed) + 12);
    m_jitterNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_jitterNoise.SetFrequency(30.f);

    // Medium-frequency FBm noise for terrain detail within plates
    m_detailNoise.SetSeed(static_cast<int>(seed) + 13);
    m_detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    m_detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_detailNoise.SetFractalOctaves(5);
    m_detailNoise.SetFractalLacunarity(2.0f);
    m_detailNoise.SetFractalGain(0.5f);
    m_detailNoise.SetFrequency(5.f);
}

// ---------------------------------------------------------------------------
// Stage 1 — Blue-noise plate placement in normalised [0,1]×[0,1] space

void World::generatePlates()
{
    m_plates.clear();
    initNoise();

    // --- Polar cap plates (always indices 0 and 1, never shuffled) ---
    auto makePolar = [](float cy, bool oceanic) {
        TectonicPlate p;
        p.centerX = 0.5f; p.centerY = cy;
        p.driftX  = 0.0f; p.driftY  = 0.0f;
        p.speed   = 0.0f;
        p.oceanic = oceanic;
        p.isPolar = true;
        return p;
    };
    m_plates.push_back(makePolar(0.0f, true));   // north — oceanic (Arctic)
    m_plates.push_back(makePolar(1.0f, false));  // south — continental (Antarctica)

    // --- Regular tectonic plates ---
    const int   N        = 12;
    const int   nOc      = 8;   // oceanic count among regular plates
    const float AR       = static_cast<float>(width) / static_cast<float>(height);
    const float minDist  = 0.18f;

    // Keep regular plates out of the polar zones + a small margin
    const float minY = POLAR_RY + 0.05f;
    const float maxY = 1.0f - POLAR_RY - 0.05f;

    std::mt19937 rng(seed + 9999);
    std::uniform_real_distribution<float> distX(0.05f, 0.95f);
    std::uniform_real_distribution<float> distY(minY, maxY);
    std::uniform_real_distribution<float> distAngle(0.f, 6.28318f);
    std::uniform_real_distribution<float> distSpeed(0.3f, 1.0f);

    int regularCount = 0;
    int attempts     = 0;
    while (regularCount < N && attempts < 10000)
    {
        ++attempts;
        float cx = distX(rng);
        float cy = distY(rng);

        bool tooClose = false;
        for (const auto& p : m_plates)
        {
            if (p.isPolar) continue;  // polar plates don't block regular placement
            float adx = std::abs(cx - p.centerX);
            float dx  = std::min(adx, 1.0f - adx) * AR;
            float dy  = cy - p.centerY;
            if (dx*dx + dy*dy < minDist*minDist) { tooClose = true; break; }
        }
        if (tooClose) continue;

        float angle = distAngle(rng);
        TectonicPlate plate;
        plate.centerX = cx;
        plate.centerY = cy;
        plate.driftX  = std::cos(angle);
        plate.driftY  = std::sin(angle);
        plate.speed   = distSpeed(rng);
        plate.oceanic = regularCount < nOc;
        plate.isPolar = false;
        m_plates.push_back(plate);
        ++regularCount;
    }

    // Fallback: fill remaining regular plates without distance constraint
    while (regularCount < N)
    {
        float angle = distAngle(rng);
        TectonicPlate plate;
        plate.centerX = distX(rng);
        plate.centerY = distY(rng);
        plate.driftX  = std::cos(angle);
        plate.driftY  = std::sin(angle);
        plate.speed   = distSpeed(rng);
        plate.oceanic = regularCount < nOc;
        plate.isPolar = false;
        m_plates.push_back(plate);
        ++regularCount;
    }

    // Shuffle only regular plates — polar plates must stay at indices 0 and 1
    std::shuffle(m_plates.begin() + 2, m_plates.end(), rng);
}

// ---------------------------------------------------------------------------
// Continuous-space Voronoi query — returns nearest and second-nearest plate

World::PlateQuery World::queryPlate(float nx, float ny) const
{
    const float AR       = static_cast<float>(width) / static_cast<float>(height);
    const float WARP_AMP = 0.03f;

    // Domain warp (applied to regular Voronoi only — polar caps use clean geometry)
    float wx = nx + m_warpX.GetNoise(nx, ny) * WARP_AMP;
    float wy = ny + m_warpY.GetNoise(nx, ny) * WARP_AMP;

    // --- Polar cap override (unwarped nx/ny for a clean ellipse boundary) ---
    float adxp = std::min(std::abs(nx - 0.5f), 1.0f - std::abs(nx - 0.5f));

    auto findNearestRegular = [&]() -> std::pair<int, float> {
        int   best  = -1;
        float bestD = std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(m_plates.size()); ++i)
        {
            if (m_plates[i].isPolar) continue;
            float adx = std::abs(wx - m_plates[i].centerX);
            float dx  = std::min(adx, 1.0f - adx) * AR;
            float dy  = wy - m_plates[i].centerY;
            float d   = std::sqrt(dx*dx + dy*dy);
            if (d < bestD) { bestD = d; best = i; }
        }
        return { best, bestD };
    };

    {   // North polar ellipse
        float ex = adxp / POLAR_RX, ey = ny / POLAR_RY;
        if (ex*ex + ey*ey < 1.0f)
        {
            auto [id2, d2] = findNearestRegular();
            return { 0, id2, 0.0f, d2 };
        }
    }
    {   // South polar ellipse
        float ex = adxp / POLAR_RX, ey = (1.0f - ny) / POLAR_RY;
        if (ex*ex + ey*ey < 1.0f)
        {
            auto [id2, d2] = findNearestRegular();
            return { 1, id2, 0.0f, d2 };
        }
    }

    // --- Regular Voronoi (polar plates excluded — they only win inside their ellipses) ---
    int   id1 = 2, id2 = 3;
    float d1  = std::numeric_limits<float>::max();
    float d2  = std::numeric_limits<float>::max();

    const int N = static_cast<int>(m_plates.size());
    for (int i = 0; i < N; ++i)
    {
        if (m_plates[i].isPolar) continue;
        float adx = std::abs(wx - m_plates[i].centerX);
        float dx  = std::min(adx, 1.0f - adx) * AR;
        float dy  = wy - m_plates[i].centerY;
        float d   = dx*dx + dy*dy;
        if (d < d1) { d2 = d1; id2 = id1; d1 = d; id1 = i; }
        else if (d < d2) { d2 = d; id2 = i; }
    }

    return { id1, id2, std::sqrt(d1), std::sqrt(d2) };
}

// ---------------------------------------------------------------------------
// plateAt — Voronoi with edge jitter for organic boundaries

int World::plateAt(float nx, float ny) const
{
    const float JITTER_THRESH = 0.04f;   // normalised distance within boundary zone

    PlateQuery q = queryPlate(nx, ny);
    if (q.d2 - q.d1 < JITTER_THRESH)
    {
        // Randomly assign to nearest or second-nearest for ragged edges
        float j = m_jitterNoise.GetNoise(nx, ny);   // returns [-1, 1]
        return (j > 0.f) ? q.id1 : q.id2;
    }
    return q.id1;
}

// ---------------------------------------------------------------------------
// Boundary classification — same relative-velocity logic as before

World::BoundaryType World::boundaryBetween(int idA, int idB) const
{
    const TectonicPlate& pa = m_plates[idA];
    const TectonicPlate& pb = m_plates[idB];

    float relX = pb.driftX * pb.speed - pa.driftX * pa.speed;
    float relY = pb.driftY * pb.speed - pa.driftY * pa.speed;

    float normX = pb.centerX - pa.centerX;
    if (normX >  0.5f) normX -= 1.0f;   // shortest path across wrap
    if (normX < -0.5f) normX += 1.0f;
    float normY = pb.centerY - pa.centerY;
    float nlen  = std::sqrt(normX*normX + normY*normY);
    if (nlen > 0.f) { normX /= nlen; normY /= nlen; }

    float dot   = relX * normX + relY * normY;
    float slide = std::abs(relX * normY - relY * normX);

    // If the perpendicular signal is strong enough, respect it unconditionally.
    // Samples consistently show: |dot| >= 0.30 → the sign of dot always wins.
    const float ABS_THRESH = 0.30f;
    if (std::abs(dot) >= ABS_THRESH)
        return dot < 0.f ? BoundaryType::Convergent : BoundaryType::Divergent;

    // Weak perpendicular signal — let slide ratio decide.
    // Convergent tendencies are guarded with a high k (7.0) so even a large slide
    // ratio doesn't override a real convergent signal easily.
    // Divergent tendencies use a lower k (1.5) — more readily become Transform.
    const float k = (dot < 0.f) ? 7.0f : 1.5f;
    if (slide > k * std::abs(dot)) return BoundaryType::Transform;
    return dot < 0.f ? BoundaryType::Convergent : BoundaryType::Divergent;
}

// ---------------------------------------------------------------------------
// Stage 2 — Populate tile.plateId from continuous plateAt()
//           (keeps getTile() returning meaningful data for UI clicks)

void World::assignTilePlates()
{
    const float fw = static_cast<float>(width);
    const float fh = static_cast<float>(height);
    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
            tileAt(tx, ty).plateId = plateAt(
                (static_cast<float>(tx) + 0.5f) / fw,
                (static_cast<float>(ty) + 0.5f) / fh);
}

// ---------------------------------------------------------------------------
// Build and cache boundary chains — fully grid-independent

void World::buildBoundaryChains()
{
    const int   W      = width;
    const int   H      = height;
    const float stride = static_cast<float>(tileSize + 1);

    // Precompute nearest-plate id at each sample point (no jitter → clean edges)
    std::vector<int> ids(W * H);
    const float fw = static_cast<float>(W);
    const float fh = static_cast<float>(H);
    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx)
            ids[ty * W + tx] = queryPlate(
                (static_cast<float>(tx) + 0.5f) / fw,
                (static_cast<float>(ty) + 0.5f) / fh).id1;

    // Collect boundary edges
    // Vertex (vx, vy) encoded as vx + vy*(W+1)
    // plateA < plateB always (canonical ordering so both sides produce the same key)
    struct Edge { int v1, v2; BoundaryType type; int plateA, plateB; };
    std::vector<Edge> edges;
    edges.reserve(W * H / 4);

    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx)
        {
            int id = ids[ty * W + tx];

            // Right neighbour → vertical edge segment
            if (tx + 1 < W)
            {
                int idR = ids[ty * W + tx + 1];
                if (idR != id)
                    edges.push_back({
                        (tx + 1) +  ty      * (W + 1),
                        (tx + 1) + (ty + 1) * (W + 1),
                        boundaryBetween(id, idR),
                        std::min(id, idR), std::max(id, idR)
                    });
            }
            // Bottom neighbour → horizontal edge segment
            if (ty + 1 < H)
            {
                int idB = ids[(ty + 1) * W + tx];
                if (idB != id)
                    edges.push_back({
                         tx      + (ty + 1) * (W + 1),
                        (tx + 1) + (ty + 1) * (W + 1),
                        boundaryBetween(id, idB),
                        std::min(id, idB), std::max(id, idB)
                    });
            }
        }

    // Build vertex adjacency map
    std::unordered_map<int, std::vector<int>> vertexEdges;
    vertexEdges.reserve(edges.size() * 2);
    for (int i = 0; i < static_cast<int>(edges.size()); ++i)
    {
        vertexEdges[edges[i].v1].push_back(i);
        vertexEdges[edges[i].v2].push_back(i);
    }

    // Vertex → world-pixel position
    auto vertexPos = [&](int v) -> sf::Vector2f {
        return sf::Vector2f(
            static_cast<float>(v % (W + 1)) * stride,
            static_cast<float>(v / (W + 1)) * stride);
    };

    // Catmull-Rom interpolation
    auto catmullRom = [](sf::Vector2f p0, sf::Vector2f p1,
                         sf::Vector2f p2, sf::Vector2f p3, float t) -> sf::Vector2f
    {
        float t2 = t*t, t3 = t2*t;
        return 0.5f * sf::Vector2f(
            (-t3 + 2*t2 - t)*p0.x + (3*t3 - 5*t2 + 2)*p1.x +
            (-3*t3 + 4*t2 + t)*p2.x + (t3 - t2)*p3.x,
            (-t3 + 2*t2 - t)*p0.y + (3*t3 - 5*t2 + 2)*p1.y +
            (-3*t3 + 4*t2 + t)*p2.y + (t3 - t2)*p3.y);
    };

    // Trace chains
    std::vector<bool> visited(edges.size(), false);
    m_boundaryChains.clear();
    const int SUBDIVISIONS = 6;

    for (int startEdge = 0; startEdge < static_cast<int>(edges.size()); ++startEdge)
    {
        if (visited[startEdge]) continue;

        visited[startEdge] = true;

        const int seedPlateA = edges[startEdge].plateA;
        const int seedPlateB = edges[startEdge].plateB;

        // Use a deque so we can grow in both directions from the seed edge.
        std::deque<int> dq;
        dq.push_back(edges[startEdge].v1);
        dq.push_back(edges[startEdge].v2);

        auto walkFrom = [&](bool fromBack)
        {
            bool extended = true;
            while (extended)
            {
                extended = false;
                int v = fromBack ? dq.back() : dq.front();
                for (int ei : vertexEdges[v])
                {
                    if (visited[ei]) continue;
                    if (edges[ei].plateA != seedPlateA || edges[ei].plateB != seedPlateB) continue;
                    visited[ei] = true;
                    extended = true;
                    int next = (edges[ei].v1 == v) ? edges[ei].v2 : edges[ei].v1;
                    if (fromBack) dq.push_back(next);
                    else          dq.push_front(next);
                    break;
                }
            }
        };

        walkFrom(true);   // extend tail forward
        walkFrom(false);  // extend head backward

        if (dq.size() < 2) continue;

        const int M = static_cast<int>(dq.size());
        std::vector<sf::Vector2f> pts(M);
        for (int i = 0; i < M; ++i) pts[i] = vertexPos(dq[i]);

        sf::Vector2f ph0 = pts[0]     - (pts[1]     - pts[0]);
        sf::Vector2f phN = pts[M - 1] + (pts[M - 1] - pts[M - 2]);

        auto getP = [&](int i) -> sf::Vector2f {
            if (i < 0)  return ph0;
            if (i >= M) return phN;
            return pts[i];
        };

        BoundaryChain chain;
        chain.type = edges[startEdge].type;

        for (int i = 0; i < M - 1; ++i)
        {
            sf::Vector2f p0 = getP(i - 1), p1 = getP(i),
                         p2 = getP(i + 1), p3 = getP(i + 2);
            for (int s = 0; s <= SUBDIVISIONS; ++s)
            {
                float t = static_cast<float>(s) / SUBDIVISIONS;
                chain.points.push_back(catmullRom(p0, p1, p2, p3, t));
            }
        }
        m_boundaryChains.push_back(std::move(chain));
    }
}

// ---------------------------------------------------------------------------
// Main generation pipeline

void World::initialize()
{
    m_selectedChainIdx = -1;
    generatePlates();        // also calls initNoise()
    assignTilePlates();      // populate tile.plateId for UI clicks
    buildBoundaryChains();   // chain tracing — needed before applyBoundaryNoise
    computeElevation();      // assign elevation from plate type + boundary proximity
    smoothElevation();       // box-blur to soften hard plateau steps at junction endpoints
    applyBoundaryNoise();    // scatter circular bumps along convergent/divergent chains
    applyErosion();          // thermal erosion — softens steep slopes and bump peaks
    bakeHeightMapTexture();  // bake grayscale elevation into a texture for fast rendering
    bakeTerrainTexture();    // bake terrain colours into a texture for fast rendering
    bakeToPlateTexture();    // per-pixel plateAt() evaluation
}

void World::setSelectedChain(int idx)
{
    m_selectedChainIdx = idx;
}

void World::setRenderMode(RenderMode mode) { m_renderMode = mode; }
World::RenderMode World::getRenderMode() const { return m_renderMode; }

// ---------------------------------------------------------------------------
// Stage 3 — Assign elevation per tile using a Gaussian splat from each
// boundary edge.  SIGMA controls ridge/trench half-width in tiles.
//
// For every boundary edge in the plate-id grid, the bell contribution is
// accumulated into all tiles within 3·SIGMA.  Only tiles whose plate ID
// matches one of the edge's two plate IDs receive the contribution, which
// prevents phantom ridges from leaking into unrelated plate territory.
// At triple junctions multiple edges overlap and their weighted average
// produces a smooth gradient blend rather than a hard seam.
//
// Bell lookup tables (bellR / bellB) are precomputed once per call so
// that std::exp() is not called inside the hot loop.

void World::computeElevation()
{
    const float SIGMA  = 4.0f;              // ridge half-width in tiles
    const float SIGMA2 = SIGMA * SIGMA;
    const int   RADIUS = static_cast<int>(3.0f * SIGMA) + 1;  // = 13
    const int   TS     = 2 * RADIUS + 1;   // lookup table side length

    const float fw = static_cast<float>(width);
    const float fh = static_cast<float>(height);

    // Plate-id grid (no jitter — same source as buildBoundaryChains).
    std::vector<int> ids(width * height);
    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
            ids[ty * width + tx] = queryPlate(
                (static_cast<float>(tx) + 0.5f) / fw,
                (static_cast<float>(ty) + 0.5f) / fh).id1;

    // Precompute bell values for the two edge orientations.
    // Right edge midpoint at (tx+1, ty+0.5): tile offset dx=dsx+0.5, dy=dsy.
    // Bottom edge midpoint at (tx+0.5, ty+1): tile offset dx=dsx, dy=dsy+0.5.
    const float RADIUS2 = static_cast<float>(RADIUS * RADIUS);
    std::vector<float> bellR(TS * TS, 0.f);
    std::vector<float> bellB(TS * TS, 0.f);
    for (int dsy = -RADIUS; dsy <= RADIUS; ++dsy)
        for (int dsx = -RADIUS; dsx <= RADIUS; ++dsx)
        {
            int   i   = (dsy + RADIUS) * TS + (dsx + RADIUS);
            float fR  = static_cast<float>(dsx) + 0.5f;
            float gR  = static_cast<float>(dsy);
            float fB  = static_cast<float>(dsx);
            float gB  = static_cast<float>(dsy) + 0.5f;
            float dR2 = fR*fR + gR*gR;
            float dB2 = fB*fB + gB*gB;
            if (dR2 < RADIUS2) { float v = std::exp(-dR2 / SIGMA2); if (v > 0.001f) bellR[i] = v; }
            if (dB2 < RADIUS2) { float v = std::exp(-dB2 / SIGMA2); if (v > 0.001f) bellB[i] = v; }
        }

    // Modifier: boundary type × plate oceanicity → elevation delta.
    auto getModifier = [](BoundaryType bt, bool thisOc, bool otherOc) -> float
    {
        if (bt == BoundaryType::Convergent)
        {
            if (!thisOc && !otherOc) return +0.40f;
            if (!thisOc &&  otherOc) return +0.40f;
            if ( thisOc && !otherOc) return +0.65f;
            return +0.30f;
        }
        if (bt == BoundaryType::Divergent)
        {
            if (!thisOc && !otherOc) return -0.20f;
            if ( thisOc &&  otherOc) return -0.10f;
        }
        return 0.f;
    };

    // Per-tile weighted modifier accumulators.
    std::vector<float> totalMod(width * height, 0.f);
    std::vector<float> totalWeight(width * height, 0.f);

    // Splat every boundary edge onto tiles within RADIUS.
    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
        {
            int id = ids[ty * width + tx];

            // --- Right edge (vertical) between (tx,ty) and (tx+1,ty) ---
            if (tx + 1 < width)
            {
                int idR = ids[ty * width + tx + 1];
                if (idR != id)
                {
                    BoundaryType bt = boundaryBetween(id, idR);
                    for (int dsy = -RADIUS; dsy <= RADIUS; ++dsy)
                    {
                        int sy = ty + dsy;
                        if (sy < 0 || sy >= height) continue;
                        for (int dsx = -RADIUS; dsx <= RADIUS; ++dsx)
                        {
                            float b = bellR[(dsy + RADIUS) * TS + (dsx + RADIUS)];
                            if (b == 0.f) continue;
                            int sxw = ((tx + 1 + dsx) % width + width) % width;
                            int pid = ids[sy * width + sxw];
                            if (pid != id && pid != idR) continue;
                            bool thisOc  = m_plates[pid].oceanic;
                            bool otherOc = (pid == id) ? m_plates[idR].oceanic : m_plates[id].oceanic;
                            float mod = getModifier(bt, thisOc, otherOc);
                            if (mod == 0.f) continue;
                            int tidx = sy * width + sxw;
                            totalMod[tidx]    += mod * b;
                            totalWeight[tidx] += b;
                        }
                    }
                }
            }

            // --- Bottom edge (horizontal) between (tx,ty) and (tx,ty+1) ---
            if (ty + 1 < height)
            {
                int idB = ids[(ty + 1) * width + tx];
                if (idB != id)
                {
                    BoundaryType bt = boundaryBetween(id, idB);
                    for (int dsy = -RADIUS; dsy <= RADIUS; ++dsy)
                    {
                        int sy = ty + 1 + dsy;
                        if (sy < 0 || sy >= height) continue;
                        for (int dsx = -RADIUS; dsx <= RADIUS; ++dsx)
                        {
                            float b = bellB[(dsy + RADIUS) * TS + (dsx + RADIUS)];
                            if (b == 0.f) continue;
                            int sxw = ((tx + dsx) % width + width) % width;
                            int pid = ids[sy * width + sxw];
                            if (pid != id && pid != idB) continue;
                            bool thisOc  = m_plates[pid].oceanic;
                            bool otherOc = (pid == id) ? m_plates[idB].oceanic : m_plates[id].oceanic;
                            float mod = getModifier(bt, thisOc, otherOc);
                            if (mod == 0.f) continue;
                            int tidx = sy * width + sxw;
                            totalMod[tidx]    += mod * b;
                            totalWeight[tidx] += b;
                        }
                    }
                }
            }
        }

    // Write final elevation: base + normalised accumulated modifier.
    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
        {
            int   idx  = ty * width + tx;
            float base = m_plates[ids[idx]].oceanic ? 0.20f : 0.60f;
            float mod  = totalMod[idx];
            if (totalWeight[idx] > 1.f) mod /= totalWeight[idx];
            tileAt(tx, ty).elevation = std::clamp(base + mod, 0.f, 1.f);
        }
}

// ---------------------------------------------------------------------------
// Smooth elevation with a separable box blur to soften junction transitions.
// This is not erosion — it does not simulate flow. It just reduces the hard
// plateau steps where boundary chains terminate at triple junctions.

void World::smoothElevation(int passes, int radius)
{
    std::vector<float> buf(width * height);

    for (int pass = 0; pass < passes; ++pass)
    {
        // Horizontal pass
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float sum = 0.f;
                int   cnt = 0;
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    int nx = x + dx;
                    if (nx < 0 || nx >= width) continue;
                    sum += tiles[y * width + nx].elevation;
                    ++cnt;
                }
                buf[y * width + x] = sum / cnt;
            }
        }
        // Vertical pass
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float sum = 0.f;
                int   cnt = 0;
                for (int dy = -radius; dy <= radius; ++dy)
                {
                    int ny = y + dy;
                    if (ny < 0 || ny >= height) continue;
                    sum += buf[ny * width + x];
                    ++cnt;
                }
                tiles[y * width + x].elevation = sum / cnt;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Step 3 — Add medium-frequency FBm noise to break up flat plate interiors.

void World::applyDetailNoise()
{
    const float AMP = 0.12f;   // ±12 % of elevation range
    const float fw  = static_cast<float>(width);
    const float fh  = static_cast<float>(height);

    for (int ty = 0; ty < height; ++ty)
        for (int tx = 0; tx < width; ++tx)
        {
            float nx = (static_cast<float>(tx) + 0.5f) / fw;
            float ny = (static_cast<float>(ty) + 0.5f) / fh;
            float n  = m_detailNoise.GetNoise(nx, ny);   // [-1, 1]
            float& e = tileAt(tx, ty).elevation;
            e = std::clamp(e + n * AMP, 0.f, 1.f);
        }
}

// ---------------------------------------------------------------------------
// Boundary noise — scatter circular bumps along convergent and divergent
// chains to break up the smooth ridge and trench surfaces.
//
// Sign bias (inverted from intuition):
//   Convergent ridges  → 70% negative bumps  (ridge is already at peak; bumps carve texture)
//   Divergent trenches → 70% positive bumps  (trench is already floored; bumps raise the bed)
//
// Bumps are placed by walking each chain's arc and stepping a random distance
// between placements.  A small lateral offset scatters bumps slightly off the
// boundary spine for a more organic look.

void World::applyBoundaryNoise()
{
    const float stride = static_cast<float>(tileSize + 1);

    std::mt19937 rng(seed + 54321);
    std::uniform_real_distribution<float> distStep   ( 3.f, 10.f);   // tiles between bumps
    std::uniform_real_distribution<float> distLateral(-3.f,  3.f);   // lateral offset (tiles)
    std::uniform_real_distribution<float> distRadius ( 4.f,  8.f);   // bump radius (tiles)
    std::normal_distribution<float>        distDelta  (0.08f, 0.035f); // height magnitude — bell around 0.08, extremes rare
    std::uniform_real_distribution<float> distSign   (0.f,   1.f);

    for (const auto& chain : m_boundaryChains)
    {
        if (chain.type != BoundaryType::Convergent &&
            chain.type != BoundaryType::Divergent)
            continue;

        const bool  isConvergent = (chain.type == BoundaryType::Convergent);
        const auto& pts = chain.points;
        const int   M   = static_cast<int>(pts.size());
        if (M < 2) continue;

        float accumulated = 0.f;
        float nextStep    = distStep(rng);

        for (int i = 0; i + 1 < M; ++i)
        {
            // Segment endpoints in tile space
            float ax = pts[i].x   / stride, ay = pts[i].y   / stride;
            float bx = pts[i+1].x / stride, by = pts[i+1].y / stride;
            float dx = bx - ax,  dy = by - ay;
            float segLen = std::sqrt(dx*dx + dy*dy);
            if (segLen < 1e-6f) continue;

            float t = 0.f;
            while (t < segLen)
            {
                float remaining = nextStep - accumulated;
                if (t + remaining >= segLen)
                {
                    accumulated += segLen - t;
                    break;
                }

                t += remaining;
                accumulated = 0.f;
                nextStep = distStep(rng);

                // Bump centre: interpolated chain point + random lateral offset
                float frac = t / segLen;
                float cx = ax + frac * dx;
                float cy = ay + frac * dy;

                // Perpendicular unit vector (rotated 90°)
                float px = -dy / segLen, py = dx / segLen;
                float lateral = distLateral(rng);
                cx += px * lateral;
                cy += py * lateral;

                float R     = distRadius(rng);
                float delta = std::clamp(distDelta(rng), 0.01f, 0.15f);

                // Invert sign bias: ridges mostly negative, trenches mostly positive
                float s = distSign(rng);
                if (isConvergent)
                    delta = (s < 0.7f) ? -delta : +delta;
                else
                    delta = (s < 0.7f) ? +delta : -delta;

                // Splat circular bump with smooth (1 - d²/R²)² falloff
                int minTx = std::max(0,          static_cast<int>(cx - R));
                int maxTx = std::min(width  - 1, static_cast<int>(cx + R) + 1);
                int minTy = std::max(0,          static_cast<int>(cy - R));
                int maxTy = std::min(height - 1, static_cast<int>(cy + R) + 1);

                float R2 = R * R;
                for (int ty = minTy; ty <= maxTy; ++ty)
                    for (int tx = minTx; tx <= maxTx; ++tx)
                    {
                        float ddx  = (tx + 0.5f) - cx;
                        float ddy  = (ty + 0.5f) - cy;
                        float d2   = ddx*ddx + ddy*ddy;
                        if (d2 >= R2) continue;
                        float fall = 1.f - d2 / R2;
                        float& e   = tileAt(tx, ty).elevation;
                        e = std::clamp(e + delta * fall * fall, 0.f, 1.f);
                    }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Step 4 — Thermal erosion: material slides downhill when slope exceeds the
// talus angle.  Double-buffered (read src, write delta) to avoid directional
// bias.  X wraps for cylindrical continuity.

void World::applyErosion()
{
    const int   PASSES = 25;
    const float TALUS  = 0.025f;  // max stable elevation difference per tile
    const float RATE   = 0.40f;   // fraction of excess that moves each pass

    const int ddx[] = { 1, -1,  0,  0 };
    const int ddy[] = { 0,  0,  1, -1 };

    std::vector<float> src(width * height);
    std::vector<float> delta(width * height);

    for (int i = 0; i < width * height; ++i)
        src[i] = tiles[i].elevation;

    for (int pass = 0; pass < PASSES; ++pass)
    {
        std::fill(delta.begin(), delta.end(), 0.f);

        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
            {
                float e = src[y * width + x];
                for (int k = 0; k < 4; ++k)
                {
                    int nx2 = (x + ddx[k] + width) % width;   // x wraps
                    int ny2 =  y + ddy[k];
                    if (ny2 < 0 || ny2 >= height) continue;
                    float diff = e - src[ny2 * width + nx2];
                    if (diff > TALUS)
                    {
                        float move = (diff - TALUS) * RATE * 0.5f;
                        delta[y   * width + x]    -= move;
                        delta[ny2 * width + nx2]  += move;
                    }
                }
            }

        for (int i = 0; i < width * height; ++i)
            src[i] = std::clamp(src[i] + delta[i], 0.f, 1.f);
    }

    for (int i = 0; i < width * height; ++i)
        tiles[i].elevation = src[i];
}

void World::computeStats()
{
}

// ---------------------------------------------------------------------------
// Rendering

sf::Color World::plateColor(int id)
{
    static const sf::Color palette[] = {
        sf::Color( 90,  70,  60),  // dark umber
        sf::Color( 55,  80,  75),  // dark teal
        sf::Color( 75,  85,  55),  // olive drab
        sf::Color( 95,  75,  45),  // dark sand
        sf::Color( 70,  60,  90),  // dusty purple
        sf::Color( 50,  80,  95),  // slate blue
        sf::Color( 95,  65,  55),  // dusty sienna
        sf::Color( 55,  90,  65),  // sage
        sf::Color( 90,  55,  75),  // dusty rose
        sf::Color( 45,  60,  85),  // steel blue
        sf::Color( 85,  75,  50),  // warm taupe
        sf::Color( 50,  85,  80),  // muted cyan
    };
    return palette[id % 12];
}

// Bake plate colours into a texture using per-pixel plateAt() evaluation —
// completely independent of the tile grid.
void World::bakeToPlateTexture()
{
    sf::Image img(sf::Vector2u{static_cast<unsigned>(width), static_cast<unsigned>(height)});

    const float fw = static_cast<float>(width);
    const float fh = static_cast<float>(height);

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            float nx = (static_cast<float>(x) + 0.5f) / fw;
            float ny = (static_cast<float>(y) + 0.5f) / fh;
            img.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)},
                         plateColor(plateAt(nx, ny)));
        }

    m_plateTexture.loadFromImage(img);
    m_plateTexture.setSmooth(false);

    const float scale = static_cast<float>(tileSize + 1);
    m_plateSprite.emplace(m_plateTexture);
    m_plateSprite->setScale({scale, scale});
}

// ---------------------------------------------------------------------------
// Draw boundary lines from cached chains

void World::drawBoundaryLines(sf::RenderWindow& window)
{
    auto typeColor = [](BoundaryType t) -> sf::Color {
        switch (t)
        {
            case BoundaryType::Convergent: return sf::Color(255, 100,  30);
            case BoundaryType::Divergent:  return sf::Color( 50, 220, 255);
            case BoundaryType::Transform:  return sf::Color(220, 255,  50);
            default:                       return sf::Color::Transparent;
        }
    };

    auto drawChain = [&](const BoundaryChain& chain, int passMin, int passMax,
                         sf::Color col, sf::Color haloCol, bool halo)
    {
        const int P = static_cast<int>(chain.points.size());
        if (P < 2) return;

        auto offsetStrip = [&](int passMin2, int passMax2, sf::Color c)
        {
            for (int pass = passMin2; pass <= passMax2; ++pass)
            {
                sf::VertexArray va(sf::PrimitiveType::LineStrip, P);
                for (int i = 0; i < P; ++i)
                {
                    sf::Vector2f pos = chain.points[i];
                    if (pass != 0)
                    {
                        int j = (i + 1 < P) ? i + 1 : i - 1;
                        sf::Vector2f seg = chain.points[j] - chain.points[i];
                        float len = std::sqrt(seg.x*seg.x + seg.y*seg.y);
                        if (len > 0.f)
                            pos += sf::Vector2f(-seg.y / len, seg.x / len) * static_cast<float>(pass);
                    }
                    va[i] = sf::Vertex{pos, c};
                }
                window.draw(va);
            }
        };

        if (halo) offsetStrip(passMin - 2, passMax + 2, haloCol);
        offsetStrip(passMin, passMax, col);
    };

    for (int ci = 0; ci < static_cast<int>(m_boundaryChains.size()); ++ci)
    {
        const BoundaryChain& chain = m_boundaryChains[ci];
        sf::Color col = typeColor(chain.type);
        if (col == sf::Color::Transparent) continue;

        const bool selected = (ci == m_selectedChainIdx);
        if (selected)
            drawChain(chain, -5, 5, col, sf::Color(255, 255, 255, 180), true);
        else
            drawChain(chain, -3, 3, col, sf::Color::Transparent, false);
    }
}

// ---------------------------------------------------------------------------
// Draw plate drift arrows — origin mapped from normalised → world pixels

void World::drawPlateArrows(sf::RenderWindow& window)
{
    const float stride     = static_cast<float>(tileSize + 1);
    const float worldW     = static_cast<float>(width)  * stride;
    const float worldH     = static_cast<float>(height) * stride;
    const float MAX_LEN    = 80.f;
    const float HEAD_LEN   = 14.f;
    const float HEAD_ANGLE = 0.45f;
    const sf::Color col(255, 255, 255);

    for (const TectonicPlate& p : m_plates)
    {
        const float shaftLen = MAX_LEN * p.speed;

        sf::Vector2f origin(p.centerX * worldW, p.centerY * worldH);
        sf::Vector2f tip = origin + sf::Vector2f(p.driftX, p.driftY) * shaftLen;

        float rx = -p.driftX, ry = -p.driftY;
        float cosA = std::cos(HEAD_ANGLE), sinA = std::sin(HEAD_ANGLE);

        sf::Vector2f arm1(
            tip.x + HEAD_LEN * (rx * cosA - ry * sinA),
            tip.y + HEAD_LEN * (rx * sinA + ry * cosA));
        sf::Vector2f arm2(
            tip.x + HEAD_LEN * (rx * cosA + ry * sinA),
            tip.y + HEAD_LEN * (-rx * sinA + ry * cosA));

        sf::VertexArray va(sf::PrimitiveType::Lines, 4);
        va[0] = sf::Vertex{origin, col};
        va[1] = sf::Vertex{tip,    col};
        va[2] = sf::Vertex{tip,    col};
        va[3] = sf::Vertex{arm1,   col};
        window.draw(va);

        sf::VertexArray va2(sf::PrimitiveType::Lines, 2);
        va2[0] = sf::Vertex{tip,  col};
        va2[1] = sf::Vertex{arm2, col};
        window.draw(va2);

        if (m_font.getInfo().family.empty()) continue;

        char buf[8];
        std::snprintf(buf, sizeof(buf), "%.2f", p.speed);
        sf::Text label(m_font, buf, 11);
        label.setFillColor(sf::Color(220, 220, 220));

        sf::FloatRect bounds = label.getLocalBounds();
        label.setOrigin(sf::Vector2f(
            bounds.position.x + bounds.size.x * 0.5f,
            bounds.position.y));
        label.setPosition(sf::Vector2f(tip.x, tip.y + 6.f));
        window.draw(label);
    }
}

// ---------------------------------------------------------------------------
// Border hit-test — returns debug info when worldPos is near a plate boundary

std::optional<World::BoundaryDebugInfo> World::pickBoundary(sf::Vector2f worldPos) const
{
    if (m_plates.size() < 2) return std::nullopt;

    const float stride = static_cast<float>(tileSize + 1);
    const float worldW = static_cast<float>(width)  * stride;
    const float worldH = static_cast<float>(height) * stride;

    const float nx = worldPos.x / worldW;
    const float ny = worldPos.y / worldH;
    if (nx < 0.f || nx > 1.f || ny < 0.f || ny > 1.f) return std::nullopt;

    // Click is "on" a boundary when the two nearest plates are nearly equidistant
    const float PICK_THRESH = 0.08f;
    PlateQuery q = queryPlate(nx, ny);
    if (q.d2 - q.d1 >= PICK_THRESH) return std::nullopt;

    const TectonicPlate& pa = m_plates[q.id1];
    const TectonicPlate& pb = m_plates[q.id2];

    float relX  = pb.driftX * pb.speed - pa.driftX * pa.speed;
    float relY  = pb.driftY * pb.speed - pa.driftY * pa.speed;
    float normX = pb.centerX - pa.centerX;
    if (normX >  0.5f) normX -= 1.0f;   // shortest path across wrap
    if (normX < -0.5f) normX += 1.0f;
    float normY = pb.centerY - pa.centerY;
    float nlen  = std::sqrt(normX*normX + normY*normY);
    if (nlen > 0.f) { normX /= nlen; normY /= nlen; }
    float dot   = relX * normX + relY * normY;
    float slide = std::abs(relX * normY - relY * normX);

    // Compass bearing: 0=North(up) 90=East(right) 180=South(down) 270=West(left)
    auto compass = [](float dx, float dy) -> float {
        float a = std::atan2(dx, -dy) * 180.f / 3.14159265f;
        if (a < 0.f) a += 360.f;
        return a;
    };

    // Find the nearest boundary chain to the click position
    const float CHAIN_PICK_PX = 20.f;   // world-pixel threshold
    int   nearestChain = -1;
    float nearestDist  = CHAIN_PICK_PX;

    for (int ci = 0; ci < static_cast<int>(m_boundaryChains.size()); ++ci)
    {
        const auto& pts = m_boundaryChains[ci].points;
        const int P = static_cast<int>(pts.size());
        for (int i = 0; i < P - 1; ++i)
        {
            sf::Vector2f ab = pts[i + 1] - pts[i];
            sf::Vector2f ap = worldPos    - pts[i];
            float abLen2 = ab.x*ab.x + ab.y*ab.y;
            float t = abLen2 > 0.f
                ? std::clamp((ap.x*ab.x + ap.y*ab.y) / abLen2, 0.f, 1.f)
                : 0.f;
            sf::Vector2f diff = worldPos - (pts[i] + ab * t);
            float d = std::sqrt(diff.x*diff.x + diff.y*diff.y);
            if (d < nearestDist) { nearestDist = d; nearestChain = ci; }
        }
    }

    BoundaryDebugInfo info;
    info.plateIdA    = q.id1;
    info.plateIdB    = q.id2;
    info.driftAX     = pa.driftX;
    info.driftAY     = pa.driftY;
    info.driftBX     = pb.driftX;
    info.driftBY     = pb.driftY;
    info.driftAngleA = compass(pa.driftX, pa.driftY);
    info.driftAngleB = compass(pb.driftX, pb.driftY);
    info.speedA      = pa.speed;
    info.speedB      = pb.speed;
    info.relVelX     = relX;
    info.relVelY     = relY;
    info.normalX     = normX;
    info.normalY     = normY;
    info.dotProduct      = dot;
    info.slideComponent  = slide;
    info.type            = boundaryBetween(q.id1, q.id2);
    info.chainIdx    = nearestChain;
    return info;
}

void World::bakeHeightMapTexture()
{
    sf::Image img(sf::Vector2u{static_cast<unsigned>(width), static_cast<unsigned>(height)});

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            auto v = static_cast<uint8_t>(tiles[y * width + x].elevation * 255.f);
            img.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)},
                         sf::Color(v, v, v));
        }

    m_heightMapTexture.loadFromImage(img);
    m_heightMapTexture.setSmooth(false);

    const float scale = static_cast<float>(tileSize + 1);
    m_heightMapSprite.emplace(m_heightMapTexture);
    m_heightMapSprite->setScale({scale, scale});
}

void World::drawHeightMap(sf::RenderWindow& window)
{
    if (m_heightMapSprite)
        window.draw(*m_heightMapSprite);
}

void World::bakeTerrainTexture()
{
    sf::Image img(sf::Vector2u{ static_cast<unsigned>(width), static_cast<unsigned>(height) });

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const Tile& t = tiles[y * width + x];
            // Temperature and moisture will be filled by the climate system later;
            // use neutral placeholders for now so biomes vary by elevation.
            TerrainType tt = classifyTerrain(t.elevation, 0.5f, 0.5f);
            img.setPixel({ static_cast<unsigned>(x), static_cast<unsigned>(y) },
                         terrainColor(tt));
        }

    m_terrainTexture.loadFromImage(img);
    m_terrainTexture.setSmooth(false);

    const float scale = static_cast<float>(tileSize + 1);
    m_terrainSprite.emplace(m_terrainTexture);
    m_terrainSprite->setScale({ scale, scale });
}

void World::drawTerrain(sf::RenderWindow& window)
{
    if (m_terrainSprite)
        window.draw(*m_terrainSprite);
}

void World::draw(sf::RenderWindow& window)
{
    if (m_renderMode == RenderMode::HeightMap)
    {
        drawHeightMap(window);
        drawBoundaryLines(window);
    }
    else if (m_renderMode == RenderMode::Terrain)
    {
        drawTerrain(window);
    }
    else
    {
        if (m_plateSprite)
            window.draw(*m_plateSprite);
        drawBoundaryLines(window);
        drawPlateArrows(window);
    }
}
