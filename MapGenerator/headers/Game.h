#pragma once
#include <SFML/Graphics.hpp>
#include "World.h"

class Game
{
public:
    Game();
    ~Game();
    void run();

private:
    void resetView();
    void handleResize(unsigned w, unsigned h);
    void handleScroll(float delta, sf::Vector2i mousePos);
    void handleDragBegin(sf::Vector2i pos);
    void handleDragEnd();
    void handleDragMove(sf::Vector2i pos);
    void handleMouseMove(sf::Vector2i pos);
    void renderUI();

    // Viewport: world renders in left (1 - PANEL_W) fraction of the window
    static constexpr float PANEL_W   = 0.15f;
    static constexpr float ZOOM_MIN  = 0.25f;
    static constexpr float ZOOM_MAX  = 8.00f;
    static constexpr float ZOOM_STEP = 0.10f;

    // Returns the pixel width of the world viewport
    float viewportW() const { return m_window.getSize().x * (1.f - PANEL_W); }
    float viewportH() const { return static_cast<float>(m_window.getSize().y); }

    sf::RenderWindow m_window;
    sf::Clock        m_clock;
    World            m_world;

    // Camera
    sf::View         m_worldView;
    bool             m_dragging      = false;
    sf::Vector2i     m_dragOrigin;
    sf::Vector2f     m_dragViewOrigin;

    // Mouse tracking for hover info
    sf::Vector2i     m_lastMousePos;

    // Generation
    unsigned int     m_seed;
    char             m_seedBuffer[12];
};
