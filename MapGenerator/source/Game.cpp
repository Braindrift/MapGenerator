#include "Game.h"
#include <imgui.h>
#include <imgui-SFML.h>
#include <random>
#include <cstdio>
#include <cstring>

Game::Game()
    : window(sf::VideoMode({ 1200, 800 }), "World"),
      world(512, 256, 8, 0),
      panel(1200.f * MAP_PANEL_RATIO + 8.f, 16.f),
      m_camera(sf::Vector2u{ 1200, 800 }, MAP_PANEL_RATIO),
      m_uiView(sf::FloatRect({ 0.f, 0.f }, { 1200.f, 800.f })),
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

    // Border pick only available in tectonic plate view
    if (world.getRenderMode() == World::RenderMode::TectonicPlates)
    {
        auto borderHit = world.pickBoundary(worldPos);
        if (borderHit)
        {
            m_selectedBoundary = borderHit;
            world.setSelectedChain(borderHit->chainIdx);
            panel.clearSelection();
            return;
        }
        m_selectedBoundary.reset();
        world.setSelectedChain(-1);
    }

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
    ImGui::SetNextWindowSize(ImVec2(panelW, 200.f), ImGuiCond_Always);
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
        m_selectedBoundary.reset();
    }

    if (ImGui::Button("Random", ImVec2(-1.f, 0.f)))
    {
        m_currentSeed = std::random_device{}();
        std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%u", m_currentSeed);
        world.regenerate(m_currentSeed);
        panel.clearSelection();
        m_selectedBoundary.reset();
    }

    ImGui::Separator();
    const bool inHeightMap = (world.getRenderMode() == World::RenderMode::HeightMap);
    if (ImGui::Button(inHeightMap ? "View: Height Map" : "View: Tectonic Plates", ImVec2(-1.f, 0.f)))
        world.setRenderMode(inHeightMap ? World::RenderMode::TectonicPlates : World::RenderMode::HeightMap);

    ImGui::End();

    // --- Legend ---
    const float legendY = 150.f + 200.f + 8.f;
    ImGui::SetNextWindowPos(ImVec2(panelX, legendY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, 110.f), ImGuiCond_Always);
    ImGui::Begin("Boundaries", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse);

    const ImVec2 swatchSize(14.f, 14.f);
    ImGui::ColorButton("##conv", ImVec4(1.f, 0.392f, 0.118f, 1.f),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, swatchSize);
    ImGui::SameLine(); ImGui::Text("Convergent  (collision)");

    ImGui::ColorButton("##div", ImVec4(0.196f, 0.863f, 1.f, 1.f),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, swatchSize);
    ImGui::SameLine(); ImGui::Text("Divergent   (rift)");

    ImGui::ColorButton("##tra", ImVec4(0.863f, 1.f, 0.196f, 1.f),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, swatchSize);
    ImGui::SameLine(); ImGui::Text("Transform   (slide)");

    ImGui::End();

    // --- Border debug info (shown when a boundary is selected) ---
    if (m_selectedBoundary)
    {
        const World::BoundaryDebugInfo& b = *m_selectedBoundary;

        const char* typeName =
            b.type == World::BoundaryType::Convergent ? "Convergent" :
            b.type == World::BoundaryType::Divergent  ? "Divergent"  :
            b.type == World::BoundaryType::Transform  ? "Transform"  : "None";

        const float infoY = legendY + 110.f + 8.f;
        ImGui::SetNextWindowPos(ImVec2(panelX, infoY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelW, 210.f), ImGuiCond_Always);
        ImGui::Begin("Selected Border", nullptr,
            ImGuiWindowFlags_NoMove    |
            ImGuiWindowFlags_NoResize  |
            ImGuiWindowFlags_NoCollapse);

        ImGui::TextColored(ImVec4(1.f, 0.85f, 0.2f, 1.f), "Type: %s", typeName);
        float ratio = (b.dotProduct != 0.f) ? b.slideComponent / std::abs(b.dotProduct) : 999.f;
        ImGui::Text("Perp: %+.3f   Slide: %.3f   Ratio: %.2f", b.dotProduct, b.slideComponent, ratio);
        ImGui::Separator();

        ImGui::Text("Plate %d", b.plateIdA);
        ImGui::Text("  drift (%.2f, %.2f)  bearing %.0f deg", b.driftAX, b.driftAY, b.driftAngleA);
        ImGui::Text("  speed %.2f", b.speedA);

        ImGui::Text("Plate %d", b.plateIdB);
        ImGui::Text("  drift (%.2f, %.2f)  bearing %.0f deg", b.driftBX, b.driftBY, b.driftAngleB);
        ImGui::Text("  speed %.2f", b.speedB);

        ImGui::Separator();
        ImGui::Text("Rel vel  (%.2f, %.2f)", b.relVelX, b.relVelY);
        ImGui::Text("A->B norm(%.2f, %.2f)", b.normalX,  b.normalY);

        if (ImGui::Button("Copy to clipboard", ImVec2(-1.f, 0.f)))
        {
            char buf[512];
            float ratio2 = (b.dotProduct != 0.f) ? b.slideComponent / std::abs(b.dotProduct) : 999.f;
            std::snprintf(buf, sizeof(buf),
                "Type: %s | Perp: %+.3f | Slide: %.3f | Ratio: %.2f\n"
                "Plate %d: drift(%.2f, %.2f) bearing %.0fdeg speed %.2f\n"
                "Plate %d: drift(%.2f, %.2f) bearing %.0fdeg speed %.2f\n"
                "Rel vel: (%.2f, %.2f)\n"
                "A->B normal: (%.2f, %.2f)",
                typeName, b.dotProduct, b.slideComponent, ratio2,
                b.plateIdA, b.driftAX, b.driftAY, b.driftAngleA, b.speedA,
                b.plateIdB, b.driftBX, b.driftBY, b.driftAngleB, b.speedB,
                b.relVelX, b.relVelY,
                b.normalX, b.normalY);
            ImGui::SetClipboardText(buf);
        }

        ImGui::End();
    }
}
