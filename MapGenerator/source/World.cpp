#include "World.h"

World::World(int width, int height, int tileSize)
    : width(width), height(height), tileSize(tileSize)
{
    tiles.resize(width * height);
}

Tile& World::getTile(int x, int y)
{
    return tiles[y * width + x];
}

void World::initialize()
{
    const sf::Color c1 = sf::Color::Blue;
    const sf::Color c2 = sf::Color::Red;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            getTile(x, y).color = ((x + y) & 1) ? c2 : c1;
        }
    }
}

void World::draw(sf::RenderWindow& window)
{
    sf::RectangleShape rect(sf::Vector2f(tileSize, tileSize));

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            rect.setPosition(sf::Vector2f((x * tileSize) + (x * 1), (y * tileSize) + (y * 1)));
            rect.setFillColor(getTile(x, y).color);

            window.draw(rect);
        }
    }
}