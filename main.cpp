#include "BaseX/Engine.h"
#include "BaseX/Demos/PrecisionTest.h"
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    std::cout << "=== Base-X Engine Demo Suite ===" << std::endl;
    std::cout << "Dynamic Radix Optimization Technology" << std::endl;
    std::cout << "Base-4 to Base-64 Adaptive Precision" << std::endl;
    std::cout << "=====================================" << std::endl << std::endl;

    // Create engine
    auto engine = std::make_unique<BaseX::Engine>();

    // Configure engine
    BaseX::EngineConfig config;
    config.appName = "Base-X Demo Suite";
    config.windowWidth = 1920;
    config.windowHeight = 1080;
    config.enableVR = true;
    config.enableValidation = true;
    config.databasePath = "data/basex.db";
    config.useDRO = true;
    config.useVRProfile = true;

    // Initialize engine
    if (!engine->initialize(config)) {
        std::cerr << "Failed to initialize Base-X Engine!" << std::endl;
        return -1;
    }

    std::cout << "Engine initialized successfully" << std::endl;
    std::cout << "DRO System: " << (config.useDRO ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "VR Profile: " << (config.useVRProfile ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << std::endl;

    // Register demos
    engine->registerDemo(std::make_unique<BaseX::Demos::PrecisionTestDemo>());
    
    // You can add more demos here:
    // engine->registerDemo(std::make_unique<BaseX::Demos::VRHandTrackingDemo>());
    // engine->registerDemo(std::make_unique<BaseX::Demos::BiblicalSimulationDemo>());
    // engine->registerDemo(std::make_unique<BaseX::Demos::SpacePhysicsDemo>());

    // List available demos
    const auto& demoNames = engine->getDemoNames();
    std::cout << "Available demos (" << demoNames.size() << "):" << std::endl;
    for (size_t i = 0; i < demoNames.size(); ++i) {
        std::cout << "  [" << i << "] " << demoNames[i] << std::endl;
    }
    std::cout << std::endl;

    // Select demo
    int demoIndex = 0;
    if (argc > 1) {
        demoIndex = std::atoi(argv[1]);
    }

    if (demoIndex >= 0 && demoIndex < demoNames.size()) {
        std::cout << "Starting demo: " << demoNames[demoIndex] << std::endl << std::endl;
        engine->setActiveDemo(demoNames[demoIndex]);
        
        // Run engine
        engine->run();
    } else {
        std::cerr << "Invalid demo index: " << demoIndex << std::endl;
        return -1;
    }

    // Shutdown
    engine->shutdown();
    
    std::cout << "\n=== Engine Statistics ===" << std::endl;
    std::cout << "Total frames: " << engine->getFrameCount() << std::endl;
    std::cout << "Average FPS: " << engine->getFPS() << std::endl;
    
    if (engine->getDROManager()) {
        auto metrics = engine->getDROManager()->getMetrics();
        std::cout << "\n=== DRO Metrics ===" << std::endl;
        std::cout << "Radix switches: " << metrics.radixSwitches << std::endl;
        std::cout << "Precision warnings: " << metrics.precisionWarnings << std::endl;
        std::cout << "Average error: " << metrics.averageError << "m" << std::endl;
    }

    return 0;
}
