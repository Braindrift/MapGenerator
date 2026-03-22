#pragma once
#include <vector>
#include <SFML/Graphics.hpp>
#include "Tile.h"

class World
{
public:
    World(int width, int height, int tileSize);

    void initialize();
    void draw(sf::RenderWindow& window);

private:
    int width;
    int height;
    int tileSize;

    std::vector<Tile> tiles;

    Tile& getTile(int x, int y);
};