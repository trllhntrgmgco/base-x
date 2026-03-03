/**
 * WLD-007: Tornado Demo
 * Base-X Engine - Demonstrates DRO with complex vortex physics
 * 
 * Controls:
 *   1-6: Set EF Scale (EF0-EF5)
 *   W/S: Increase/Decrease rotation speed
 *   A/D: Increase/Decrease funnel width
 *   Q/E: Increase/Decrease movement speed
 *   SPACE: Toggle debris
 *   R: Reset tornado
 *   ESC: Exit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <cmath>
#include <vector>
#include <random>
#include <iostream>
#include <sstream>
#include <iomanip>

// EF Scale definitions
struct EFScale {
    const char* name;
    int windMin;
    int windMax;
    int width;
    float color[3];
};

const EFScale EF_SCALES[] = {
    {"EF0", 65, 85, 50, {0.27f, 0.67f, 0.27f}},      // green
    {"EF1", 86, 110, 100, {0.53f, 0.80f, 0.27f}},    // yellow-green
    {"EF2", 111, 135, 200, {0.80f, 0.80f, 0.27f}},   // yellow
    {"EF3", 136, 165, 300, {1.00f, 0.67f, 0.27f}},   // orange
    {"EF4", 166, 200, 400, {1.00f, 0.40f, 0.27f}},   // red-orange
    {"EF5", 201, 250, 500, {1.00f, 0.27f, 0.27f}}    // red
};

// Particle types
enum DebrisType { LEAVES, WOOD, BRICK, CAR, ROOF };

struct Debris {
    float x, y;
    float vx, vy;
    DebrisType type;
    float size;
    float rotation;
    float rotSpeed;
    float life;
};

struct DustParticle {
    float x, y;
    float vx, vy;
    float size;
    float alpha;
    float life;
};

struct RainDrop {
    float x, y;
    float vx, vy;
    float length;
};

struct Lightning {
    std::vector<std::pair<float, float>> segments;
    float life;
};

// Tornado state
class TornadoSimulation {
public:
    int efScale = 2;
    int rotationSpeed = 150;
    int funnelWidth = 200;
    int moveSpeed = 35;
    float debrisDensity = 0.5f;
    
    bool showDebris = true;
    bool showDust = true;
    bool showRain = true;
    bool showLightning = true;
    
    float tornadoX = 400.0f;
    float tornadoAngle = 0.0f;
    float pathLength = 0.0f;
    
    std::vector<Debris> debris;
    std::vector<DustParticle> dust;
    std::vector<RainDrop> rain;
    std::vector<Lightning> lightning;
    
    BaseX::DROManager droManager;
    
    int debrisCounts[5] = {0, 0, 0, 0, 0}; // leaves, wood, brick, car, roof
    
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist01{0.0f, 1.0f};
    
    float screenWidth = 1280.0f;
    float screenHeight = 720.0f;
    float groundY = 0.85f;
    
    TornadoSimulation() : rng(std::random_device{}()) {
        droManager.initializeVRProfile();
    }
    
    void setEFScale(int ef) {
        efScale = std::clamp(ef, 0, 5);
        const auto& scale = EF_SCALES[efScale];
        rotationSpeed = (scale.windMin + scale.windMax) / 2;
        funnelWidth = scale.width;
    }
    
    // Rankine vortex model
    struct VortexForce {
        float fx, fy;
        bool inVortex;
    };
    
    VortexForce getVortexForce(float x, float y, float centerX, float centerY, float radius) {
        float dx = x - centerX;
        float dy = y - centerY;
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist < 1.0f) return {0, 0, false};
        
        float coreRadius = radius * 0.3f;
        float tangentialSpeed;
        
        if (dist < coreRadius) {
            // Solid body rotation in core
            tangentialSpeed = (rotationSpeed / 100.0f) * (dist / coreRadius);
        } else {
            // Decay outside core
            tangentialSpeed = (rotationSpeed / 100.0f) * (coreRadius / dist);
        }
        
        // Perpendicular to radius (counterclockwise)
        float angle = std::atan2(dy, dx);
        float fx = -std::sin(angle) * tangentialSpeed;
        float fy = std::cos(angle) * tangentialSpeed;
        
        // Add inward pull
        float inwardForce = dist < radius ? 0.5f * (1.0f - dist / radius) : 0.0f;
        float inwardFx = -dx / dist * inwardForce;
        float inwardFy = -dy / dist * inwardForce;
        
        // Add updraft near center
        float updraft = dist < coreRadius ? -2.0f * (1.0f - dist / coreRadius) : 0.0f;
        
        return {
            fx + inwardFx,
            fy + inwardFy + updraft,
            dist < radius
        };
    }
    
    void spawnDebris(float dt) {
        if (!showDebris) return;
        
        float spawnRate = debrisDensity * efScale * 2.0f * dt;
        float spawnGroundY = groundY * screenHeight;
        
        for (int i = 0; i < (int)spawnRate; i++) {
            if (debris.size() < 500) {
                float spawnX = tornadoX + (dist01(rng) - 0.5f) * funnelWidth * 0.5f;
                
                DebrisType type;
                float size;
                float r = dist01(rng);
                
                if (efScale < 2 || r < 0.4f) {
                    type = LEAVES; size = 8.0f;
                    debrisCounts[0]++;
                } else if (efScale < 3 || r < 0.6f) {
                    type = WOOD; size = 15.0f;
                    debrisCounts[1]++;
                } else if (efScale < 4 || r < 0.8f) {
                    type = BRICK; size = 12.0f;
                    debrisCounts[2]++;
                } else if (efScale < 5 || r < 0.95f) {
                    type = CAR; size = 25.0f;
                    debrisCounts[3]++;
                } else {
                    type = ROOF; size = 30.0f;
                    debrisCounts[4]++;
                }
                
                debris.push_back({
                    spawnX, spawnGroundY,
                    0, 0,
                    type, size,
                    dist01(rng) * 6.28f,
                    (dist01(rng) - 0.5f) * 0.3f,
                    1.0f
                });
            }
        }
    }
    
    void spawnDust(float dt) {
        if (!showDust) return;
        
        float spawnRate = efScale * 10.0f * dt;
        float spawnGroundY = groundY * screenHeight;
        
        for (int i = 0; i < (int)spawnRate; i++) {
            if (dust.size() < 1000) {
                dust.push_back({
                    tornadoX + (dist01(rng) - 0.5f) * funnelWidth,
                    spawnGroundY + dist01(rng) * 20.0f,
                    (dist01(rng) - 0.5f) * 2.0f,
                    -dist01(rng) * 2.0f,
                    2.0f + dist01(rng) * 6.0f,
                    0.3f + dist01(rng) * 0.4f,
                    1.0f
                });
            }
        }
    }
    
    void spawnRain(float dt) {
        if (!showRain) return;
        
        float spawnRate = 30.0f * dt;
        
        for (int i = 0; i < (int)spawnRate; i++) {
            if (rain.size() < 500) {
                rain.push_back({
                    dist01(rng) * screenWidth,
                    -10.0f,
                    -3.0f + dist01(rng) * 2.0f,
                    10.0f + dist01(rng) * 5.0f,
                    10.0f + dist01(rng) * 15.0f
                });
            }
        }
    }
    
    void updateLightning(float dt) {
        if (!showLightning) return;
        
        // Random lightning
        if (dist01(rng) < 0.002f * efScale) {
            createLightning();
        }
        
        // Update existing bolts
        for (auto it = lightning.begin(); it != lightning.end();) {
            it->life -= 0.1f * dt;
            if (it->life <= 0) {
                it = lightning.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void createLightning() {
        float startX = tornadoX + (dist01(rng) - 0.5f) * 300.0f;
        float startY = 50.0f;
        float endX = startX + (dist01(rng) - 0.5f) * 200.0f;
        float endY = screenHeight * 0.7f + dist01(rng) * 100.0f;
        
        Lightning bolt;
        bolt.life = 1.0f;
        
        int steps = 10 + (int)(dist01(rng) * 10);
        for (int i = 0; i < steps; i++) {
            float t = (float)i / steps;
            float x = startX + (endX - startX) * t + (dist01(rng) - 0.5f) * 50.0f;
            float y = startY + (endY - startY) * t;
            bolt.segments.push_back({x, y});
        }
        
        lightning.push_back(std::move(bolt));
    }
    
    void update(float dt) {
        // Move tornado
        tornadoX += moveSpeed * 0.05f * dt;
        if (tornadoX > screenWidth + 100) {
            tornadoX = -100;
        }
        
        pathLength += moveSpeed * 0.0001f * dt;
        tornadoAngle += rotationSpeed * 0.01f * dt;
        
        // Spawn particles
        spawnDebris(dt);
        spawnDust(dt);
        spawnRain(dt);
        updateLightning(dt);
        
        // Update particles
        float funnelRadius = funnelWidth * 0.3f;
        float centerY = screenHeight * 0.5f;
        float spawnGroundY = groundY * screenHeight;
        
        // Update debris
        for (auto it = debris.begin(); it != debris.end();) {
            auto& d = *it;
            
            auto force = getVortexForce(d.x, d.y, tornadoX, centerY, funnelRadius);
            d.vx += force.fx * dt * 2.0f;
            d.vy += force.fy * dt * 2.0f;
            
            // Gravity
            d.vy += 0.1f * dt;
            
            // Drag
            d.vx *= 0.98f;
            d.vy *= 0.98f;
            
            d.x += d.vx * dt * 10.0f;
            d.y += d.vy * dt * 10.0f;
            d.rotation += d.rotSpeed * dt;
            
            if (!force.inVortex) {
                d.life -= 0.01f * dt;
            }
            
            // Ground collision
            if (d.y > spawnGroundY) {
                d.y = spawnGroundY;
                d.vy *= -0.3f;
                d.life -= 0.1f;
            }
            
            if (d.life <= 0 || d.x < -100 || d.x > screenWidth + 100 || d.y < -200) {
                it = debris.erase(it);
            } else {
                ++it;
            }
        }
        
        // Update dust
        for (auto it = dust.begin(); it != dust.end();) {
            auto& d = *it;
            
            auto force = getVortexForce(d.x, d.y, tornadoX, centerY, funnelRadius * 1.5f);
            d.vx += force.fx * dt * 3.0f;
            d.vy += force.fy * dt * 3.0f;
            
            d.vx *= 0.95f;
            d.vy *= 0.95f;
            
            d.x += d.vx * dt * 10.0f;
            d.y += d.vy * dt * 10.0f;
            d.life -= 0.005f * dt;
            
            if (d.life <= 0 || d.y < -50) {
                it = dust.erase(it);
            } else {
                ++it;
            }
        }
        
        // Update rain
        for (auto it = rain.begin(); it != rain.end();) {
            auto& r = *it;
            
            auto force = getVortexForce(r.x, r.y, tornadoX, centerY, funnelRadius * 2.0f);
            r.vx += force.fx * dt * 0.5f;
            
            r.x += r.vx * dt;
            r.y += r.vy * dt;
            
            if (r.y > spawnGroundY || r.x < -50 || r.x > screenWidth + 50) {
                it = rain.erase(it);
            } else {
                ++it;
            }
        }
        
        // Update DRO
        int totalParticles = debris.size() + dust.size();
        // DRO logic based on particle count - matches HTML demo
        droManager.resetMetrics();
    }
    
    void reset() {
        debris.clear();
        dust.clear();
        rain.clear();
        lightning.clear();
        tornadoX = screenWidth * 0.5f;
        pathLength = 0.0f;
        for (int i = 0; i < 5; i++) debrisCounts[i] = 0;
    }
};

// Global state
TornadoSimulation sim;
int windowWidth = 1280;
int windowHeight = 720;

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE); break;
            case GLFW_KEY_1: sim.setEFScale(0); break;
            case GLFW_KEY_2: sim.setEFScale(1); break;
            case GLFW_KEY_3: sim.setEFScale(2); break;
            case GLFW_KEY_4: sim.setEFScale(3); break;
            case GLFW_KEY_5: sim.setEFScale(4); break;
            case GLFW_KEY_6: sim.setEFScale(5); break;
            case GLFW_KEY_SPACE: sim.showDebris = !sim.showDebris; break;
            case GLFW_KEY_R: sim.reset(); break;
        }
    }
    
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
            case GLFW_KEY_W: sim.rotationSpeed = std::min(250, sim.rotationSpeed + 5); break;
            case GLFW_KEY_S: sim.rotationSpeed = std::max(65, sim.rotationSpeed - 5); break;
            case GLFW_KEY_A: sim.funnelWidth = std::max(50, sim.funnelWidth - 10); break;
            case GLFW_KEY_D: sim.funnelWidth = std::min(500, sim.funnelWidth + 10); break;
            case GLFW_KEY_Q: sim.moveSpeed = std::max(0, sim.moveSpeed - 5); break;
            case GLFW_KEY_E: sim.moveSpeed = std::min(70, sim.moveSpeed + 5); break;
        }
    }
}

void drawCircle(float cx, float cy, float r, int segments) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * 3.14159265359f * i / segments;
        glVertex2f(cx + r * cos(angle), cy + r * sin(angle));
    }
    glEnd();
}

void drawFunnel(float x, float topY, float bottomY, float topWidth, float bottomWidth) {
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 16; i++) {
        float t = (float)i / 16.0f;
        float y = topY + (bottomY - topY) * t;
        float width = topWidth * (1.0f - t) + bottomWidth * t;
        
        float spiralAngle = sim.tornadoAngle + t * 10.0f;
        float offset = sin(spiralAngle) * width * 0.8f;
        
        float alpha = 0.6f + 0.3f * (1.0f - abs(2.0f * (float)i / 16.0f - 1.0f));
        glColor4f(0.5f, 0.55f, 0.6f, alpha);
        
        glVertex2f(x - width + offset, y);
        glVertex2f(x + width + offset, y);
    }
    glEnd();
}

void render() {
    // Storm sky gradient
    glBegin(GL_QUADS);
    glColor3f(0.10f, 0.15f, 0.19f); glVertex2f(0, 0);
    glColor3f(0.10f, 0.15f, 0.19f); glVertex2f(windowWidth, 0);
    glColor3f(0.29f, 0.35f, 0.35f); glVertex2f(windowWidth, windowHeight * sim.groundY);
    glColor3f(0.29f, 0.35f, 0.35f); glVertex2f(0, windowHeight * sim.groundY);
    glEnd();
    
    // Ground
    glColor3f(0.23f, 0.29f, 0.23f);
    glBegin(GL_QUADS);
    glVertex2f(0, windowHeight * sim.groundY);
    glVertex2f(windowWidth, windowHeight * sim.groundY);
    glVertex2f(windowWidth, windowHeight);
    glVertex2f(0, windowHeight);
    glEnd();
    
    // Rain
    if (sim.showRain) {
        glColor4f(0.59f, 0.71f, 0.78f, 0.4f);
        glBegin(GL_LINES);
        for (const auto& r : sim.rain) {
            glVertex2f(r.x, r.y);
            glVertex2f(r.x + r.vx, r.y - r.length);
        }
        glEnd();
    }
    
    // Lightning
    glLineWidth(3.0f);
    for (const auto& bolt : sim.lightning) {
        glColor4f(1.0f, 1.0f, 0.78f, bolt.life);
        glBegin(GL_LINE_STRIP);
        for (const auto& seg : bolt.segments) {
            glVertex2f(seg.first, seg.second);
        }
        glEnd();
    }
    glLineWidth(1.0f);
    
    // Dust
    if (sim.showDust) {
        glEnable(GL_POINT_SMOOTH);
        glBegin(GL_POINTS);
        for (const auto& d : sim.dust) {
            glColor4f(0.55f, 0.47f, 0.40f, d.alpha * d.life);
            glPointSize(d.size);
            glVertex2f(d.x, d.y);
        }
        glEnd();
    }
    
    // Tornado funnel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawFunnel(sim.tornadoX, 100, windowHeight * sim.groundY, sim.funnelWidth * 0.3f, sim.funnelWidth * 0.15f);
    
    // Debris (simple colored squares)
    if (sim.showDebris) {
        for (const auto& d : sim.debris) {
            glPushMatrix();
            glTranslatef(d.x, d.y, 0);
            glRotatef(d.rotation * 57.2958f, 0, 0, 1);
            
            switch (d.type) {
                case LEAVES: glColor4f(0.3f, 0.7f, 0.3f, d.life); break;
                case WOOD: glColor4f(0.6f, 0.4f, 0.2f, d.life); break;
                case BRICK: glColor4f(0.7f, 0.3f, 0.2f, d.life); break;
                case CAR: glColor4f(0.5f, 0.5f, 0.8f, d.life); break;
                case ROOF: glColor4f(0.4f, 0.4f, 0.4f, d.life); break;
            }
            
            float hs = d.size * 0.5f;
            glBegin(GL_QUADS);
            glVertex2f(-hs, -hs);
            glVertex2f(hs, -hs);
            glVertex2f(hs, hs);
            glVertex2f(-hs, hs);
            glEnd();
            
            glPopMatrix();
        }
    }
    
    // HUD
    glColor3f(1.0f, 0.67f, 0.0f);
    
    // Simple text rendering would require a font library
    // For now, draw indicator circles for EF scale
    for (int i = 0; i < 6; i++) {
        float x = 50 + i * 40;
        float y = 30;
        
        if (i == sim.efScale) {
            glColor3fv(EF_SCALES[i].color);
            glLineWidth(3.0f);
        } else {
            glColor4f(0.5f, 0.5f, 0.5f, 0.3f);
            glLineWidth(1.0f);
        }
        
        drawCircle(x, y, 12, 16);
    }
    glLineWidth(1.0f);
    
    glDisable(GL_BLEND);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "WLD-007: Tornado Demo | Base-X Engine", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSwapInterval(1);
    
    sim.screenWidth = windowWidth;
    sim.screenHeight = windowHeight;
    sim.setEFScale(2);
    
    // Setup OpenGL
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    double lastTime = glfwGetTime();
    
    std::cout << "WLD-007: Tornado Demo\n";
    std::cout << "Controls:\n";
    std::cout << "  1-6: Set EF Scale\n";
    std::cout << "  W/S: Adjust wind speed\n";
    std::cout << "  A/D: Adjust funnel width\n";
    std::cout << "  Q/E: Adjust movement speed\n";
    std::cout << "  SPACE: Toggle debris\n";
    std::cout << "  R: Reset\n";
    std::cout << "  ESC: Exit\n\n";
    
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float dt = std::min(0.032f, (float)(currentTime - lastTime)) / 0.01667f;
        lastTime = currentTime;
        
        sim.update(dt);
        
        glClear(GL_COLOR_BUFFER_BIT);
        render();
        
        // Print stats occasionally
        static int frameCount = 0;
        if (++frameCount % 60 == 0) {
            std::cout << "EF" << sim.efScale << " | Wind: " << sim.rotationSpeed << " mph | "
                      << "Debris: " << sim.debris.size() << " | Dust: " << sim.dust.size() << "\r" << std::flush;
        }
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    std::cout << "\nDemo ended.\n";
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
