#include "Game.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <random>
#include <cstdio>
#include <cstring>

Game::Game()
    : window(sf::VideoMode({ 720, 500 }), "World"),
      world(50, 50, 8, 0),
      panel(465.f, 16.f),
      m_camera(sf::Vector2u{ 720, 500 }),
      m_currentSeed(std::random_device{}())
{
    ImGui::SFML::Init(window);
    std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_currentSeed);
    world.regenerate(m_currentSeed);
}

Game::~Game()
{
    ImGui::SFML::Shutdown();
}

void Game::run()
{
    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>())
                window.close();

            if (!ImGui::GetIO().WantCaptureMouse)
            {
                if (const auto* e = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (e->button == sf::Mouse::Button::Left)
                        handleClick(e->position.x, e->position.y);
                    if (e->button == sf::Mouse::Button::Right)
                        m_camera.beginDrag(e->position);
                }

                if (const auto* e = event->getIf<sf::Event::MouseButtonReleased>())
                    if (e->button == sf::Mouse::Button::Right)
                        m_camera.endDrag();

                if (const auto* e = event->getIf<sf::Event::MouseMoved>())
                    m_camera.updateDrag(e->position, window);

                if (const auto* e = event->getIf<sf::Event::MouseWheelScrolled>())
                    m_camera.handleScroll(e->delta, e->position, window);
            }
        }

        ImGui::SFML::Update(window, m_clock.restart());
        renderControls();

        window.clear(sf::Color(20, 20, 20));

        window.setView(m_camera.getView());
        world.draw(window);

        window.setView(window.getDefaultView());
        panel.draw(window);
        ImGui::SFML::Render(window);

        window.display();
    }
}

void Game::handleClick(int px, int py)
{
    sf::Vector2f worldPos = m_camera.screenToWorld({ px, py }, window);

    const int stride   = world.getTileSize() + 1;
    const int tileSize = world.getTileSize();

    const int wx = static_cast<int>(worldPos.x);
    const int wy = static_cast<int>(worldPos.y);

    if (wx < 0 || wy < 0) { panel.clearSelection(); return; }

    const int  gridX    = wx / stride;
    const int  gridY    = wy / stride;
    const bool inTile   = (wx % stride) < tileSize && (wy % stride) < tileSize;
    const bool inBounds = gridX < world.getWidth() && gridY < world.getHeight();

    if (inTile && inBounds)
        panel.setSelectedTile(gridX, gridY, world.getTile(gridX, gridY));
    else
        panel.clearSelection();
}

void Game::renderControls()
{
    ImGui::SetNextWindowPos(ImVec2(465.f, 150.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(245.f, 120.f), ImGuiCond_Always);
    ImGui::Begin("Generation", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Seed:");
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##seed", m_seedBuffer, sizeof(m_seedBuffer),
        ImGuiInputTextFlags_CharsDecimal);

    if (ImGui::Button("Regenerate", ImVec2(-1.f, 0.f)))
    {
        if (m_seedBuffer[0] == '\0')
        {
            m_currentSeed = std::random_device{}();
            std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_currentSeed);
        }
        else
        {
            m_currentSeed = static_cast<unsigned int>(std::stoul(m_seedBuffer));
        }
        world.regenerate(m_currentSeed);
        panel.clearSelection();
    }

    if (ImGui::Button("Random", ImVec2(-1.f, 0.f)))
    {
        m_currentSeed = std::random_device{}();
        std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_currentSeed);
        world.regenerate(m_currentSeed);
        panel.clearSelection();
    }

    ImGui::End();
}
