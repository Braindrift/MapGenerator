#include "World.h"
#include "FastNoiseLite.h"
#include <algorithm>
#include <cmath>

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

void World::initialize()
{
    // Large-scale continent shapes
    FastNoiseLite continentNoise;
    continentNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    continentNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    continentNoise.SetFractalOctaves(4);
    continentNoise.SetFrequency(0.005f);
    continentNoise.SetSeed(static_cast<int>(seed));

    // Domain warper — organic, irregular coastlines
    FastNoiseLite warpNoise;
    warpNoise.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    warpNoise.SetDomainWarpAmp(30.0f);
    warpNoise.SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive);
    warpNoise.SetFractalOctaves(3);
    warpNoise.SetFrequency(0.01f);
    warpNoise.SetSeed(static_cast<int>(seed) + 1);

    // Surface detail
    FastNoiseLite detailNoise;
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(5);
    detailNoise.SetFrequency(0.03f);
    detailNoise.SetSeed(static_cast<int>(seed) + 2);

    // Moisture — distinguishes jungle/forest from desert/savanna in warm regions
    FastNoiseLite moistureNoise;
    moistureNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    moistureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    moistureNoise.SetFractalOctaves(4);
    moistureNoise.SetFrequency(0.02f);
    moistureNoise.SetSeed(static_cast<int>(seed) + 3);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);

            float continent = (continentNoise.GetNoise(fx, fy) + 1.0f) * 0.5f;

            float wx = fx, wy = fy;
            warpNoise.DomainWarp(wx, wy);
            float detail = (detailNoise.GetNoise(wx, wy) + 1.0f) * 0.5f;

            float elevation    = continent * 0.6f + detail * 0.4f;
            float latTemp      = 1.0f - std::abs(2.0f * y / (height - 1) - 1.0f);
            float temperature  = std::clamp(latTemp - elevation * 0.3f, 0.0f, 1.0f);
            float moisture     = (moistureNoise.GetNoise(fx, fy) + 1.0f) * 0.5f;

            Tile& tile = tileAt(x, y);
            tile.elevation   = elevation;
            tile.temperature = temperature;
            tile.moisture    = moisture;
            tile.terrain     = classifyTerrain(elevation, temperature, moisture);
            tile.color       = terrainColor(tile.terrain);
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
            rect.setFillColor(tileAt(x, y).color);

            window.draw(rect);
        }
    }
}
