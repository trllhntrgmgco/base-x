// NEC_001_Limbo_glfw.cpp - First Circle of Hell
// Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
//
// LIMBO - Circle 1
// Punishment: Eternal longing for divine presence
// Environment: Misty grey plains, ancient ruins, eternal twilight
// Structures: Crumbling castles, forgotten temples, wanderer camps
// Atmosphere: Melancholy, isolation, timeless

#include "BaseX/Engine.h"
#include "BaseX/WorldPromptEngine.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>

int main() {
    // Initialize engine
    BaseX::Engine engine;
    if (!engine.init()) {
        std::cerr << "Failed to initialize Base-X engine" << std::endl;
        return -1;
    }

    std::cout << "=== NECROTOPIA VR - LIMBO (Circle 1) ===" << std::endl;
    std::cout << "Designing realm layout..." << std::endl;

    // World generation prompt
    std::string prompt = "misty grey plains, ancient ruins, eternal twilight, "
                         "sparse trees, crumbling castles, forgotten temples, "
                         "melancholy atmosphere, timeless void";

    // Generate base terrain
    auto world = engine.GenerateWorld(prompt);
    world.SetSize(128, 128);  // Medium realm
    world.SetTerrainType("plains_grey");

    // Apply Limbo biome
    world.SetBiome("limbo_grey", {0, 0}, {128, 128});
    world.SetColorPalette({
        0.5f, 0.5f, 0.5f,  // Grey ground
        0.6f, 0.6f, 0.7f,  // Misty sky
        0.4f, 0.4f, 0.45f  // Shadow tones
    });

    // Place structures (sparse density - few souls reside here)
    std::cout << "Placing structures..." << std::endl;
    
    // Castles - sparse, crumbling
    world.PlaceStructure("castle_ruins", {64, 64}, 0.2f);  // 20% density (sparse)
    
    // Temples - ancient, forgotten
    world.PlaceStructure("temple_ancient", {32, 96}, 0.25f);
    world.PlaceStructure("temple_ancient", {96, 32}, 0.25f);
    
    // Wanderer camps - very sparse
    world.PlaceStructure("camp", {20, 20}, 0.15f);
    world.PlaceStructure("camp", {108, 108}, 0.15f);
    
    // Trees - sparse, dead
    world.PlaceStructure("tree_dead", {50, 80}, 0.3f);

    // Weather - perpetual mist
    world.SetWeather("mist", 0.8f);  // 80% intensity
    world.SetAtmosphere({
        {"fog_density", 0.7f},
        {"light_level", 0.3f},  // Eternal twilight
        {"wind_speed", 0.1f}    // Calm, eerie stillness
    });

    // Demon encounters (minimal - Limbo is not actively punished)
    world.AddEncounter({
        {"type", "wanderer_soul"},
        {"count", 5},
        {"aggression", 0.0f}  // Passive
    });

    // Gameplay elements
    world.AddObjective({
        {"type", "find_portal"},
        {"location", {64, 64}},
        {"description", "Seek the gate to deeper circles"}
    });

    // Preview in GLFW
    std::cout << "Rendering preview..." << std::endl;
    GLFWwindow* window = engine.CreatePreviewWindow(1280, 720, "Limbo - Circle 1");
    
    while (!glfwWindowShouldClose(window)) {
        engine.Render(world);
        glfwSwapBuffers(window);
        glfwPollEvents();

        // ESC to exit
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }
    }

    // Export to JSON for Unreal import
    std::cout << "Exporting to NecrotopiaVR..." << std::endl;
    std::string exportPath = "../../export/NecrotopiaVR/Limbo.json";
    world.ExportToJSON(exportPath);
    
    std::cout << "✓ Limbo exported to: " << exportPath << std::endl;
    std::cout << "✓ Run sync_to_unreal.ps1 to import into Unreal" << std::endl;

    engine.cleanup();
    return 0;
}
