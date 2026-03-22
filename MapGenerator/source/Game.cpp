#include "Game.h"

Game::Game()
    : window(sf::VideoMode({ 800, 800 }), "World"),
    world(50, 50, 8)
{
    world.initialize();
}

void Game::run()
{
    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        window.clear();
        world.draw(window);
        window.display();
    }
}