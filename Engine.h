#ifndef BASEX_ENGINE_H
#define BASEX_ENGINE_H

#include "BaseX/DRO.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace BaseX {

// Forward declarations
class Renderer;
class VRSystem;
class PhysicsWorld;
class DemoBase;
class DatabaseManager;

// Engine configuration
struct EngineConfig {
    std::string appName = "Base-X Engine";
    uint32_t windowWidth = 1920;
    uint32_t windowHeight = 1080;
    bool enableVR = true;
    bool enableValidation = true;
    std::string databasePath = "data/basex.db";
    
    // DRO Configuration
    bool useDRO = true;
    bool useVRProfile = true;
};

// Engine class - main entry point
class Engine {
public:
    Engine();
    ~Engine();

    // Initialization
    bool initialize(const EngineConfig& config);
    void shutdown();

    // Main loop
    void run();
    bool isRunning() const { return m_running; }
    void stop() { m_running = false; }

    // Demo management
    void registerDemo(std::unique_ptr<DemoBase> demo);
    void setActiveDemo(const std::string& demoName);
    DemoBase* getActiveDemo() const { return m_activeDemo; }
    const std::vector<std::string>& getDemoNames() const { return m_demoNames; }

    // System access
    DROManager* getDROManager() { return m_droManager.get(); }
    LODManager* getLODManager() { return m_lodManager.get(); }
    Renderer* getRenderer() { return m_renderer.get(); }
    VRSystem* getVRSystem() { return m_vrSystem.get(); }
    PhysicsWorld* getPhysicsWorld() { return m_physicsWorld.get(); }
    DatabaseManager* getDatabase() { return m_database.get(); }

    // Frame timing
    float getDeltaTime() const { return m_deltaTime; }
    uint64_t getFrameCount() const { return m_frameCount; }
    float getFPS() const { return m_fps; }

private:
    // Core systems
    std::unique_ptr<DROManager> m_droManager;
    std::unique_ptr<LODManager> m_lodManager;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<VRSystem> m_vrSystem;
    std::unique_ptr<PhysicsWorld> m_physicsWorld;
    std::unique_ptr<DatabaseManager> m_database;

    // Demo system
    std::vector<std::unique_ptr<DemoBase>> m_demos;
    std::vector<std::string> m_demoNames;
    DemoBase* m_activeDemo = nullptr;

    // Engine state
    bool m_running = false;
    EngineConfig m_config;
    
    // Frame timing
    float m_deltaTime = 0.0f;
    uint64_t m_frameCount = 0;
    float m_fps = 0.0f;
    double m_lastFrameTime = 0.0;

    // Private methods
    void update();
    void render();
    void updateFrameTiming();
};

// Base class for all demos
class DemoBase {
public:
    virtual ~DemoBase() = default;

    // Demo lifecycle
    virtual bool initialize(Engine* engine) = 0;
    virtual void shutdown() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;

    // Demo info
    virtual const char* getName() const = 0;
    virtual const char* getDescription() const = 0;
    virtual const char* getCategory() const = 0;

    // Input handling
    virtual void onKeyPress(int key) {}
    virtual void onMouseMove(float x, float y) {}
    virtual void onMouseButton(int button, bool pressed) {}
    virtual void onVRInput(int controller, int button, bool pressed) {}

protected:
    Engine* m_engine = nullptr;
};

} // namespace BaseX

#endif // BASEX_ENGINE_H
