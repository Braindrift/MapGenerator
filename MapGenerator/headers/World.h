#pragma once
#include <vector>
#include <SFML/Graphics.hpp>
#include "Tile.h"
#include "Terrain.h"

class World
{
public:
    World(int width, int height, int tileSize, unsigned int seed);

    void initialize();
    void regenerate(unsigned int newSeed);
    void draw(sf::RenderWindow& window);

    int getWidth()    const;
    int getHeight()   const;
    int getTileSize() const;
    const Tile& getTile(int x, int y) const;

private:
    int width;
    int height;
    int tileSize;
    unsigned int seed;

    std::vector<Tile> tiles;

    Tile& tileAt(int x, int y);
};