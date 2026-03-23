#include "Game.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <random>
#include <cstdio>
#include <cstring>

Game::Game()
    : window(sf::VideoMode({ 720, 500 }), "World"),
      world(512, 256, 8, 0),
      panel(720.f * MAP_PANEL_RATIO + 8.f, 16.f),
      m_camera(sf::Vector2u{ 720, 500 }, MAP_PANEL_RATIO),
      m_uiView(sf::FloatRect({ 0.f, 0.f }, { 720.f, 500.f })),
      m_currentSeed(std::random_device{}())
{
    ImGui::SFML::Init(window);
    std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_currentSeed);
    world.regenerate(m_currentSeed);

    const float worldW = static_cast<float>(world.getWidth()  * (world.getTileSize() + 1));
    const float worldH = static_cast<float>(world.getHeight() * (world.getTileSize() + 1));
    m_camera.focusOn(worldW, worldH);
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

            if (const auto* e = event->getIf<sf::Event::Resized>())
                handleResize(e->size.x, e->size.y);

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

        window.setView(m_uiView);
        const auto sz = window.getSize();
        sf::RectangleShape uiBg({ sz.x * (1.f - MAP_PANEL_RATIO), static_cast<float>(sz.y) });
        uiBg.setPosition({ sz.x * MAP_PANEL_RATIO, 0.f });
        uiBg.setFillColor(sf::Color(40, 40, 40));
        window.draw(uiBg);

        panel.draw(window);
        ImGui::SFML::Render(window);

        window.display();
    }
}

void Game::handleResize(unsigned w, unsigned h)
{
    m_uiView = sf::View(sf::FloatRect({ 0.f, 0.f },
        { static_cast<float>(w), static_cast<float>(h) }));
    m_camera.onWindowResize(w, h);
    panel.setOrigin(w * MAP_PANEL_RATIO + 8.f, 16.f);
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
    const auto sz = window.getSize();
    const float panelX = sz.x * MAP_PANEL_RATIO;
    const float panelW = sz.x * (1.f - MAP_PANEL_RATIO);

    ImGui::SetNextWindowPos(ImVec2(panelX, 150.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, 295.f), ImGuiCond_Always);
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

    ImGui::Separator();
    ImGui::Text("View:");

    const ViewMode current = world.getViewMode();

    auto viewButton = [&](const char* label, ViewMode mode)
    {
        if (current == mode)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.3f, 0.6f, 0.9f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 1.f));
            ImGui::Button(label, ImVec2(-1.f, 0.f));
            ImGui::PopStyleColor(2);
        }
        else if (ImGui::Button(label, ImVec2(-1.f, 0.f)))
        {
            world.setViewMode(mode);
        }
    };

    viewButton("Terrain",     ViewMode::Terrain);
    viewButton("Elevation",   ViewMode::Elevation);
    viewButton("Temperature", ViewMode::Temperature);
    viewButton("Moisture",    ViewMode::Moisture);
    viewButton("Plates",      ViewMode::Plates);

    ImGui::End();

    // --- World Analysis window ---
    const float analysisY = 150.f + 295.f + 8.f;
    ImGui::SetNextWindowPos(ImVec2(panelX, analysisY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, 255.f), ImGuiCond_Always);
    ImGui::Begin("World Analysis", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse);

    const WorldStats& s = world.getStats();

    auto statRow = [&](const char* label, float value, float refLo, float refHi, const char* refStr)
    {
        bool inRange = value >= refLo && value <= refHi;
        ImVec4 col = inRange ? ImVec4(0.4f, 1.f, 0.4f, 1.f) : ImVec4(1.f, 0.4f, 0.4f, 1.f);
        ImGui::Text("%-12s", label);
        ImGui::SameLine();
        ImGui::TextColored(col, "%5.1f%%", value);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", refStr);
    };

    auto statRowInt = [&](const char* label, int value, int refLo, int refHi, const char* refStr)
    {
        bool inRange = value >= refLo && value <= refHi;
        ImVec4 col = inRange ? ImVec4(0.4f, 1.f, 0.4f, 1.f) : ImVec4(1.f, 0.4f, 0.4f, 1.f);
        ImGui::Text("%-12s", label);
        ImGui::SameLine();
        ImGui::TextColored(col, "%5d  ", value);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", refStr);
    };

    statRow   ("Land",       s.landPercent,            25.f,  35.f,  "[~29%]");
    statRow   ("Ocean",      s.oceanPercent,            65.f,  75.f,  "[~71%]");
    statRowInt("Landmasses", s.landmassCount,           4,     9,     "[~7]");
    statRow   ("Largest",    s.largestLandmassPercent,  8.f,   20.f,  "[~11%]");
    statRow   ("Mountains",  s.mountainPercent,         4.f,   12.f,  "[~7%]");
    statRow   ("Tropical",   s.tropicalPercent,         3.f,   9.f,   "[~6%]");
    statRow   ("Polar",      s.polarPercent,            7.f,   14.f,  "[~10%]");

    ImGui::Separator();
    if (ImGui::Button("Copy Stats", ImVec2(-1.f, 0.f)))
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Seed:%u Land:%.1f%% Ocean:%.1f%% Landmasses:%d Largest:%.1f%% Mountains:%.1f%% Tropical:%.1f%% Polar:%.1f%%",
            m_currentSeed,
            s.landPercent, s.oceanPercent,
            s.landmassCount, s.largestLandmassPercent,
            s.mountainPercent, s.tropicalPercent, s.polarPercent);
        ImGui::SetClipboardText(buf);
    }

    ImGui::End();
}
