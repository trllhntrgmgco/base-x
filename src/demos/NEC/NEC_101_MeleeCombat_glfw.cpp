// NEC_101_MeleeCombat_glfw.cpp - VR Melee Combat System
// Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
//
// MELEE COMBAT SYSTEM
// Focus: VR weapon wielding, hit detection, damage calculation
// Weapons: Swords, axes, maces, spears
// Mechanics: Physics-based swings, parrying, combos

#include "BaseX/Engine.h"
#include "BaseX/PhysicsEngine.h"
#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    BaseX::Engine engine;
    if (!engine.init()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }

    std::cout << "=== NECROTOPIA VR - MELEE COMBAT SYSTEM ===" << std::endl;

    // Create test arena
    auto world = engine.GenerateWorld("simple combat arena, stone floor, practice dummies");
    world.SetSize(32, 32);  // Small test area

    // Initialize physics for weapons
    BaseX::PhysicsEngine physics;
    physics.InitBullet();

    // Test weapon: Longsword
    struct Weapon {
        std::string name;
        float damage;
        float weight;
        float reach;
        float swingSpeed;
    };

    Weapon longsword = {
        "Longsword",
        25.0f,      // Base damage
        2.5f,       // kg
        1.2f,       // meters
        1.0f        // Normal swing speed
    };

    std::cout << "\nWeapon: " << longsword.name << std::endl;
    std::cout << "Damage: " << longsword.damage << std::endl;
    std::cout << "Weight: " << longsword.weight << " kg" << std::endl;
    std::cout << "Reach: " << longsword.reach << " m" << std::endl;

    // Spawn practice dummies
    world.AddActor({
        {"type", "practice_dummy"},
        {"health", 100.0f},
        {"position", {16, 16, 0}}
    });

    // Combat mechanics testing
    std::cout << "\n=== Combat Mechanics Test ===" << std::endl;

    // Simulate VR controller swing
    struct SwingData {
        float velocity;      // m/s
        float angle;         // degrees
        bool isBlocking;
    };

    SwingData swing1 = {5.0f, 45.0f, false};
    SwingData swing2 = {2.0f, 90.0f, false};
    SwingData parry = {0.5f, 0.0f, true};

    // Damage calculation: base * velocity multiplier
    auto calculateDamage = [](const Weapon& weapon, const SwingData& swing) {
        if (swing.isBlocking) return 0.0f;
        
        float velocityMod = swing.velocity / 5.0f;  // 5 m/s = 1.0x
        float angleMod = std::abs(std::sin(swing.angle * 3.14159f / 180.0f));
        
        return weapon.damage * velocityMod * angleMod;
    };

    std::cout << "Swing 1 (fast, 45°): " << calculateDamage(longsword, swing1) << " damage" << std::endl;
    std::cout << "Swing 2 (slow, 90°): " << calculateDamage(longsword, swing2) << " damage" << std::endl;
    std::cout << "Parry (blocking): " << calculateDamage(longsword, parry) << " damage" << std::endl;

    // Hit detection test
    std::cout << "\n=== Hit Detection Test ===" << std::endl;
    
    struct HitResult {
        bool hit;
        float distance;
        std::string hitPart;  // head, body, limb
    };

    auto testHit = [&longsword](float distance, const std::string& part) -> HitResult {
        bool hit = distance <= longsword.reach;
        return {hit, distance, part};
    };

    HitResult hit1 = testHit(1.0f, "body");
    HitResult hit2 = testHit(1.5f, "head");
    HitResult hit3 = testHit(0.8f, "limb");

    std::cout << "Target at 1.0m (body): " << (hit1.hit ? "HIT" : "MISS") << std::endl;
    std::cout << "Target at 1.5m (head): " << (hit2.hit ? "HIT" : "MISS") << std::endl;
    std::cout << "Target at 0.8m (limb): " << (hit3.hit ? "HIT" : "MISS") << std::endl;

    // Combo system test
    std::cout << "\n=== Combo System Test ===" << std::endl;
    
    std::vector<std::string> combo1 = {"slash", "slash", "thrust"};
    std::vector<std::string> combo2 = {"overhead", "parry", "riposte"};

    auto executeCombo = [](const std::vector<std::string>& moves) {
        float totalDamage = 0.0f;
        float comboMultiplier = 1.0f;
        
        for (size_t i = 0; i < moves.size(); ++i) {
            float moveDamage = 25.0f;  // Base per move
            totalDamage += moveDamage * comboMultiplier;
            comboMultiplier += 0.1f;  // Each move adds 10% bonus
        }
        
        return totalDamage;
    };

    std::cout << "Combo 1 (slash/slash/thrust): " << executeCombo(combo1) << " total damage" << std::endl;
    std::cout << "Combo 2 (overhead/parry/riposte): " << executeCombo(combo2) << " total damage" << std::endl;

    // Render preview
    GLFWwindow* window = engine.CreatePreviewWindow(1280, 720, "Melee Combat System");
    
    while (!glfwWindowShouldClose(window)) {
        engine.Render(world);
        
        // Visual debug: draw weapon reach sphere
        engine.DrawDebugSphere({16, 16, 1.5f}, longsword.reach, {1.0f, 0.0f, 0.0f});
        
        glfwSwapBuffers(window);
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) break;
    }

    // Export combat data
    std::cout << "\nExporting combat system data..." << std::endl;
    std::string exportPath = "../../export/NecrotopiaVR/Combat.json";
    
    // Create JSON manually (simplified)
    std::ofstream out(exportPath);
    out << "{\n";
    out << "  \"weapon\": \"" << longsword.name << "\",\n";
    out << "  \"damage\": " << longsword.damage << ",\n";
    out << "  \"reach\": " << longsword.reach << ",\n";
    out << "  \"combos\": [\n";
    out << "    [\"slash\", \"slash\", \"thrust\"],\n";
    out << "    [\"overhead\", \"parry\", \"riposte\"]\n";
    out << "  ]\n";
    out << "}\n";
    out.close();

    std::cout << "✓ Combat system exported to: " << exportPath << std::endl;

    engine.cleanup();
    return 0;
}
