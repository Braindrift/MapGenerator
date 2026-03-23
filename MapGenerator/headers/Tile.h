#pragma once
#include <SFML/Graphics.hpp>
#include "Terrain.h"

class Tile
{
public:
    float       elevation   = 0.0f;
    float       temperature = 0.0f;  // 0.0 = arctic, 1.0 = tropical
    float       moisture    = 0.0f;  // 0.0 = arid, 1.0 = wet
    TerrainType terrain     = TerrainType::DeepOcean;
    sf::Color   color;
    int         plateId     = 0;
};
