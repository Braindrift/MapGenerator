#include "Camera.h"
#include <algorithm>

Camera::Camera(sf::Vector2u windowSize)
{
    m_view = sf::View(sf::FloatRect(
        { 0.f, 0.f },
        { static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) }
    ));
}

void Camera::handleScroll(float delta, sf::Vector2i mousePos, const sf::RenderWindow& window)
{
    const float factor = delta > 0 ? 0.85f : 1.15f;

    // World point under the cursor before zoom
    sf::Vector2f before = window.mapPixelToCoords(mousePos, m_view);

    sf::Vector2f newSize = m_view.getSize() * factor;
    newSize.x = std::clamp(newSize.x, 64.f, 4000.f);
    newSize.y = std::clamp(newSize.y, 64.f, 4000.f);
    m_view.setSize(newSize);

    // Move view so the same world point stays under the cursor
    sf::Vector2f after = window.mapPixelToCoords(mousePos, m_view);
    m_view.move(before - after);
}

void Camera::beginDrag(sf::Vector2i screenPos)
{
    m_dragging    = true;
    m_lastDragPos = screenPos;
}

void Camera::updateDrag(sf::Vector2i screenPos, const sf::RenderWindow& window)
{
    if (!m_dragging) return;

    sf::Vector2f worldPrev = window.mapPixelToCoords(m_lastDragPos, m_view);
    sf::Vector2f worldCurr = window.mapPixelToCoords(screenPos,     m_view);
    m_view.move(worldPrev - worldCurr);

    m_lastDragPos = screenPos;
}

void Camera::endDrag()
{
    m_dragging = false;
}

bool Camera::isDragging() const
{
    return m_dragging;
}

const sf::View& Camera::getView() const
{
    return m_view;
}

sf::Vector2f Camera::screenToWorld(sf::Vector2i screenPos, const sf::RenderWindow& window) const
{
    return window.mapPixelToCoords(screenPos, m_view);
}
