#pragma once
#include <SFML/Graphics.hpp>
#include "World.h"
#include "InfoPanel.h"
#include "Camera.h"

class Game
{
public:
    Game();
    ~Game();
    void run();

private:
    void handleClick(int px, int py);
    void renderControls();

    sf::RenderWindow window;
    sf::Clock        m_clock;
    World            world;
    InfoPanel        panel;
    Camera           m_camera;

    unsigned int     m_currentSeed;
    char             m_seedBuffer[12];
};
