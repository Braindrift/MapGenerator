#include "InfoPanel.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>

InfoPanel::InfoPanel(float originX, float originY)
    : m_originX(originX), m_originY(originY)
{
    if (!m_font.openFromFile("C:/Windows/Fonts/arial.ttf"))
        throw std::runtime_error("InfoPanel: failed to load font");
}

void InfoPanel::setSelectedTile(int x, int y, const Tile& tile)
{
    m_selected = TileInfo{ x, y, tile.elevation, tile.temperature, tile.moisture, tile.terrain };
}

void InfoPanel::clearSelection()
{
    m_selected.reset();
}

void InfoPanel::setOrigin(float x, float y)
{
    m_originX = x;
    m_originY = y;
}

void InfoPanel::draw(sf::RenderWindow& window) const
{
    sf::RectangleShape bg(sf::Vector2f(239.f, 135.f));
    bg.setPosition(sf::Vector2f(m_originX, m_originY));
    bg.setFillColor(sf::Color(30, 30, 30, 200));
    window.draw(bg);

    auto makeText = [&](const std::string& str, float dy)
    {
        sf::Text t(m_font, str, 14);
        t.setFillColor(sf::Color::White);
        t.setPosition(sf::Vector2f(m_originX + 8.f, m_originY + dy));
        return t;
    };

    if (!m_selected)
    {
        window.draw(makeText("No tile selected", 8.f));
        return;
    }

    const TileInfo& ti = *m_selected;

    auto fmt = [](float v) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << v;
        return oss.str();
    };

    window.draw(makeText("Tile (" + std::to_string(ti.x) + ", " + std::to_string(ti.y) + ")", 8.f));
    window.draw(makeText("Type: " + terrainName(ti.terrain), 30.f));
    window.draw(makeText("Elevation:   " + fmt(ti.elevation),   52.f));
    window.draw(makeText("Temperature: " + fmt(ti.temperature), 74.f));
    window.draw(makeText("Moisture:    " + fmt(ti.moisture),    96.f));
}
