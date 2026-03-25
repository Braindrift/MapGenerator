#pragma once
#include <vector>
#include <optional>
#include <SFML/Graphics.hpp>
#include "Tile.h"
#include "FastNoiseLite.h"

class World
{
public:
    World(int width, int height, int tileSize);

    void generate(unsigned int seed);
    void draw(sf::RenderWindow& window);

    enum class RenderMode { Plates, HeightMap };
    void       setRenderMode(RenderMode mode) { m_renderMode = mode; }
    RenderMode getRenderMode() const          { return m_renderMode; }

    // Overlays
    bool showBoundaries = true;

    int   getWidth()    const { return m_width; }
    int   getHeight()   const { return m_height; }
    int   getTileSize() const { return m_tileSize; }
    float pixelWidth()  const { return static_cast<float>(m_width  * (m_tileSize + 1)); }
    float pixelHeight() const { return static_cast<float>(m_height * (m_tileSize + 1)); }

    // -------------------------------------------------------------------------
    // Debug / inspection
    // -------------------------------------------------------------------------
    struct Stats
    {
        int   plateCount       = 0;
        int   plateOceanic     = 0;
        int   plateContinental = 0;
        int   chainTotal       = 0;
        int   chainConvergent  = 0;
        int   chainDivergent   = 0;
        int   chainTransform   = 0;
        float genTimeMs        = 0.f;
    };

    struct HoverInfo
    {
        bool  valid     = false;
        int   tileX     = 0;
        int   tileY     = 0;
        int   plateId   = -1;
        bool  oceanic   = false;
        bool  isPolar   = false;
        float elevation = 0.f;
    };

    const Stats& getStats() const { return m_stats; }
    HoverInfo    getHoverInfo(sf::Vector2f worldPos) const;
    void         exportCSV(const std::string& path) const;

private:
    // -------------------------------------------------------------------------
    // Tectonic plates
    // -------------------------------------------------------------------------
    struct Plate
    {
        float cx, cy;
        float dx, dy;
        float speed;
        bool  oceanic;
        bool  isPolar = false;
    };

    // -------------------------------------------------------------------------
    // Boundary chains
    // -------------------------------------------------------------------------
    enum class BoundaryType { Convergent, Divergent, Transform };

    struct BoundaryEdge
    {
        sf::Vector2f pos;
        sf::Vector2f tangent;
        sf::Vector2f normal;
    };

    struct BoundaryChain
    {
        std::vector<BoundaryEdge> edges;
        BoundaryType              type;
        int                       plateA, plateB;
        float                     compression;
    };

    // -------------------------------------------------------------------------
    // Data
    // -------------------------------------------------------------------------
    int          m_width;
    int          m_height;
    int          m_tileSize;
    unsigned int m_seed = 0;

    std::vector<Tile>          m_tiles;
    std::vector<Plate>         m_plates;
    std::vector<BoundaryChain> m_chains;

    sf::Texture                m_plateTexture;
    std::optional<sf::Sprite>  m_plateSprite;

    sf::Texture                m_boundaryTexture;
    std::optional<sf::Sprite>  m_boundarySprite;

    sf::Texture                m_heightTexture;
    std::optional<sf::Sprite>  m_heightSprite;

    RenderMode    m_renderMode = RenderMode::Plates;
    Stats         m_stats;

    FastNoiseLite m_warpX;
    FastNoiseLite m_warpY;
    FastNoiseLite m_strengthNoise;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    Tile&       tileAt(int x, int y)       { return m_tiles[y * m_width + x]; }
    const Tile& tileAt(int x, int y) const { return m_tiles[y * m_width + x]; }

    void generatePlates();
    int  nearestPlate(float nx, float ny) const;
    void assignTilePlates();
    void buildBoundaryChains();
    void computeElevation();
    void bakePlateTexture();
    void bakeBoundaryOverlay();
    void bakeHeightTexture();

    static sf::Color plateColor(int id);
    static sf::Color boundaryColor(BoundaryType type);
};
