#include "Game.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <random>
#include <cstdio>
#include <algorithm>

static constexpr unsigned int WIN_W = 1200;
static constexpr unsigned int WIN_H =  800;

Game::Game()
    : m_window(sf::VideoMode({ WIN_W, WIN_H }), "MapGenerator"),
      m_world(512, 256, 8),
      m_seed(std::random_device{}())
{
    ImGui::SFML::Init(m_window);
    std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_seed);
    m_world.generate(m_seed);
    resetView();
}

Game::~Game()
{
    ImGui::SFML::Shutdown();
}

// ---------------------------------------------------------------------------
// View setup
// ---------------------------------------------------------------------------

void Game::resetView()
{
    const float aspect = viewportW() / viewportH();
    const float viewH  = m_world.pixelHeight();
    const float viewW  = viewH * aspect;

    m_worldView = sf::View(sf::FloatRect({ 0.f, 0.f }, { viewW, viewH }));
    m_worldView.setViewport(sf::FloatRect({ 0.f, 0.f }, { 1.f - PANEL_W, 1.f }));
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void Game::run()
{
    while (m_window.isOpen())
    {
        while (const auto event = m_window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(m_window, *event);

            if (event->is<sf::Event::Closed>())
                m_window.close();

            if (const auto* e = event->getIf<sf::Event::Resized>())
                handleResize(e->size.x, e->size.y);

            if (const auto* e = event->getIf<sf::Event::MouseMoved>())
                handleMouseMove(e->position);

            if (!ImGui::GetIO().WantCaptureMouse)
            {
                if (const auto* e = event->getIf<sf::Event::MouseWheelScrolled>())
                    handleScroll(e->delta, e->position);

                if (const auto* e = event->getIf<sf::Event::MouseButtonPressed>())
                    if (e->button == sf::Mouse::Button::Right)
                        handleDragBegin(e->position);

                if (const auto* e = event->getIf<sf::Event::MouseButtonReleased>())
                    if (e->button == sf::Mouse::Button::Right)
                        handleDragEnd();
            }
        }

        ImGui::SFML::Update(m_window, m_clock.restart());
        renderUI();

        m_window.clear(sf::Color(15, 15, 15));
        m_window.setView(m_worldView);
        m_world.draw(m_window);
        m_window.setView(m_window.getDefaultView());
        ImGui::SFML::Render(m_window);
        m_window.display();
    }
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void Game::renderUI()
{
    const float winW  = static_cast<float>(m_window.getSize().x);
    const float winH  = static_cast<float>(m_window.getSize().y);
    const float panelW = winW * PANEL_W;
    const float panelX = winW * (1.f - PANEL_W);

    ImGui::SetNextWindowPos(ImVec2(panelX, 0.f),    ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, winH),  ImGuiCond_Always);
    ImGui::Begin("##panel", nullptr,
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoCollapse   |
        ImGuiWindowFlags_NoTitleBar);

    // ---- Generation --------------------------------------------------------
    ImGui::SeparatorText("Generation");

    ImGui::Text("Seed");
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##seed", m_seedBuffer, sizeof(m_seedBuffer),
        ImGuiInputTextFlags_CharsDecimal);

    if (ImGui::Button("Regenerate", ImVec2(-1.f, 0.f)))
    {
        m_seed = (m_seedBuffer[0] == '\0')
            ? std::random_device{}()
            : static_cast<unsigned int>(std::stoul(m_seedBuffer));
        std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_seed);
        m_world.generate(m_seed);
    }

    if (ImGui::Button("Random", ImVec2(-1.f, 0.f)))
    {
        m_seed = std::random_device{}();
        std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_seed);
        m_world.generate(m_seed);
    }

    ImGui::Spacing();
    if (ImGui::Button("Export CSV", ImVec2(-1.f, 0.f)))
        m_world.exportCSV("E:/Projects/ProjectsVS/MapGenerator/world_export.csv");

    // ---- View --------------------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText("View");
    {
        const bool plates = m_world.getRenderMode() == World::RenderMode::Plates;
        if (ImGui::RadioButton("Plates",    plates))
            m_world.setRenderMode(World::RenderMode::Plates);
        ImGui::SameLine();
        if (ImGui::RadioButton("HeightMap", !plates))
            m_world.setRenderMode(World::RenderMode::HeightMap);
    }

    // ---- Overlays ----------------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText("Overlays");
    ImGui::Checkbox("Boundaries", &m_world.showBoundaries);

    // ---- World stats -------------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText("World");

    const auto& s = m_world.getStats();
    ImGui::Text("Gen time:   %.1f ms",  s.genTimeMs);
    ImGui::Text("Plates:     %d",       s.plateCount);
    ImGui::Text("  Oceanic:  %d",       s.plateOceanic);
    ImGui::Text("  Continen: %d",       s.plateContinental);
    ImGui::Spacing();
    ImGui::Text("Chains:     %d",       s.chainTotal);
    ImGui::Text("  Converg:  %d",       s.chainConvergent);
    ImGui::Text("  Diverg:   %d",       s.chainDivergent);
    ImGui::Text("  Transform:%d",       s.chainTransform);

    // ---- Tile info ---------------------------------------------------------
    ImGui::Spacing();
    ImGui::SeparatorText("Tile");

    const sf::Vector2f worldPos =
        m_window.mapPixelToCoords(m_lastMousePos, m_worldView);
    const auto hover = m_world.getHoverInfo(worldPos);

    if (hover.valid)
    {
        ImGui::Text("Pos:    %d, %d",   hover.tileX, hover.tileY);
        ImGui::Text("Plate:  %d (%s%s)",
            hover.plateId,
            hover.oceanic ? "oceanic" : "continental",
            hover.isPolar ? ", polar" : "");
        ImGui::Text("Elev:   %.3f",     hover.elevation);
    }
    else
    {
        ImGui::TextDisabled("(no tile)");
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

void Game::handleResize(unsigned w, unsigned h)
{
    const float aspect = (w * (1.f - PANEL_W)) / static_cast<float>(h);
    m_worldView.setSize({ m_worldView.getSize().y * aspect, m_worldView.getSize().y });
    m_worldView.setViewport(sf::FloatRect({ 0.f, 0.f }, { 1.f - PANEL_W, 1.f }));
}

void Game::handleScroll(float delta, sf::Vector2i mousePos)
{
    const float factor = 1.f - delta * ZOOM_STEP;
    const float curH   = m_worldView.getSize().y;
    const float newH   = std::clamp(curH * factor,
        m_world.pixelHeight() * ZOOM_MIN,
        m_world.pixelHeight() * ZOOM_MAX);

    const float ratio  = newH / curH;
    const auto  mp     = m_window.mapPixelToCoords(mousePos, m_worldView);
    const auto  center = m_worldView.getCenter();

    m_worldView.setCenter(mp + (center - mp) * ratio);
    m_worldView.setSize(m_worldView.getSize() * ratio);
}

void Game::handleDragBegin(sf::Vector2i pos)
{
    m_dragging       = true;
    m_dragOrigin     = pos;
    m_dragViewOrigin = m_worldView.getCenter();
}

void Game::handleDragEnd()
{
    m_dragging = false;
}

void Game::handleDragMove(sf::Vector2i pos)
{
    if (!m_dragging) return;

    const sf::Vector2i delta = m_dragOrigin - pos;
    const sf::Vector2f scale = m_worldView.getSize();

    m_worldView.setCenter(m_dragViewOrigin + sf::Vector2f(
        delta.x * scale.x / viewportW(),
        delta.y * scale.y / viewportH()));
}

void Game::handleMouseMove(sf::Vector2i pos)
{
    m_lastMousePos = pos;
    if (m_dragging) handleDragMove(pos);
}
