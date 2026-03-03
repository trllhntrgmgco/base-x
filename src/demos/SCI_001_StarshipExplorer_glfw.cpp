/**
 * SCI-001: Starship Explorer — Walkable Sci-Fi Vessel
 * A first-person exploration of a starship interior with automatic doors.
 *
 * Features:
 *   - Multiple decks: Bridge, Engineering, Sick Bay, Crew Quarters, Cargo Bay
 *   - Automatic sliding doors with proximity sensors
 *   - Turbolift system between decks
 *   - Space view through windows
 *   - Futuristic sci-fi aesthetic
 *   - Ambient lighting and panels
 *
 * Controls:
 *   WASD: Walk
 *   Mouse: Look around
 *   Space: Jump
 *   Shift: Run
 *   E: Interact (turbolift, consoles)
 *   1-5: Teleport to deck (Bridge, Engineering, Sick Bay, Quarters, Cargo)
 *   M: Toggle map overlay
 *   Tab: Toggle flashlight
 *   C: Toggle collision debug
 *   ESC: Quit
 */

#include <GLFW/glfw3.h>
#include <BaseX/DRO.h>
#include <BaseX/GLText.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>

static constexpr float PI = 3.14159265f;

// ══════════════════════════════════════════════════════════════════════
// SHIP LAYOUT CONSTANTS
// ══════════════════════════════════════════════════════════════════════

static constexpr float DECK_HEIGHT = 3.0f;
static constexpr float CORRIDOR_WIDTH = 3.0f;
static constexpr float DOOR_WIDTH = 1.5f;
static constexpr float DOOR_HEIGHT = 2.4f;
static constexpr float PLAYER_HEIGHT = 1.7f;
static constexpr float PLAYER_RADIUS = 0.3f;

// ══════════════════════════════════════════════════════════════════════
// ROOM TYPES
// ══════════════════════════════════════════════════════════════════════

enum class RoomType {
    CORRIDOR,
    BRIDGE,
    ENGINEERING,
    SICK_BAY,
    QUARTERS,
    CARGO_BAY,
    TURBOLIFT,
    TRANSPORTER,
    MESS_HALL,
    READY_ROOM
};

struct RoomInfo {
    const char* name;
    float ambientR, ambientG, ambientB;
};

static const RoomInfo ROOM_INFO[] = {
    {"Corridor",     0.6f, 0.65f, 0.7f},
    {"Bridge",       0.5f, 0.6f, 0.8f},
    {"Engineering",  0.8f, 0.5f, 0.4f},
    {"Sick Bay",     0.7f, 0.8f, 0.9f},
    {"Quarters",     0.7f, 0.65f, 0.55f},
    {"Cargo Bay",    0.5f, 0.5f, 0.5f},
    {"Turbolift",    0.6f, 0.6f, 0.7f},
    {"Transporter",  0.5f, 0.7f, 0.8f},
    {"Mess Hall",    0.7f, 0.7f, 0.6f},
    {"Ready Room",   0.6f, 0.55f, 0.5f},
};

// ══════════════════════════════════════════════════════════════════════
// DOOR SYSTEM
// ══════════════════════════════════════════════════════════════════════

struct Door {
    float x, y, z;           // Center position
    float rotation;          // 0 = faces Z, PI/2 = faces X
    float openAmount;        // 0 = closed, 1 = open
    float targetOpen;
    bool locked;
    int connectsRoom1;       // Room indices
    int connectsRoom2;
    
    static constexpr float OPEN_SPEED = 4.0f;
    static constexpr float SENSOR_RANGE = 5.0f;
};

// ══════════════════════════════════════════════════════════════════════
// ROOM STRUCTURE
// ══════════════════════════════════════════════════════════════════════

struct Room {
    RoomType type;
    float x, y, z;           // Center position
    float width, height, depth;
    int deck;
    std::vector<int> doorIndices;
    
    // Special features
    bool hasViewport;
    bool hasTurbolift;
    bool hasConsoles;
};

// ══════════════════════════════════════════════════════════════════════
// CONSOLE / INTERACTIVE
// ══════════════════════════════════════════════════════════════════════

struct Console {
    float x, y, z;
    float rotation;
    const char* label;
    bool active;
    float blinkTimer;
};

// ══════════════════════════════════════════════════════════════════════
// SHIP STATE
// ══════════════════════════════════════════════════════════════════════

static std::vector<Room> g_rooms;
static std::vector<Door> g_doors;
static std::vector<Console> g_consoles;

// Player state
static float g_playerX = 0, g_playerY = PLAYER_HEIGHT, g_playerZ = 0;
static float g_playerVX = 0, g_playerVY = 0, g_playerVZ = 0;
static float g_playerYaw = 0, g_playerPitch = 0;
static bool g_onGround = true;
static int g_currentRoom = -1;
static int g_currentDeck = 1;

// Camera
static bool g_mouseCaptured = true;
static double g_mouseX, g_mouseY;

// UI
static bool g_showMap = false;
static bool g_showDebug = false;
static bool g_flashlight = false;
static std::string g_statusMessage;
static float g_statusTimer = 0;

// Input
static bool g_keys[512] = {};

// Time
static float g_time = 0;

// Alert status
enum class AlertStatus { NORMAL, YELLOW, RED };
static AlertStatus g_alertStatus = AlertStatus::NORMAL;

// ══════════════════════════════════════════════════════════════════════
// SHIP GENERATION
// ══════════════════════════════════════════════════════════════════════

static int addRoom(RoomType type, float x, float z, float w, float d, int deck) {
    Room r;
    r.type = type;
    r.x = x;
    r.y = deck * DECK_HEIGHT;
    r.z = z;
    r.width = w;
    r.height = DECK_HEIGHT;
    r.depth = d;
    r.deck = deck;
    r.hasViewport = false;
    r.hasTurbolift = (type == RoomType::TURBOLIFT);
    r.hasConsoles = (type == RoomType::BRIDGE || type == RoomType::ENGINEERING);
    g_rooms.push_back(r);
    return (int)g_rooms.size() - 1;
}

static int addDoor(float x, float z, float rot, int deck, int room1, int room2) {
    Door d;
    d.x = x;
    d.y = deck * DECK_HEIGHT;
    d.z = z;
    d.rotation = rot;
    d.openAmount = 0;
    d.targetOpen = 0;
    d.locked = false;
    d.connectsRoom1 = room1;
    d.connectsRoom2 = room2;
    g_doors.push_back(d);
    int idx = (int)g_doors.size() - 1;
    
    if (room1 >= 0 && room1 < (int)g_rooms.size()) g_rooms[room1].doorIndices.push_back(idx);
    if (room2 >= 0 && room2 < (int)g_rooms.size()) g_rooms[room2].doorIndices.push_back(idx);
    
    return idx;
}

static void addConsole(float x, float z, float rot, int deck, const char* label) {
    Console c;
    c.x = x;
    c.y = deck * DECK_HEIGHT + 1.0f;
    c.z = z;
    c.rotation = rot;
    c.label = label;
    c.active = true;
    c.blinkTimer = 0;
    g_consoles.push_back(c);
}

static void generateShip() {
    g_rooms.clear();
    g_doors.clear();
    g_consoles.clear();
    
    // ═══════════════════════════════════════════════════════════════════
    // DECK 1: BRIDGE LEVEL
    // ═══════════════════════════════════════════════════════════════════
    
    // Bridge (circular, front of ship)
    int bridge = addRoom(RoomType::BRIDGE, 0, -20, 12, 12, 1);
    g_rooms[bridge].hasViewport = true;
    
    // Ready Room (captain's office)
    int ready = addRoom(RoomType::READY_ROOM, -8, -18, 5, 6, 1);
    g_rooms[ready].hasViewport = true;
    
    // Bridge corridor
    int bridgeCorridor = addRoom(RoomType::CORRIDOR, 0, -8, 4, 8, 1);
    
    // Turbolift
    int turbo1 = addRoom(RoomType::TURBOLIFT, 0, -2, 3, 3, 1);
    
    // Add doors - Deck 1
    addDoor(0, -13, 0, 1, bridge, bridgeCorridor);           // Bridge to corridor
    addDoor(-5.5f, -18, PI/2, 1, bridge, ready);              // Bridge to ready room
    addDoor(0, -4, 0, 1, bridgeCorridor, turbo1);             // Corridor to turbolift
    
    // Bridge consoles
    addConsole(0, -24, 0, 1, "HELM");
    addConsole(-3, -22, 0.3f, 1, "OPS");
    addConsole(3, -22, -0.3f, 1, "TACTICAL");
    addConsole(-5, -18, PI/2, 1, "SCIENCE");
    addConsole(5, -18, -PI/2, 1, "ENGINEERING");
    addConsole(0, -16, PI, 1, "CAPTAIN");
    
    // ═══════════════════════════════════════════════════════════════════
    // DECK 2: CREW LEVEL
    // ═══════════════════════════════════════════════════════════════════
    
    // Main corridor
    int mainCorridor = addRoom(RoomType::CORRIDOR, 0, 0, 3, 40, 2);
    
    // Turbolift (connects to deck 1)
    int turbo2 = addRoom(RoomType::TURBOLIFT, 0, -2, 3, 3, 2);
    
    // Crew quarters (port side)
    int quarters1 = addRoom(RoomType::QUARTERS, -8, -10, 6, 8, 2);
    int quarters2 = addRoom(RoomType::QUARTERS, -8, 2, 6, 8, 2);
    int quarters3 = addRoom(RoomType::QUARTERS, -8, 14, 6, 8, 2);
    
    // Sick Bay (starboard)
    int sickbay = addRoom(RoomType::SICK_BAY, 8, -5, 8, 12, 2);
    
    // Mess Hall
    int mess = addRoom(RoomType::MESS_HALL, 8, 10, 8, 10, 2);
    g_rooms[mess].hasViewport = true;
    
    // Doors - Deck 2
    addDoor(0, -4, 0, 2, mainCorridor, turbo2);
    addDoor(-4, -10, PI/2, 2, mainCorridor, quarters1);
    addDoor(-4, 2, PI/2, 2, mainCorridor, quarters2);
    addDoor(-4, 14, PI/2, 2, mainCorridor, quarters3);
    addDoor(3, -5, PI/2, 2, mainCorridor, sickbay);
    addDoor(3, 10, PI/2, 2, mainCorridor, mess);
    
    // Sick Bay consoles
    addConsole(10, -8, -PI/2, 2, "MEDICAL");
    addConsole(10, -2, -PI/2, 2, "BIOBED 1");
    
    // ═══════════════════════════════════════════════════════════════════
    // DECK 3: ENGINEERING LEVEL
    // ═══════════════════════════════════════════════════════════════════
    
    // Engineering corridor
    int engCorridor = addRoom(RoomType::CORRIDOR, 0, 5, 3, 30, 3);
    
    // Main Engineering
    int engineering = addRoom(RoomType::ENGINEERING, 0, 25, 15, 15, 3);
    
    // Turbolift
    int turbo3 = addRoom(RoomType::TURBOLIFT, 0, -2, 3, 3, 3);
    
    // Cargo Bays
    int cargo1 = addRoom(RoomType::CARGO_BAY, -10, 8, 8, 10, 3);
    int cargo2 = addRoom(RoomType::CARGO_BAY, 10, 8, 8, 10, 3);
    
    // Transporter Room
    int transporter = addRoom(RoomType::TRANSPORTER, -10, 22, 6, 8, 3);
    
    // Doors - Deck 3
    addDoor(0, -1, 0, 3, engCorridor, turbo3);
    addDoor(0, 17, 0, 3, engCorridor, engineering);
    addDoor(-5, 8, PI/2, 3, engCorridor, cargo1);
    addDoor(5, 8, PI/2, 3, engCorridor, cargo2);
    addDoor(-6, 22, PI/2, 3, engCorridor, transporter);
    
    // Engineering consoles
    addConsole(0, 30, PI, 3, "WARP CORE");
    addConsole(-5, 28, PI + 0.5f, 3, "PLASMA");
    addConsole(5, 28, PI - 0.5f, 3, "POWER");
    addConsole(-6, 22, PI, 3, "DILITHIUM");
    addConsole(6, 22, PI, 3, "ANTIMATTER");
    
    // Transporter console
    addConsole(-12, 18, 0, 3, "TRANSPORTER");
}

// ══════════════════════════════════════════════════════════════════════
// COLLISION DETECTION
// ══════════════════════════════════════════════════════════════════════

static bool isInRoom(const Room& r, float x, float y, float z) {
    float halfW = r.width / 2 + 0.1f;
    float halfD = r.depth / 2 + 0.1f;
    return x >= r.x - halfW && x <= r.x + halfW &&
           z >= r.z - halfD && z <= r.z + halfD &&
           y >= r.y && y <= r.y + r.height;
}

// Check if position (x,z) is inside the doorway passage connecting two rooms.
// Instead of a fixed zone around the door, this creates a rectangular corridor
// that bridges the full gap between the two connected rooms.
static bool canPassDoor(const Door& d, float x, float z) {
    if (d.locked) return false;
    if (d.openAmount < 0.3f && d.targetOpen <= 0) return false;

    int r1 = d.connectsRoom1;
    int r2 = d.connectsRoom2;
    if (r1 < 0 || r1 >= (int)g_rooms.size()) return false;
    if (r2 < 0 || r2 >= (int)g_rooms.size()) return false;

    const Room& room1 = g_rooms[r1];
    const Room& room2 = g_rooms[r2];

    bool facesX = fabsf(fabsf(d.rotation) - PI/2) < 0.3f;

    // The passage extends from the nearest wall of room1 to the nearest wall
    // of room2, with a 0.5 margin overlapping into each room so the player
    // can step seamlessly from room → passage → room.
    float halfDoor = DOOR_WIDTH / 2 + 0.3f;

    if (facesX) {
        // Door faces ±X: passage runs along X, narrow along Z
        float r1Edge = (fabsf(room1.x - room1.width/2 - d.x) < fabsf(room1.x + room1.width/2 - d.x))
                     ? (room1.x - room1.width/2) : (room1.x + room1.width/2);
        float r2Edge = (fabsf(room2.x - room2.width/2 - d.x) < fabsf(room2.x + room2.width/2 - d.x))
                     ? (room2.x - room2.width/2) : (room2.x + room2.width/2);

        float xMin = std::min(r1Edge, r2Edge) - 0.5f;
        float xMax = std::max(r1Edge, r2Edge) + 0.5f;

        return x >= xMin && x <= xMax &&
               z >= d.z - halfDoor && z <= d.z + halfDoor;
    } else {
        // Door faces ±Z: passage runs along Z, narrow along X
        float r1Edge = (fabsf(room1.z - room1.depth/2 - d.z) < fabsf(room1.z + room1.depth/2 - d.z))
                     ? (room1.z - room1.depth/2) : (room1.z + room1.depth/2);
        float r2Edge = (fabsf(room2.z - room2.depth/2 - d.z) < fabsf(room2.z + room2.depth/2 - d.z))
                     ? (room2.z - room2.depth/2) : (room2.z + room2.depth/2);

        float zMin = std::min(r1Edge, r2Edge) - 0.5f;
        float zMax = std::max(r1Edge, r2Edge) + 0.5f;

        return z >= zMin && z <= zMax &&
               x >= d.x - halfDoor && x <= d.x + halfDoor;
    }
}

static bool checkCollision(float x, float y, float z, float radius) {
    // Check if position is inside any room (regardless of deck).
    // The previous deck-based filtering was wrong: the player's eye-height Y
    // caused integer division to pick the wrong deck, blocking all movement.
    bool inAnyRoom = false;
    for (const auto& room : g_rooms) {
        if (isInRoom(room, x, y, z)) {
            inAnyRoom = true;
            break;
        }
    }

    if (!inAnyRoom) {
        // Check if in or near a door connecting two rooms
        for (const auto& door : g_doors) {
            // Check Y proximity to door (door.y is room floor level)
            float dy = fabsf(y - door.y - DECK_HEIGHT / 2);
            if (dy > DECK_HEIGHT) continue;
            if (canPassDoor(door, x, z)) {
                return false;  // Can pass through open/opening door
            }
        }
        return true;  // Collision with wall
    }

    // Inside a room — no collision
    return false;
}

static int getCurrentRoom(float x, float y, float z) {
    // Return the smallest room containing the player so that small rooms
    // (turbolifts, transporter) are preferred over large overlapping ones
    // (corridors).
    int bestRoom = -1;
    float bestVolume = 1e18f;
    for (int i = 0; i < (int)g_rooms.size(); i++) {
        if (isInRoom(g_rooms[i], x, y, z)) {
            float vol = g_rooms[i].width * g_rooms[i].depth * g_rooms[i].height;
            if (vol < bestVolume) {
                bestVolume = vol;
                bestRoom = i;
            }
        }
    }
    return bestRoom;
}

// ══════════════════════════════════════════════════════════════════════
// DOOR UPDATE
// ══════════════════════════════════════════════════════════════════════

static void updateDoors(float dt) {
    for (auto& door : g_doors) {
        // Check Y (same deck)
        float dy = fabsf(g_playerY - door.y - DECK_HEIGHT/2);
        if (dy > DECK_HEIGHT || door.locked) {
            door.targetOpen = 0.0f;
        } else {
            // Use axis-aligned proximity matching the doorway passage shape.
            // For facesX doors: check Z proximity to door (narrow axis) + generous X range
            // For facesZ doors: check X proximity to door (narrow axis) + generous Z range
            bool facesX = fabsf(fabsf(door.rotation) - PI/2) < 0.3f;
            bool inRange = false;
            float sensorWidth = DOOR_WIDTH / 2 + 1.5f;  // how close along narrow axis
            float sensorDepth = Door::SENSOR_RANGE;      // how far along approach axis

            if (facesX) {
                float dzDoor = fabsf(g_playerZ - door.z);
                float dxDoor = fabsf(g_playerX - door.x);
                inRange = dzDoor < sensorWidth && dxDoor < sensorDepth;
            } else {
                float dxDoor = fabsf(g_playerX - door.x);
                float dzDoor = fabsf(g_playerZ - door.z);
                inRange = dxDoor < sensorWidth && dzDoor < sensorDepth;
            }

            door.targetOpen = inRange ? 1.0f : 0.0f;
        }

        // Animate door
        if (door.openAmount < door.targetOpen) {
            door.openAmount += Door::OPEN_SPEED * dt;
            if (door.openAmount > door.targetOpen) door.openAmount = door.targetOpen;
        } else if (door.openAmount > door.targetOpen) {
            door.openAmount -= Door::OPEN_SPEED * dt;
            if (door.openAmount < door.targetOpen) door.openAmount = door.targetOpen;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// PLAYER MOVEMENT
// ══════════════════════════════════════════════════════════════════════

static void updatePlayer(float dt) {
    // Look direction
    float lookX = -sinf(g_playerYaw) * cosf(g_playerPitch);
    float lookZ = -cosf(g_playerYaw) * cosf(g_playerPitch);
    float rightX = cosf(g_playerYaw);
    float rightZ = -sinf(g_playerYaw);
    
    // Movement input
    float moveX = 0, moveZ = 0;
    float walkSpeed = g_keys[GLFW_KEY_LEFT_SHIFT] ? 6.0f : 3.0f;
    
    if (g_keys[GLFW_KEY_W]) { moveX += lookX; moveZ += lookZ; }
    if (g_keys[GLFW_KEY_S]) { moveX -= lookX; moveZ -= lookZ; }
    if (g_keys[GLFW_KEY_A]) { moveX -= rightX; moveZ -= rightZ; }
    if (g_keys[GLFW_KEY_D]) { moveX += rightX; moveZ += rightZ; }
    
    // Normalize
    float moveLen = sqrtf(moveX*moveX + moveZ*moveZ);
    if (moveLen > 0) {
        moveX /= moveLen;
        moveZ /= moveLen;
    }
    
    // Apply velocity
    g_playerVX = moveX * walkSpeed;
    g_playerVZ = moveZ * walkSpeed;
    
    // Jump
    if (g_keys[GLFW_KEY_SPACE] && g_onGround) {
        g_playerVY = 5.0f;
        g_onGround = false;
    }
    
    // Gravity
    g_playerVY -= 15.0f * dt;
    
    // Move with collision
    float newX = g_playerX + g_playerVX * dt;
    float newZ = g_playerZ + g_playerVZ * dt;
    float newY = g_playerY + g_playerVY * dt;
    
    // X collision
    if (!checkCollision(newX, g_playerY, g_playerZ, PLAYER_RADIUS)) {
        g_playerX = newX;
    } else {
        g_playerVX = 0;
    }
    
    // Z collision
    if (!checkCollision(g_playerX, g_playerY, newZ, PLAYER_RADIUS)) {
        g_playerZ = newZ;
    } else {
        g_playerVZ = 0;
    }
    
    // Y movement (floor/ceiling based on current deck)
    // Deck N has floor at N * DECK_HEIGHT, ceiling at (N+1) * DECK_HEIGHT
    int deck = g_currentDeck;
    float floorY = deck * DECK_HEIGHT;
    float ceilingY = (deck + 1) * DECK_HEIGHT - 0.1f;
    
    if (newY < floorY + PLAYER_HEIGHT) {
        newY = floorY + PLAYER_HEIGHT;
        g_playerVY = 0;
        g_onGround = true;
    }
    if (newY > ceilingY) {
        newY = ceilingY;
        g_playerVY = 0;
    }
    g_playerY = newY;
    
    // Update current room
    g_currentRoom = getCurrentRoom(g_playerX, g_playerY, g_playerZ);
    if (g_currentRoom >= 0) {
        g_currentDeck = g_rooms[g_currentRoom].deck;
    }
}

// ══════════════════════════════════════════════════════════════════════
// TURBOLIFT INTERACTION
// ══════════════════════════════════════════════════════════════════════

static void useTurbolift(int targetDeck) {
    if (g_currentRoom < 0) return;
    if (g_rooms[g_currentRoom].type != RoomType::TURBOLIFT) return;
    
    // Find turbolift on target deck
    for (const auto& room : g_rooms) {
        if (room.type == RoomType::TURBOLIFT && room.deck == targetDeck) {
            g_playerX = room.x;
            g_playerZ = room.z;
            g_playerY = room.y + PLAYER_HEIGHT + 0.1f;
            g_currentDeck = targetDeck;
            
            g_statusMessage = "Deck " + std::to_string(targetDeck);
            g_statusTimer = 2.0f;
            return;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING HELPERS
// ══════════════════════════════════════════════════════════════════════

static void perspectiveGL(double fov, double asp, double zn, double zf) {
    double f = 1.0 / tan(fov * PI / 360.0);
    double m[16] = {};
    m[0] = f / asp; m[5] = f;
    m[10] = (zf + zn) / (zn - zf); m[11] = -1;
    m[14] = 2 * zf * zn / (zn - zf);
    glMultMatrixd(m);
}

static void setRoomLighting(const Room& room) {
    const RoomInfo& info = ROOM_INFO[(int)room.type];
    float r = info.ambientR, g = info.ambientG, b = info.ambientB;
    
    // Alert status affects lighting
    if (g_alertStatus == AlertStatus::RED) {
        r = 0.8f; g *= 0.3f; b *= 0.3f;
    } else if (g_alertStatus == AlertStatus::YELLOW) {
        r = 0.9f; g = 0.8f; b *= 0.4f;
    }
    
    glColor3f(r, g, b);
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING - ROOMS
// ══════════════════════════════════════════════════════════════════════

static void renderRoom(const Room& room) {
    float x = room.x, y = room.y, z = room.z;
    float hw = room.width / 2, hd = room.depth / 2;
    float h = room.height;
    
    const RoomInfo& info = ROOM_INFO[(int)room.type];
    
    // Floor
    float floorBright = 0.3f;
    glColor3f(info.ambientR * floorBright, info.ambientG * floorBright, info.ambientB * floorBright);
    glBegin(GL_QUADS);
    glVertex3f(x - hw, y + 0.01f, z - hd);
    glVertex3f(x + hw, y + 0.01f, z - hd);
    glVertex3f(x + hw, y + 0.01f, z + hd);
    glVertex3f(x - hw, y + 0.01f, z + hd);
    glEnd();
    
    // Floor grid pattern
    glColor3f(info.ambientR * 0.4f, info.ambientG * 0.4f, info.ambientB * 0.4f);
    glBegin(GL_LINES);
    for (float gx = x - hw; gx <= x + hw; gx += 1.0f) {
        glVertex3f(gx, y + 0.02f, z - hd);
        glVertex3f(gx, y + 0.02f, z + hd);
    }
    for (float gz = z - hd; gz <= z + hd; gz += 1.0f) {
        glVertex3f(x - hw, y + 0.02f, gz);
        glVertex3f(x + hw, y + 0.02f, gz);
    }
    glEnd();
    
    // Ceiling
    float ceilBright = 0.5f;
    glColor3f(info.ambientR * ceilBright, info.ambientG * ceilBright, info.ambientB * ceilBright);
    glBegin(GL_QUADS);
    glVertex3f(x - hw, y + h - 0.01f, z + hd);
    glVertex3f(x + hw, y + h - 0.01f, z + hd);
    glVertex3f(x + hw, y + h - 0.01f, z - hd);
    glVertex3f(x - hw, y + h - 0.01f, z - hd);
    glEnd();
    
    // Ceiling lights
    glColor3f(0.9f, 0.95f, 1.0f);
    for (float lx = x - hw + 1; lx < x + hw; lx += 2.0f) {
        for (float lz = z - hd + 1; lz < z + hd; lz += 3.0f) {
            glBegin(GL_QUADS);
            glVertex3f(lx - 0.3f, y + h - 0.02f, lz - 0.1f);
            glVertex3f(lx + 0.3f, y + h - 0.02f, lz - 0.1f);
            glVertex3f(lx + 0.3f, y + h - 0.02f, lz + 0.1f);
            glVertex3f(lx - 0.3f, y + h - 0.02f, lz + 0.1f);
            glEnd();
        }
    }
    
    // Walls — rendered with doorway cutouts so rooms connect visually
    float wallBright = 0.35f;
    float wr = info.ambientR * wallBright, wg = info.ambientG * wallBright, wb = info.ambientB * wallBright;

    // Collect door positions on each wall of this room.
    // A door is "on" a wall if its center is within a small tolerance of the
    // wall plane AND its rotation matches the wall orientation.
    //   rotation ≈ 0    → door faces ±Z → sits on a Z-constant wall (back/front)
    //   rotation ≈ PI/2 → door faces ±X → sits on an X-constant wall (left/right)

    int roomIdx = (int)(&room - g_rooms.data());

    // For each wall we collect the "position along the wall" of each door
    // Back wall  (-Z): doors where z ≈ room.z - hd, rotation ≈ 0,    pos = door.x
    // Front wall (+Z): doors where z ≈ room.z + hd, rotation ≈ 0,    pos = door.x
    // Left wall  (-X): doors where x ≈ room.x - hw, rotation ≈ PI/2, pos = door.z
    // Right wall (+X): doors where x ≈ room.x + hw, rotation ≈ PI/2, pos = door.z

    struct DoorOnWall { float pos; float halfW; };  // position along wall axis + half-width
    std::vector<DoorOnWall> backDoors, frontDoors, leftDoors, rightDoors;

    float wallTol = 5.0f;  // tolerance for "on this wall" — doors may sit far from narrow corridor walls
    float halfDW = DOOR_WIDTH / 2 + 0.12f;  // default cutout half-width for real doors

    for (int di : room.doorIndices) {
        const Door& d = g_doors[di];
        bool facesZ = fabsf(d.rotation) < 0.3f || fabsf(d.rotation - PI) < 0.3f;
        bool facesX = fabsf(fabsf(d.rotation) - PI/2) < 0.3f;

        if (facesZ) {
            float distBack  = fabsf(d.z - (z - hd));
            float distFront = fabsf(d.z - (z + hd));
            if (distBack < distFront && distBack < wallTol)
                backDoors.push_back({d.x, halfDW});
            else if (distFront < wallTol)
                frontDoors.push_back({d.x, halfDW});
        }
        if (facesX) {
            float distLeft  = fabsf(d.x - (x - hw));
            float distRight = fabsf(d.x - (x + hw));
            if (distLeft < distRight && distLeft < wallTol)
                leftDoors.push_back({d.z, halfDW});
            else if (distRight < wallTol)
                rightDoors.push_back({d.z, halfDW});
        }
    }

    // Helper lambda: render a wall with door cutouts.
    // wallAxis: the coordinate that varies along the wall (x for back/front, z for left/right)
    // wallMin/wallMax: extent of the wall along that axis
    // wallY: floor Y,  wallH: height
    // The wall is at a fixed position on the perpendicular axis.
    // doors: sorted list of door center positions along the wall axis
    auto renderWallWithCutouts = [&](
        float wallMin, float wallMax, float wallY, float wallH,
        std::vector<DoorOnWall>& doors,
        auto emitQuad)
    {
        // Sort doors by position
        std::sort(doors.begin(), doors.end(), [](const DoorOnWall& a, const DoorOnWall& b){ return a.pos < b.pos; });

        // Merge overlapping cutouts into non-overlapping spans
        // (e.g. a narrow door cutout + a wider virtual overlap cutout at the same position)
        std::vector<std::pair<float,float>> gaps;  // merged [left, right] spans
        for (auto& dw : doors) {
            float l = std::max(dw.pos - dw.halfW, wallMin);
            float r = std::min(dw.pos + dw.halfW, wallMax);
            if (!gaps.empty() && l <= gaps.back().second + 0.01f) {
                gaps.back().second = std::max(gaps.back().second, r);  // extend
            } else {
                gaps.push_back({l, r});
            }
        }

        if (gaps.empty()) {
            glColor3f(wr, wg, wb);
            emitQuad(wallMin, wallMax, wallY, wallY + wallH);
            return;
        }

        glColor3f(wr, wg, wb);
        float cursor = wallMin;
        for (auto& [gl, gr] : gaps) {
            if (cursor < gl - 0.01f) {
                emitQuad(cursor, gl, wallY, wallY + wallH);
            }
            cursor = gr;
        }
        if (cursor < wallMax - 0.01f) {
            emitQuad(cursor, wallMax, wallY, wallY + wallH);
        }
    };

    // When a connected room overlaps this room's wall (e.g. turbolift
    // inside a corridor), cut only the OVERLAPPING SECTION of the wall
    // by injecting a virtual cutout sized to the overlap region.
    for (int di : room.doorIndices) {
        const Door& d = g_doors[di];
        int otherIdx = (d.connectsRoom1 == roomIdx) ? d.connectsRoom2 : d.connectsRoom1;
        if (otherIdx < 0 || otherIdx >= (int)g_rooms.size()) continue;
        const Room& other = g_rooms[otherIdx];
        if (other.deck != room.deck) continue;

        float oMinX = other.x - other.width / 2 - 0.1f;
        float oMaxX = other.x + other.width / 2 + 0.1f;
        float oMinZ = other.z - other.depth / 2 - 0.1f;
        float oMaxZ = other.z + other.depth / 2 + 0.1f;

        // Back wall (-Z): if z-hd is inside the other room, cut overlap
        if ((z - hd) > oMinZ && (z - hd) < oMaxZ &&
            oMinX < x + hw && oMaxX > x - hw) {
            float cLeft  = std::max(oMinX, x - hw);
            float cRight = std::min(oMaxX, x + hw);
            backDoors.push_back({(cLeft + cRight) / 2, (cRight - cLeft) / 2 + 0.01f});
        }
        // Front wall (+Z)
        if ((z + hd) > oMinZ && (z + hd) < oMaxZ &&
            oMinX < x + hw && oMaxX > x - hw) {
            float cLeft  = std::max(oMinX, x - hw);
            float cRight = std::min(oMaxX, x + hw);
            frontDoors.push_back({(cLeft + cRight) / 2, (cRight - cLeft) / 2 + 0.01f});
        }
        // Left wall (-X)
        if ((x - hw) > oMinX && (x - hw) < oMaxX &&
            oMinZ < z + hd && oMaxZ > z - hd) {
            float cBot = std::max(oMinZ, z - hd);
            float cTop = std::min(oMaxZ, z + hd);
            leftDoors.push_back({(cBot + cTop) / 2, (cTop - cBot) / 2 + 0.01f});
        }
        // Right wall (+X)
        if ((x + hw) > oMinX && (x + hw) < oMaxX &&
            oMinZ < z + hd && oMaxZ > z - hd) {
            float cBot = std::max(oMinZ, z - hd);
            float cTop = std::min(oMaxZ, z + hd);
            rightDoors.push_back({(cBot + cTop) / 2, (cTop - cBot) / 2 + 0.01f});
        }
    }

    // Back wall (-Z) — quad spans X, wall at z - hd
    renderWallWithCutouts(x - hw, x + hw, y, h, backDoors,
        [&](float segMin, float segMax, float segBot, float segTop) {
            glBegin(GL_QUADS);
            glVertex3f(segMin, segBot, z - hd);
            glVertex3f(segMax, segBot, z - hd);
            glVertex3f(segMax, segTop, z - hd);
            glVertex3f(segMin, segTop, z - hd);
            glEnd();
        });

    // Front wall (+Z) — quad spans X, wall at z + hd
    renderWallWithCutouts(x - hw, x + hw, y, h, frontDoors,
        [&](float segMin, float segMax, float segBot, float segTop) {
            glBegin(GL_QUADS);
            glVertex3f(segMax, segBot, z + hd);
            glVertex3f(segMin, segBot, z + hd);
            glVertex3f(segMin, segTop, z + hd);
            glVertex3f(segMax, segTop, z + hd);
            glEnd();
        });

    // Left wall (-X) — quad spans Z, wall at x - hw
    renderWallWithCutouts(z - hd, z + hd, y, h, leftDoors,
        [&](float segMin, float segMax, float segBot, float segTop) {
            glBegin(GL_QUADS);
            glVertex3f(x - hw, segBot, segMax);
            glVertex3f(x - hw, segBot, segMin);
            glVertex3f(x - hw, segTop, segMin);
            glVertex3f(x - hw, segTop, segMax);
            glEnd();
        });

    // Right wall (+X) — quad spans Z, wall at x + hw
    renderWallWithCutouts(z - hd, z + hd, y, h, rightDoors,
        [&](float segMin, float segMax, float segBot, float segTop) {
            glBegin(GL_QUADS);
            glVertex3f(x + hw, segBot, segMin);
            glVertex3f(x + hw, segBot, segMax);
            glVertex3f(x + hw, segTop, segMax);
            glVertex3f(x + hw, segTop, segMin);
            glEnd();
        });
    
    // Wall accent strips — continuous LED-style lighting with door gaps
    // Two strips: lower at waist height, upper near ceiling trim
    float stripOffset = 0.01f;  // offset from wall surface

    struct StripDef { float yPos; float height; float brightness; };
    StripDef strips[] = {
        { y + 1.0f,       0.05f, 0.7f },   // lower strip
        { y + h - 0.35f,  0.05f, 0.9f },   // upper strip (near ceiling)
    };

    // Helper: render accent strip segments with door gaps
    auto renderStripWithGaps = [&](float wallMin, float wallMax,
        std::vector<DoorOnWall>& doors, auto emitStrip)
    {
        std::sort(doors.begin(), doors.end(), [](const DoorOnWall& a, const DoorOnWall& b){ return a.pos < b.pos; });
        // Merge overlapping gaps
        std::vector<std::pair<float,float>> gaps;
        for (auto& dw : doors) {
            float l = std::max(dw.pos - dw.halfW, wallMin);
            float r = std::min(dw.pos + dw.halfW, wallMax);
            if (!gaps.empty() && l <= gaps.back().second + 0.01f) {
                gaps.back().second = std::max(gaps.back().second, r);
            } else {
                gaps.push_back({l, r});
            }
        }
        if (gaps.empty()) { emitStrip(wallMin, wallMax); return; }
        float cursor = wallMin;
        for (auto& [gl, gr] : gaps) {
            if (cursor < gl - 0.01f) emitStrip(cursor, gl);
            cursor = gr;
        }
        if (cursor < wallMax - 0.01f) emitStrip(cursor, wallMax);
    };

    for (auto& strip : strips) {
        float sY = strip.yPos;
        float sH = strip.height;
        float br = strip.brightness;

        // Glow color — tinted by room ambient
        float gr = info.ambientR * br;
        float gg = info.ambientG * br;
        float gb = info.ambientB * br;

        // Subtle pulse
        float pulse = 0.92f + 0.08f * sinf(g_time * 1.5f + sY * 2.0f);
        gr *= pulse; gg *= pulse; gb *= pulse;

        glColor3f(gr, gg, gb);

        // Back wall strip (-Z)
        renderStripWithGaps(x - hw + 0.05f, x + hw - 0.05f, backDoors,
            [&](float a, float b) {
                glBegin(GL_QUADS);
                glVertex3f(a, sY, z - hd + stripOffset);
                glVertex3f(b, sY, z - hd + stripOffset);
                glVertex3f(b, sY + sH, z - hd + stripOffset);
                glVertex3f(a, sY + sH, z - hd + stripOffset);
                glEnd();
            });
        // Front wall strip (+Z)
        renderStripWithGaps(x - hw + 0.05f, x + hw - 0.05f, frontDoors,
            [&](float a, float b) {
                glBegin(GL_QUADS);
                glVertex3f(b, sY, z + hd - stripOffset);
                glVertex3f(a, sY, z + hd - stripOffset);
                glVertex3f(a, sY + sH, z + hd - stripOffset);
                glVertex3f(b, sY + sH, z + hd - stripOffset);
                glEnd();
            });
        // Left wall strip (-X)
        renderStripWithGaps(z - hd + 0.05f, z + hd - 0.05f, leftDoors,
            [&](float a, float b) {
                glBegin(GL_QUADS);
                glVertex3f(x - hw + stripOffset, sY, b);
                glVertex3f(x - hw + stripOffset, sY, a);
                glVertex3f(x - hw + stripOffset, sY + sH, a);
                glVertex3f(x - hw + stripOffset, sY + sH, b);
                glEnd();
            });
        // Right wall strip (+X)
        renderStripWithGaps(z - hd + 0.05f, z + hd - 0.05f, rightDoors,
            [&](float a, float b) {
                glBegin(GL_QUADS);
                glVertex3f(x + hw - stripOffset, sY, a);
                glVertex3f(x + hw - stripOffset, sY, b);
                glVertex3f(x + hw - stripOffset, sY + sH, b);
                glVertex3f(x + hw - stripOffset, sY + sH, a);
                glEnd();
            });
    }
    
    // Viewport (window) for rooms that have them
    if (room.hasViewport) {
        // Space view!
        glColor3f(0.02f, 0.02f, 0.08f);
        glBegin(GL_QUADS);
        float vy = y + 1.2f;
        float vh = 1.0f;
        glVertex3f(x - hw + 0.02f, vy, z - hd + 1);
        glVertex3f(x - hw + 0.02f, vy, z + hd - 1);
        glVertex3f(x - hw + 0.02f, vy + vh, z + hd - 1);
        glVertex3f(x - hw + 0.02f, vy + vh, z - hd + 1);
        glEnd();
        
        // Stars
        glColor3f(1, 1, 1);
        glPointSize(2);
        glBegin(GL_POINTS);
        for (int i = 0; i < 30; i++) {
            float sz = z - hd + 1 + (i * 0.1f);
            float sy = vy + 0.1f + (i % 7) * 0.12f;
            glVertex3f(x - hw + 0.03f, sy, sz);
        }
        glEnd();
    }
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING - DOORS
// ══════════════════════════════════════════════════════════════════════

static void renderDoor(const Door& door) {
    glPushMatrix();
    glTranslatef(door.x, door.y, door.z);
    glRotatef(door.rotation * 180.0f / PI, 0, 1, 0);
    
    // Door frame
    glColor3f(0.3f, 0.3f, 0.35f);
    float frameW = DOOR_WIDTH / 2 + 0.1f;
    float frameH = DOOR_HEIGHT + 0.1f;
    float frameD = 0.15f;
    
    // Top frame
    glBegin(GL_QUADS);
    glVertex3f(-frameW, DOOR_HEIGHT, -frameD);
    glVertex3f(frameW, DOOR_HEIGHT, -frameD);
    glVertex3f(frameW, frameH, -frameD);
    glVertex3f(-frameW, frameH, -frameD);
    
    glVertex3f(-frameW, DOOR_HEIGHT, frameD);
    glVertex3f(frameW, DOOR_HEIGHT, frameD);
    glVertex3f(frameW, frameH, frameD);
    glVertex3f(-frameW, frameH, frameD);
    glEnd();
    
    // Door panels (slide into walls)
    float panelW = (DOOR_WIDTH / 2) * (1.0f - door.openAmount);
    
    if (panelW > 0.01f) {
        // Left panel
        glColor3f(0.4f, 0.4f, 0.45f);
        glBegin(GL_QUADS);
        float leftEdge = -DOOR_WIDTH / 2 * door.openAmount - panelW;
        glVertex3f(leftEdge, 0.01f, -0.05f);
        glVertex3f(leftEdge + panelW, 0.01f, -0.05f);
        glVertex3f(leftEdge + panelW, DOOR_HEIGHT, -0.05f);
        glVertex3f(leftEdge, DOOR_HEIGHT, -0.05f);
        
        glVertex3f(leftEdge + panelW, 0.01f, 0.05f);
        glVertex3f(leftEdge, 0.01f, 0.05f);
        glVertex3f(leftEdge, DOOR_HEIGHT, 0.05f);
        glVertex3f(leftEdge + panelW, DOOR_HEIGHT, 0.05f);
        glEnd();
        
        // Right panel
        float rightEdge = DOOR_WIDTH / 2 * door.openAmount;
        glBegin(GL_QUADS);
        glVertex3f(rightEdge, 0.01f, -0.05f);
        glVertex3f(rightEdge + panelW, 0.01f, -0.05f);
        glVertex3f(rightEdge + panelW, DOOR_HEIGHT, -0.05f);
        glVertex3f(rightEdge, DOOR_HEIGHT, -0.05f);
        
        glVertex3f(rightEdge + panelW, 0.01f, 0.05f);
        glVertex3f(rightEdge, 0.01f, 0.05f);
        glVertex3f(rightEdge, DOOR_HEIGHT, 0.05f);
        glVertex3f(rightEdge + panelW, DOOR_HEIGHT, 0.05f);
        glEnd();
    }
    
    // Door status light
    if (door.locked) {
        glColor3f(1, 0, 0);
    } else if (door.openAmount > 0.1f) {
        glColor3f(0, 1, 0);
    } else {
        glColor3f(0.5f, 0.5f, 0);
    }
    glBegin(GL_QUADS);
    glVertex3f(-0.1f, DOOR_HEIGHT + 0.02f, 0);
    glVertex3f(0.1f, DOOR_HEIGHT + 0.02f, 0);
    glVertex3f(0.1f, DOOR_HEIGHT + 0.12f, 0);
    glVertex3f(-0.1f, DOOR_HEIGHT + 0.12f, 0);
    glEnd();
    
    glPopMatrix();
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING - DOORWAY CORRIDORS (connecting passages between rooms)
// ══════════════════════════════════════════════════════════════════════

static void renderDoorwayPassage(const Door& door) {
    int r1 = door.connectsRoom1;
    int r2 = door.connectsRoom2;
    if (r1 < 0 || r1 >= (int)g_rooms.size()) return;
    if (r2 < 0 || r2 >= (int)g_rooms.size()) return;

    const Room& room1 = g_rooms[r1];
    const Room& room2 = g_rooms[r2];

    bool facesX = fabsf(fabsf(door.rotation) - PI/2) < 0.3f;
    float halfDoor = DOOR_WIDTH / 2 + 0.12f;  // matches wall cutout halfDW
    float floorY = door.y;
    float ceilY  = door.y + DECK_HEIGHT;

    // Passage color — blend of the two rooms
    const RoomInfo& i1 = ROOM_INFO[(int)room1.type];
    const RoomInfo& i2 = ROOM_INFO[(int)room2.type];
    float pr = (i1.ambientR + i2.ambientR) * 0.15f;
    float pg = (i1.ambientG + i2.ambientG) * 0.15f;
    float pb = (i1.ambientB + i2.ambientB) * 0.15f;

    if (facesX) {
        // Passage runs along X
        float r1Edge = (fabsf(room1.x - room1.width/2 - door.x) < fabsf(room1.x + room1.width/2 - door.x))
                     ? (room1.x - room1.width/2) : (room1.x + room1.width/2);
        float r2Edge = (fabsf(room2.x - room2.width/2 - door.x) < fabsf(room2.x + room2.width/2 - door.x))
                     ? (room2.x - room2.width/2) : (room2.x + room2.width/2);

        float xA = std::min(r1Edge, r2Edge);
        float xB = std::max(r1Edge, r2Edge);
        float zA = door.z - halfDoor;
        float zB = door.z + halfDoor;

        // Floor
        glColor3f(pr, pg, pb);
        glBegin(GL_QUADS);
        glVertex3f(xA, floorY + 0.01f, zA);
        glVertex3f(xB, floorY + 0.01f, zA);
        glVertex3f(xB, floorY + 0.01f, zB);
        glVertex3f(xA, floorY + 0.01f, zB);
        glEnd();

        // Ceiling
        glColor3f(pr * 1.5f, pg * 1.5f, pb * 1.5f);
        glBegin(GL_QUADS);
        glVertex3f(xA, ceilY - 0.01f, zB);
        glVertex3f(xB, ceilY - 0.01f, zB);
        glVertex3f(xB, ceilY - 0.01f, zA);
        glVertex3f(xA, ceilY - 0.01f, zA);
        glEnd();

        // Side walls (along Z = zA and Z = zB)
        glColor3f(pr * 1.2f, pg * 1.2f, pb * 1.2f);
        glBegin(GL_QUADS);
        glVertex3f(xA, floorY, zA); glVertex3f(xB, floorY, zA);
        glVertex3f(xB, ceilY, zA);  glVertex3f(xA, ceilY, zA);

        glVertex3f(xB, floorY, zB); glVertex3f(xA, floorY, zB);
        glVertex3f(xA, ceilY, zB);  glVertex3f(xB, ceilY, zB);
        glEnd();

        // End walls above door frame (fills gap between door top and ceiling)
        float dh = floorY + DOOR_HEIGHT;
        glColor3f(pr * 1.0f, pg * 1.0f, pb * 1.0f);
        glBegin(GL_QUADS);
        // End at xA
        glVertex3f(xA, dh, zA); glVertex3f(xA, dh, zB);
        glVertex3f(xA, ceilY, zB); glVertex3f(xA, ceilY, zA);
        // End at xB
        glVertex3f(xB, dh, zB); glVertex3f(xB, dh, zA);
        glVertex3f(xB, ceilY, zA); glVertex3f(xB, ceilY, zB);
        glEnd();

        // Accent strip
        glColor3f(pr * 2.5f, pg * 2.5f, pb * 2.5f);
        float sY = floorY + 1.5f;
        glBegin(GL_QUADS);
        glVertex3f(xA, sY, zA); glVertex3f(xB, sY, zA);
        glVertex3f(xB, sY + 0.1f, zA); glVertex3f(xA, sY + 0.1f, zA);
        glVertex3f(xB, sY, zB); glVertex3f(xA, sY, zB);
        glVertex3f(xA, sY + 0.1f, zB); glVertex3f(xB, sY + 0.1f, zB);
        glEnd();
    } else {
        // Passage runs along Z
        float r1Edge = (fabsf(room1.z - room1.depth/2 - door.z) < fabsf(room1.z + room1.depth/2 - door.z))
                     ? (room1.z - room1.depth/2) : (room1.z + room1.depth/2);
        float r2Edge = (fabsf(room2.z - room2.depth/2 - door.z) < fabsf(room2.z + room2.depth/2 - door.z))
                     ? (room2.z - room2.depth/2) : (room2.z + room2.depth/2);

        float zA = std::min(r1Edge, r2Edge);
        float zB = std::max(r1Edge, r2Edge);
        float xA = door.x - halfDoor;
        float xB = door.x + halfDoor;

        // Floor
        glColor3f(pr, pg, pb);
        glBegin(GL_QUADS);
        glVertex3f(xA, floorY + 0.01f, zA);
        glVertex3f(xB, floorY + 0.01f, zA);
        glVertex3f(xB, floorY + 0.01f, zB);
        glVertex3f(xA, floorY + 0.01f, zB);
        glEnd();

        // Ceiling
        glColor3f(pr * 1.5f, pg * 1.5f, pb * 1.5f);
        glBegin(GL_QUADS);
        glVertex3f(xA, ceilY - 0.01f, zB);
        glVertex3f(xB, ceilY - 0.01f, zB);
        glVertex3f(xB, ceilY - 0.01f, zA);
        glVertex3f(xA, ceilY - 0.01f, zA);
        glEnd();

        // Side walls (along X = xA and X = xB)
        glColor3f(pr * 1.2f, pg * 1.2f, pb * 1.2f);
        glBegin(GL_QUADS);
        glVertex3f(xA, floorY, zB); glVertex3f(xA, floorY, zA);
        glVertex3f(xA, ceilY, zA);  glVertex3f(xA, ceilY, zB);

        glVertex3f(xB, floorY, zA); glVertex3f(xB, floorY, zB);
        glVertex3f(xB, ceilY, zB);  glVertex3f(xB, ceilY, zA);
        glEnd();

        // End walls above door frame (fills gap between door top and ceiling)
        float dh = floorY + DOOR_HEIGHT;
        glColor3f(pr * 1.0f, pg * 1.0f, pb * 1.0f);
        glBegin(GL_QUADS);
        // End at zA
        glVertex3f(xA, dh, zA); glVertex3f(xB, dh, zA);
        glVertex3f(xB, ceilY, zA); glVertex3f(xA, ceilY, zA);
        // End at zB
        glVertex3f(xB, dh, zB); glVertex3f(xA, dh, zB);
        glVertex3f(xA, ceilY, zB); glVertex3f(xB, ceilY, zB);
        glEnd();

        // Accent strip
        glColor3f(pr * 2.5f, pg * 2.5f, pb * 2.5f);
        float sY = floorY + 1.5f;
        glBegin(GL_QUADS);
        glVertex3f(xA, sY, zA); glVertex3f(xA, sY, zB);
        glVertex3f(xA, sY + 0.1f, zB); glVertex3f(xA, sY + 0.1f, zA);
        glVertex3f(xB, sY, zB); glVertex3f(xB, sY, zA);
        glVertex3f(xB, sY + 0.1f, zA); glVertex3f(xB, sY + 0.1f, zB);
        glEnd();
    }
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING - CONSOLES
// ══════════════════════════════════════════════════════════════════════

static void renderConsole(const Console& console) {
    glPushMatrix();
    glTranslatef(console.x, console.y, console.z);
    glRotatef(console.rotation * 180.0f / PI, 0, 1, 0);
    
    // Console base
    glColor3f(0.25f, 0.25f, 0.3f);
    glBegin(GL_QUADS);
    // Top (angled)
    glVertex3f(-0.4f, 0.3f, -0.3f);
    glVertex3f(0.4f, 0.3f, -0.3f);
    glVertex3f(0.4f, 0.1f, 0.3f);
    glVertex3f(-0.4f, 0.1f, 0.3f);
    glEnd();
    
    // Screen
    float blink = sinf(g_time * 3.0f + console.blinkTimer) * 0.1f + 0.9f;
    if (console.active) {
        glColor3f(0.1f * blink, 0.3f * blink, 0.5f * blink);
    } else {
        glColor3f(0.05f, 0.05f, 0.05f);
    }
    glBegin(GL_QUADS);
    glVertex3f(-0.35f, 0.28f, -0.25f);
    glVertex3f(0.35f, 0.28f, -0.25f);
    glVertex3f(0.35f, 0.12f, 0.25f);
    glVertex3f(-0.35f, 0.12f, 0.25f);
    glEnd();
    
    // Buttons
    if (console.active) {
        for (int i = 0; i < 5; i++) {
            float bx = -0.25f + i * 0.12f;
            float bb = (sinf(g_time * 2.0f + i) > 0.5f) ? 1.0f : 0.6f;
            glColor3f(0.2f * bb, 0.6f * bb, 0.3f * bb);
            glBegin(GL_QUADS);
            glVertex3f(bx, 0.11f, 0.28f);
            glVertex3f(bx + 0.08f, 0.11f, 0.28f);
            glVertex3f(bx + 0.08f, 0.09f, 0.32f);
            glVertex3f(bx, 0.09f, 0.32f);
            glEnd();
        }
    }
    
    glPopMatrix();
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING - SPECIAL FEATURES
// ══════════════════════════════════════════════════════════════════════

static void renderWarpCore(const Room& eng) {
    if (eng.type != RoomType::ENGINEERING) return;
    
    float coreX = eng.x;
    float coreY = eng.y;
    float coreZ = eng.z + eng.depth / 4;
    
    // Core tube
    glColor3f(0.2f, 0.3f, 0.4f);
    int segs = 16;
    float coreR = 0.5f;
    float coreH = DECK_HEIGHT - 0.5f;
    
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segs; i++) {
        float a = i * 2 * PI / segs;
        glVertex3f(coreX + cosf(a) * coreR, coreY + 0.2f, coreZ + sinf(a) * coreR);
        glVertex3f(coreX + cosf(a) * coreR, coreY + coreH, coreZ + sinf(a) * coreR);
    }
    glEnd();
    
    // Glowing plasma
    float pulse = 0.5f + 0.5f * sinf(g_time * 4.0f);
    glColor3f(0.3f + pulse * 0.5f, 0.5f + pulse * 0.3f, 0.9f + pulse * 0.1f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segs; i++) {
        float a = i * 2 * PI / segs;
        glVertex3f(coreX + cosf(a) * coreR * 0.3f, coreY + 0.3f, coreZ + sinf(a) * coreR * 0.3f);
        glVertex3f(coreX + cosf(a) * coreR * 0.3f, coreY + coreH - 0.1f, coreZ + sinf(a) * coreR * 0.3f);
    }
    glEnd();
}

static void renderTransporterPad(const Room& room) {
    if (room.type != RoomType::TRANSPORTER) return;
    
    // 6 pads in a circle
    float padY = room.y + 0.02f;
    for (int i = 0; i < 6; i++) {
        float angle = i * PI / 3;
        float px = room.x + cosf(angle) * 1.5f;
        float pz = room.z + sinf(angle) * 1.5f;
        
        // Pad base
        glColor3f(0.3f, 0.3f, 0.35f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(px, padY, pz);
        for (int j = 0; j <= 12; j++) {
            float a = j * 2 * PI / 12;
            glVertex3f(px + cosf(a) * 0.4f, padY, pz + sinf(a) * 0.4f);
        }
        glEnd();
        
        // Pad ring
        float ringPulse = 0.5f + 0.5f * sinf(g_time * 2.0f + i);
        glColor3f(0.2f * ringPulse, 0.4f * ringPulse, 0.6f * ringPulse);
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j < 12; j++) {
            float a = j * 2 * PI / 12;
            glVertex3f(px + cosf(a) * 0.35f, padY + 0.01f, pz + sinf(a) * 0.35f);
        }
        glEnd();
    }
}

// ══════════════════════════════════════════════════════════════════════
// RENDERING - UI
// ══════════════════════════════════════════════════════════════════════

static void renderUI(int winW, int winH) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    
    char buf[128];
    
    // Crosshair
    glColor3f(0.5f, 0.8f, 1.0f);
    glBegin(GL_LINES);
    glVertex2f(winW/2 - 10, winH/2);
    glVertex2f(winW/2 + 10, winH/2);
    glVertex2f(winW/2, winH/2 - 10);
    glVertex2f(winW/2, winH/2 + 10);
    glEnd();
    
    // Room info panel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0.1f, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(10, winH - 10);
    glVertex2f(200, winH - 10);
    glVertex2f(200, winH - 90);
    glVertex2f(10, winH - 90);
    glEnd();
    
    glColor3f(0.7f, 0.85f, 1.0f);
    float y = winH - 25.0f;
    
    if (g_currentRoom >= 0) {
        const Room& room = g_rooms[g_currentRoom];
        const RoomInfo& info = ROOM_INFO[(int)room.type];
        gltext::draw(info.name, 15, y); y -= 18;
    } else {
        gltext::draw("Unknown Area", 15, y); y -= 18;
    }
    
    snprintf(buf, sizeof(buf), "Deck %d", g_currentDeck);
    gltext::draw(buf, 15, y); y -= 15;
    
    // Alert status
    if (g_alertStatus == AlertStatus::RED) {
        glColor3f(1, 0.2f, 0.2f);
        gltext::draw("RED ALERT", 15, y);
    } else if (g_alertStatus == AlertStatus::YELLOW) {
        glColor3f(1, 1, 0.2f);
        gltext::draw("YELLOW ALERT", 15, y);
    }
    
    // Status message
    if (g_statusTimer > 0) {
        glColor3f(1, 1, 1);
        gltext::drawCentered(g_statusMessage.c_str(), winW / 2.0f, winH / 2.0f + 50, 2.0f);
    }
    
    // Turbolift prompt
    if (g_currentRoom >= 0 && g_rooms[g_currentRoom].type == RoomType::TURBOLIFT) {
        glColor3f(0.5f, 0.8f, 1.0f);
        gltext::drawCentered("Press 1-3 for Deck", winW / 2.0f, 80, 1.5f);
    }
    
    // Controls hint
    glColor3f(0.5f, 0.5f, 0.6f);
    gltext::draw("WASD: Move  Mouse: Look  E: Interact  M: Map  ESC: Quit", 10, 15);
    
    // Map overlay
    if (g_showMap) {
        float mapX = winW - 220, mapY = winH - 220;
        float mapScale = 4.0f;
        
        glColor4f(0, 0, 0.1f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2f(mapX, mapY + 200);
        glVertex2f(mapX + 200, mapY + 200);
        glVertex2f(mapX + 200, mapY);
        glVertex2f(mapX, mapY);
        glEnd();
        
        // Draw rooms on current deck
        for (const auto& room : g_rooms) {
            if (room.deck != g_currentDeck) continue;
            
            float rx = mapX + 100 + room.x * mapScale;
            float ry = mapY + 100 + room.z * mapScale;
            float rw = room.width * mapScale / 2;
            float rd = room.depth * mapScale / 2;
            
            const RoomInfo& info = ROOM_INFO[(int)room.type];
            glColor3f(info.ambientR * 0.5f, info.ambientG * 0.5f, info.ambientB * 0.5f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(rx - rw, ry - rd);
            glVertex2f(rx + rw, ry - rd);
            glVertex2f(rx + rw, ry + rd);
            glVertex2f(rx - rw, ry + rd);
            glEnd();
        }
        
        // Player position
        float px = mapX + 100 + g_playerX * mapScale;
        float py = mapY + 100 + g_playerZ * mapScale;
        glColor3f(0, 1, 0);
        glPointSize(5);
        glBegin(GL_POINTS);
        glVertex2f(px, py);
        glEnd();
        
        // Player direction
        glBegin(GL_LINES);
        glVertex2f(px, py);
        glVertex2f(px - sinf(g_playerYaw) * 10, py - cosf(g_playerYaw) * 10);
        glEnd();
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// ══════════════════════════════════════════════════════════════════════
// INPUT
// ══════════════════════════════════════════════════════════════════════

static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action == GLFW_PRESS) {
        g_keys[key] = true;
        
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
        if (key == GLFW_KEY_M) g_showMap = !g_showMap;
        if (key == GLFW_KEY_TAB) g_flashlight = !g_flashlight;
        if (key == GLFW_KEY_C) g_showDebug = !g_showDebug;
        
        // Deck selection (turbolift or teleport)
        if (key == GLFW_KEY_1) useTurbolift(1);
        if (key == GLFW_KEY_2) useTurbolift(2);
        if (key == GLFW_KEY_3) useTurbolift(3);
        
        // Alert status toggle
        if (key == GLFW_KEY_F1) g_alertStatus = AlertStatus::NORMAL;
        if (key == GLFW_KEY_F2) g_alertStatus = AlertStatus::YELLOW;
        if (key == GLFW_KEY_F3) g_alertStatus = AlertStatus::RED;
        
    } else if (action == GLFW_RELEASE) {
        g_keys[key] = false;
    }
}

static void cursorCallback(GLFWwindow*, double x, double y) {
    static double lastX = x, lastY = y;
    
    if (g_mouseCaptured) {
        double dx = x - lastX;
        double dy = y - lastY;
        
        g_playerYaw -= (float)dx * 0.002f;
        g_playerPitch -= (float)dy * 0.002f;
        g_playerPitch = std::max(-PI/2 + 0.1f, std::min(PI/2 - 0.1f, g_playerPitch));
    }
    
    lastX = x;
    lastY = y;
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        g_mouseCaptured = true;
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        g_mouseCaptured = false;
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

// ══════════════════════════════════════════════════════════════════════
// MAIN
// ══════════════════════════════════════════════════════════════════════

int main() {
    if (!glfwInit()) return 1;
    
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "SCI-001: Starship Explorer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    
    // Generate ship
    generateShip();
    
    // Start position (Bridge)
    g_playerX = 0;
    g_playerZ = -18;
    g_playerY = 1 * DECK_HEIGHT + PLAYER_HEIGHT;
    g_currentDeck = 1;
    g_playerYaw = 0;
    
    glEnable(GL_DEPTH_TEST);
    glEnable(0x809D);  // GL_MULTISAMPLE
    
    double lastTime = glfwGetTime();
    
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = std::min(0.05f, (float)(now - lastTime));
        lastTime = now;
        g_time = (float)now;
        
        glfwPollEvents();
        
        // Update — doors must update before player so collision checks
        // see currently-open doors, not stale state from previous frame
        updateDoors(dt);
        updatePlayer(dt);
        
        if (g_statusTimer > 0) g_statusTimer -= dt;
        
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        
        // Background (space black)
        glClearColor(0.01f, 0.01f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Camera (first person)
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        perspectiveGL(75.0, (double)w / h, 0.05, 100.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // First person view
        glRotatef(-g_playerPitch * 180 / PI, 1, 0, 0);
        glRotatef(-g_playerYaw * 180 / PI, 0, 1, 0);
        glTranslatef(-g_playerX, -g_playerY, -g_playerZ);
        
        // Render ship
        for (const auto& room : g_rooms) {
            renderRoom(room);
            if (room.type == RoomType::ENGINEERING) renderWarpCore(room);
            if (room.type == RoomType::TRANSPORTER) renderTransporterPad(room);
        }
        
        for (const auto& door : g_doors) {
            renderDoorwayPassage(door);
            renderDoor(door);
        }
        
        for (const auto& console : g_consoles) {
            renderConsole(console);
        }
        
        // UI
        renderUI(w, h);
        
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
