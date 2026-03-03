#pragma once

#include <memory>
#include <string>

typedef struct VkInstance_T* VkInstance; // Forward declare

namespace basex {

/**
 * @brief Main engine class for the Base-X Game Engine.
 *
 * This class manages the initialization, updating, and shutdown of the engine
 * and its subsystems (e.g., physics, rendering, VR).
 */
class Engine {
public:
    /**
     * @brief Constructor.
     */
    Engine();

    /**
     * @brief Destructor. Automatically shuts down if running.
     */
    ~Engine();

    /**
     * @brief Initialize the engine.
     * @param configPath Path to configuration file (optional).
     * @return True if initialization succeeded, false otherwise.
     */
    bool initialize(const std::string& configPath = "");

    /**
     * @brief Update the engine for one frame.
     * @param deltaTime Time elapsed since last update.
     */
    void update(float deltaTime);

    /**
     * @brief Shutdown the engine.
     */
    void shutdown();

    /**
     * @brief Check if the engine is currently running.
     * @return True if running, false otherwise.
     */
    bool isRunning() const { return m_isRunning; }

private:
    bool m_isRunning; ///< Flag indicating if the engine is running.
    VkInstance m_vkInstance; ///< Vulkan instance handle.
};

} // namespace basex


