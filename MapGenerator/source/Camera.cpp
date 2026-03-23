#include "Camera.h"
#include <algorithm>

Camera::Camera(sf::Vector2u windowSize, float mapPanelRatio)
    : m_aspectRatio((windowSize.x * mapPanelRatio) / static_cast<float>(windowSize.y))
    , m_mapPanelRatio(mapPanelRatio)
{
    m_view = sf::View(sf::FloatRect(
        { 0.f, 0.f },
        { static_cast<float>(windowSize.x), static_cast<float>(windowSize.y) }
    ));
    m_view.setViewport(sf::FloatRect({ 0.f, 0.f }, { mapPanelRatio, 1.f }));
}

void Camera::focusOn(float worldPixelWidth, float worldPixelHeight)
{
    m_maxViewHeight = worldPixelHeight * 1.5f;

    float viewH = worldPixelHeight;
    float viewW = viewH * m_aspectRatio;
    if (worldPixelWidth > viewW)
    {
        viewW = worldPixelWidth;
        viewH = viewW / m_aspectRatio;
    }

    m_view.setSize({ viewW, viewH });
    m_view.setCenter({ worldPixelWidth * 0.5f, worldPixelHeight * 0.5f });
    m_view.setViewport(sf::FloatRect({ 0.f, 0.f }, { m_mapPanelRatio, 1.f }));
}

void Camera::onWindowResize(unsigned w, unsigned h)
{
    m_aspectRatio = (w * m_mapPanelRatio) / static_cast<float>(h);
    const float currentHeight = m_view.getSize().y;
    m_view.setSize({ currentHeight * m_aspectRatio, currentHeight });
    m_view.setViewport(sf::FloatRect({ 0.f, 0.f }, { m_mapPanelRatio, 1.f }));
}

void Camera::handleScroll(float delta, sf::Vector2i mousePos, const sf::RenderWindow& window)
{
    const float factor = delta > 0 ? 0.85f : 1.15f;

    sf::Vector2f before = window.mapPixelToCoords(mousePos, m_view);

    float newHeight = std::clamp(m_view.getSize().y * factor, 64.f, m_maxViewHeight);
    m_view.setSize({ newHeight * m_aspectRatio, newHeight });

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
