#pragma once
#include <SFML/Graphics.hpp>

class Camera
{
public:
    explicit Camera(sf::Vector2u windowSize);

    void handleScroll(float delta, sf::Vector2i mousePos, const sf::RenderWindow& window);
    void beginDrag(sf::Vector2i screenPos);
    void updateDrag(sf::Vector2i screenPos, const sf::RenderWindow& window);
    void endDrag();
    bool isDragging() const;

    const sf::View&  getView() const;
    sf::Vector2f     screenToWorld(sf::Vector2i screenPos, const sf::RenderWindow& window) const;

private:
    sf::View     m_view;
    bool         m_dragging = false;
    sf::Vector2i m_lastDragPos;
};
