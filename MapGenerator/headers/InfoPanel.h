#pragma once
#include <optional>
#include <string>
#include <SFML/Graphics.hpp>
#include "Tile.h"
#include "Terrain.h"

struct TileInfo
{
    int         x;
    int         y;
    float       elevation;
    float       temperature;
    float       moisture;
    TerrainType terrain;
};

class InfoPanel
{
public:
    InfoPanel(float originX, float originY);

    void setSelectedTile(int x, int y, const Tile& tile);
    void clearSelection();
    void setOrigin(float x, float y);
    void draw(sf::RenderWindow& window) const;

private:
    sf::Font                m_font;
    float                   m_originX;
    float                   m_originY;
    std::optional<TileInfo> m_selected;
};
