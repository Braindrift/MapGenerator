#pragma once
#include <SFML/Graphics.hpp>
#include <string>

enum class TerrainType
{
    DeepOcean = 0,
    ShallowWater,
    FrozenOcean,
    IceShore,
    Beach,
    Tundra,
    BorealForest,
    TemperateForest,
    Grassland,
    Shrubland,
    Savanna,
    TropicalForest,
    Desert,
    FrozenHills,
    Hills,
    Mountain,
    SnowCap,
    BarePeak,
    COUNT
};

inline sf::Color terrainColor(TerrainType t)
{
    static const sf::Color colors[] = {
        sf::Color(  0,  40, 100),  // DeepOcean
        sf::Color( 30,  80, 160),  // ShallowWater
        sf::Color(140, 175, 200),  // FrozenOcean
        sf::Color(220, 235, 245),  // IceShore
        sf::Color(210, 190, 130),  // Beach
        sf::Color(160, 165, 140),  // Tundra
        sf::Color( 40,  80,  55),  // BorealForest
        sf::Color( 55, 115,  50),  // TemperateForest
        sf::Color(105, 160,  65),  // Grassland
        sf::Color(135, 140,  70),  // Shrubland
        sf::Color(175, 165,  75),  // Savanna
        sf::Color( 15,  95,  35),  // TropicalForest
        sf::Color(215, 190, 115),  // Desert
        sf::Color(175, 185, 180),  // FrozenHills
        sf::Color(100, 110,  55),  // Hills
        sf::Color(120, 105,  90),  // Mountain
        sf::Color(240, 245, 250),  // SnowCap
        sf::Color(145, 125, 105),  // BarePeak
    };
    return colors[static_cast<int>(t)];
}

inline std::string terrainName(TerrainType t)
{
    static const char* names[] = {
        "Deep Ocean",
        "Shallow Water",
        "Frozen Ocean",
        "Ice Shore",
        "Beach",
        "Tundra",
        "Boreal Forest",
        "Temperate Forest",
        "Grassland",
        "Shrubland",
        "Savanna",
        "Tropical Forest",
        "Desert",
        "Frozen Hills",
        "Hills",
        "Mountain",
        "Snow Cap",
        "Bare Peak",
    };
    return names[static_cast<int>(t)];
}

inline TerrainType classifyTerrain(float e, float t, float m)
{
    // Ocean
    if (e < 0.40f)
    {
        if (t < 0.10f) return TerrainType::FrozenOcean;
        if (e < 0.32f) return TerrainType::DeepOcean;
        return TerrainType::ShallowWater;
    }
    // Shallow coastal
    if (e < 0.48f)
    {
        if (t < 0.10f) return TerrainType::FrozenOcean;
        return TerrainType::ShallowWater;
    }
    // Shore
    if (e < 0.52f)
    {
        if (t < 0.15f) return TerrainType::IceShore;
        return TerrainType::Beach;
    }
    // Lowland — temperature + moisture drive biome
    if (e < 0.70f)
    {
        if (t < 0.15f) return TerrainType::Tundra;
        if (t < 0.30f) return TerrainType::BorealForest;
        if (t < 0.55f) return m > 0.45f ? TerrainType::TemperateForest : TerrainType::Grassland;
        if (t < 0.65f) return m > 0.45f ? TerrainType::Shrubland      : TerrainType::Savanna;
        return              m > 0.50f ? TerrainType::TropicalForest    : TerrainType::Desert;
    }
    // Hills
    if (e < 0.82f)
    {
        if (t < 0.20f) return TerrainType::FrozenHills;
        return TerrainType::Hills;
    }
    // Mountains
    if (e < 0.92f) return TerrainType::Mountain;
    return t < 0.45f ? TerrainType::SnowCap : TerrainType::BarePeak;
}
