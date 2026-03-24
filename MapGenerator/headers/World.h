#pragma once
#include <optional>
#include <vector>
#include <SFML/Graphics.hpp>
#include "Tile.h"
#include "Terrain.h"
#include "FastNoiseLite.h"

class World
{
public:
    World(int width, int height, int tileSize, unsigned int seed);

    enum class RenderMode { TectonicPlates, HeightMap };

    void initialize();
    void regenerate(unsigned int newSeed);
    void draw(sf::RenderWindow& window);

    void       setRenderMode(RenderMode mode);
    RenderMode getRenderMode() const;

    int getWidth()    const;
    int getHeight()   const;
    int getTileSize() const;
    const Tile& getTile(int x, int y) const;

public:
    enum class BoundaryType { None, Convergent, Divergent, Transform };

    // Filled by pickBoundary(); contains everything that drives the classification.
    struct BoundaryDebugInfo
    {
        int   plateIdA,    plateIdB;
        float driftAX,     driftAY;      // plate A unit drift vector
        float driftBX,     driftBY;      // plate B unit drift vector
        float driftAngleA, driftAngleB;  // compass bearing (0=N 90=E 180=S 270=W)
        float speedA,      speedB;
        float relVelX,     relVelY;      // relative velocity (B - A)
        float normalX,     normalY;      // unit vector A→B
        float dotProduct;
        float slideComponent;  // |relVel × normal| — parallel magnitude
        BoundaryType type;
        int   chainIdx;   // index into m_boundaryChains; -1 if not found
    };

    std::optional<BoundaryDebugInfo> pickBoundary(sf::Vector2f worldPos) const;
    void setSelectedChain(int idx);

private:
    struct TectonicPlate
    {
        float centerX, centerY;  // normalized [0,1]×[0,1]
        float driftX,  driftY;   // unit direction vector
        float speed;             // 0.3–1.0
        bool  oceanic;
        bool  isPolar = false;   // static polar cap — no drift
    };

    static constexpr float POLAR_RX = 0.25f;  // half-width of polar cap ellipse (normalized)
    static constexpr float POLAR_RY = 0.10f;  // height of polar cap ellipse (normalized)

    // Result of a continuous-space Voronoi query
    struct PlateQuery
    {
        int   id1, id2;   // nearest and second-nearest plate index
        float d1,  d2;    // AR-corrected distances
    };

    struct BoundaryChain
    {
        std::vector<sf::Vector2f> points;
        BoundaryType              type;
    };

    // Per-tile record of the nearest boundary edge (built by buildBoundaryField).
    struct BoundaryField
    {
        float        dist;    // distance in tiles (FLT_MAX = none within range)
        int          plateA, plateB;
        BoundaryType type;
    };

    int          width;
    int          height;
    int          tileSize;
    unsigned int seed;

    std::vector<Tile>          tiles;
    std::vector<TectonicPlate> m_plates;
    std::vector<BoundaryChain> m_boundaryChains;
    std::vector<BoundaryField> m_boundaryField;
    sf::Texture                m_plateTexture;
    std::optional<sf::Sprite>  m_plateSprite;
    sf::Texture                m_heightMapTexture;
    std::optional<sf::Sprite>  m_heightMapSprite;
    sf::Font                   m_font;

    int                        m_selectedChainIdx = -1;
    RenderMode                 m_renderMode = RenderMode::TectonicPlates;

    FastNoiseLite              m_warpX;
    FastNoiseLite              m_warpY;
    FastNoiseLite              m_jitterNoise;
    FastNoiseLite              m_detailNoise;

    Tile&        tileAt(int x, int y);
    void         initNoise();
    void         generatePlates();
    void         assignTilePlates();
    PlateQuery   queryPlate(float nx, float ny) const;
    int          plateAt(float nx, float ny)    const;
    BoundaryType boundaryBetween(int idA, int idB) const;
    void         buildBoundaryChains();
    void         bakeToPlateTexture();
    void         drawBoundaryLines(sf::RenderWindow& window);
    void         drawPlateArrows(sf::RenderWindow& window);

    void         buildBoundaryField();
    void         computeElevation();
    void         smoothElevation(int passes = 2, int radius = 3);
    void         applyDetailNoise();
    void         applyErosion();
    void         bakeHeightMapTexture();
    void         drawHeightMap(sf::RenderWindow& window);
    void         computeStats();

    static sf::Color plateColor(int id);
};
