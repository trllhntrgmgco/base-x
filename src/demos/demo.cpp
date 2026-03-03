#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>
#include "BaseX/Engine.h"

static std::atomic<bool> g_running{true};

static void handle_signal(int) {
    g_running.store(false);
}

int main() {
    // Install SIGINT handler (Ctrl+C)
    std::signal(SIGINT, handle_signal);

    std::cout << "BaseX Demo starting...\n";

    basex::Engine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize engine.\n";
        return 1;
    }

    std::cout << "Engine initialized. Running demo. Press Enter or Ctrl+C to exit...\n";

    // Main loop
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (g_running.load() && engine.isRunning()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        engine.update(deltaTime);

        // Simulate frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    engine.shutdown();
    std::cout << "Demo exited.\n";
    return 0;
}
