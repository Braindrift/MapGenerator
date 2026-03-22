#pragma once
#include <SFML/Graphics.hpp>
#include "World.h"

class Game
{
public:
    Game();
    void run();

private:
    sf::RenderWindow window;
    World world;
};