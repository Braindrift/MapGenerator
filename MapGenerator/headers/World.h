#pragma once
#include <optional>
#include <vector>
#include <SFML/Graphics.hpp>
#include "Tile.h"
#include "Terrain.h"

enum class ViewMode { Terrain, Elevation, Temperature, Moisture, Plates };

struct WorldStats
{
    float landPercent            = 0.f;
    float oceanPercent           = 0.f;
    int   landmassCount          = 0;
    float largestLandmassPercent = 0.f;
    float mountainPercent        = 0.f;
    float tropicalPercent        = 0.f;
    float polarPercent           = 0.f;
};

class World
{
public:
    World(int width, int height, int tileSize, unsigned int seed);

    void initialize();
    void regenerate(unsigned int newSeed);
    void setViewMode(ViewMode mode);
    void draw(sf::RenderWindow& window);

    int getWidth()    const;
    int getHeight()   const;
    int getTileSize() const;
    const Tile&       getTile(int x, int y) const;
    const WorldStats& getStats()            const { return m_stats; }

    ViewMode getViewMode() const { return m_viewMode; }

private:
    struct TectonicPlate
    {
        float centerX, centerY;  // plate-grid coordinates
        float driftX,  driftY;   // unit direction vector
        bool  oceanic;
    };

    static constexpr int PLATE_SCALE = 8;  // tiles per plate-grid cell

    int          width;
    int          height;
    int          tileSize;
    unsigned int seed;
    ViewMode     m_viewMode = ViewMode::Terrain;

    std::vector<Tile>          tiles;
    std::vector<TectonicPlate> m_plates;
    std::vector<int>           m_plateGrid;  // [PH × PW] plate IDs
    sf::RenderTexture          m_worldTexture;
    std::optional<sf::Sprite>  m_worldSprite;
    WorldStats                 m_stats;

    Tile& tileAt(int x, int y);
    void  generatePlates();
    void  buildPlateGrid();
    void  buildInfluenceMap(std::vector<float>& posInfluence, std::vector<float>& negInfluence);
    void  applyErosion();
    void  computeStats();
    void  bakeToTexture();

    static sf::Color elevationColor(float v);
    static sf::Color temperatureColor(float v);
    static sf::Color moistureColor(float v);
    static sf::Color plateColor(int id);
};
