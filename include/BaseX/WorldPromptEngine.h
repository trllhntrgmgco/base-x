#ifndef BASEX_WORLD_PROMPT_ENGINE_H
#define BASEX_WORLD_PROMPT_ENGINE_H

/**
 * WorldPromptEngine.h — Natural Language → Procedural World Generation
 *
 * Implements the SBX-001 pipeline:
 *   1. NLP prompt parsing  → keyword extraction (terrain, biome, structure, weather)
 *   2. Heightmap generation → multi-octave noise shaped by terrain keywords
 *   3. Biome painting       → height-based color mapping from biome keywords
 *   4. Structure placement  → valid-terrain placement from structure keywords
 *   5. Weather system       → atmospheric state from weather keywords
 *
 * Integrated with DRO (Dynamic Radix Optimization) to shift precision
 * across subsystems during generation phases.
 */

#include <BaseX/DRO.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <random>
#include <functional>

namespace BaseX {

// ══════════════════════════════════════════════════════════════════════
// CONSTANTS
// ══════════════════════════════════════════════════════════════════════

static constexpr int WPE_GRID_SIZE = 64;
static constexpr int WPE_GRID_SIZE_MAX = 256;
static constexpr int WPE_GRID_CELLS = WPE_GRID_SIZE * WPE_GRID_SIZE;

// ══════════════════════════════════════════════════════════════════════
// PERLIN NOISE — deterministic, tileable, no external dependency
// ══════════════════════════════════════════════════════════════════════

class PerlinNoise {
public:
    PerlinNoise(uint32_t seed = 0) {
        for (int i = 0; i < 256; i++) p[i] = i;
        std::mt19937 rng(seed);
        for (int i = 255; i > 0; i--) {
            int j = rng() % (i + 1);
            std::swap(p[i], p[j]);
        }
        for (int i = 0; i < 256; i++) p[256 + i] = p[i];
    }

    float noise(float x, float y) const {
        int xi = (int)std::floor(x) & 255;
        int yi = (int)std::floor(y) & 255;
        float xf = x - std::floor(x);
        float yf = y - std::floor(y);
        float u = fade(xf), v = fade(yf);
        int aa = p[p[xi] + yi], ab = p[p[xi] + yi + 1];
        int ba = p[p[xi + 1] + yi], bb = p[p[xi + 1] + yi + 1];
        return lerp(v, lerp(u, grad(aa, xf, yf), grad(ba, xf - 1, yf)),
                       lerp(u, grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1)));
    }

    // Multi-octave fractal Brownian motion
    float fbm(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const {
        float sum = 0.0f, amp = 1.0f, freq = 1.0f, maxAmp = 0.0f;
        for (int i = 0; i < octaves; i++) {
            sum += noise(x * freq, y * freq) * amp;
            maxAmp += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return sum / maxAmp;
    }

    // Ridged noise (for mountain ridges, canyon walls)
    float ridged(float x, float y, int octaves, float lacunarity = 2.0f, float gain = 0.5f) const {
        float sum = 0.0f, amp = 1.0f, freq = 1.0f, maxAmp = 0.0f;
        for (int i = 0; i < octaves; i++) {
            float n = 1.0f - std::abs(noise(x * freq, y * freq));
            sum += n * n * amp;
            maxAmp += amp;
            amp *= gain;
            freq *= lacunarity;
        }
        return sum / maxAmp;
    }

private:
    int p[512];
    static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static float lerp(float t, float a, float b) { return a + t * (b - a); }
    static float grad(int hash, float x, float y) {
        int h = hash & 3;
        float u = (h < 2) ? x : y;
        float v = (h < 2) ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }
};

// ══════════════════════════════════════════════════════════════════════
// DATA TYPES
// ══════════════════════════════════════════════════════════════════════

struct TerrainKeyword {
    std::string name;
    float maxHeight;   // 0..1  — peak amplitude
    float roughness;   // 0..1  — noise contribution
};

struct BiomeKeyword {
    std::string name;
    float r, g, b;     // RGB colour 0..1
};

struct StructureInstance {
    std::string type;
    int gx, gy;        // grid position
    float height;       // terrain height at placement
    float size;         // visual scale
    float r, g, b;      // colour
};

struct WeatherState {
    bool hasRain    = false;
    bool hasSnow    = false;
    bool hasStorm   = false;
    bool hasMist    = false;
    bool hasAurora  = false;
    bool hasAsh     = false;
    bool isScorching = false;
    bool isMoonlit  = false;
    bool isDawn     = false;
};

struct ParseResult {
    std::vector<std::string> terrain;
    std::vector<std::string> biome;
    std::vector<std::string> structure;
    std::vector<std::string> weather;
};

struct WorldStats {
    int tileCount     = 0;
    int structCount   = 0;
    int biomeCount    = 0;
    float maxElevation = 0.0f;  // metres
};

// Generation phases (matches HTML demo pipeline)
enum class GenPhase : uint8_t {
    IDLE       = 0,
    HEIGHTMAP  = 1,
    BIOMES     = 2,
    STRUCTURES = 3,
    WEATHER    = 4,
    COMPLETE   = 5
};

// ══════════════════════════════════════════════════════════════════════
// DRO PROFILE — per-subsystem radix allocation per phase
// ══════════════════════════════════════════════════════════════════════

struct DROSubsystemState {
    RadixBase nearPlayer;
    RadixBase genFrontier;
    RadixBase heightmap;
    RadixBase biomePaint;
    RadixBase structures;
    RadixBase atmosphere;
};

inline DROSubsystemState getDROForPhase(GenPhase phase) {
    using RB = RadixBase;
    switch (phase) {
        case GenPhase::HEIGHTMAP:
            return { RB::BASE_32, RB::BASE_64, RB::BASE_64, RB::BASE_8,  RB::BASE_16, RB::BASE_8  };
        case GenPhase::BIOMES:
            return { RB::BASE_32, RB::BASE_32, RB::BASE_32, RB::BASE_64, RB::BASE_16, RB::BASE_8  };
        case GenPhase::STRUCTURES:
            return { RB::BASE_32, RB::BASE_32, RB::BASE_16, RB::BASE_16, RB::BASE_64, RB::BASE_8  };
        case GenPhase::WEATHER:
            return { RB::BASE_32, RB::BASE_16, RB::BASE_16, RB::BASE_16, RB::BASE_32, RB::BASE_32 };
        case GenPhase::COMPLETE:
            return { RB::BASE_64, RB::BASE_32, RB::BASE_32, RB::BASE_32, RB::BASE_32, RB::BASE_8  };
        default: // IDLE
            return { RB::BASE_64, RB::BASE_32, RB::BASE_32, RB::BASE_16, RB::BASE_32, RB::BASE_8  };
    }
}

// ══════════════════════════════════════════════════════════════════════
// NLP PROMPT PARSER
// ══════════════════════════════════════════════════════════════════════

class NLPPromptParser {
public:
    NLPPromptParser() { buildDictionaries(); }

    ParseResult parse(const std::string& prompt) const {
        ParseResult result;
        std::string lower = toLower(prompt);

        for (auto& kv : m_terrainKW)
            if (lower.find(kv.first) != std::string::npos)
                result.terrain.push_back(kv.first);
        for (auto& kv : m_biomeKW)
            if (lower.find(kv.first) != std::string::npos)
                result.biome.push_back(kv.first);
        for (auto& kw : m_structKW)
            if (lower.find(kw) != std::string::npos)
                result.structure.push_back(kw);
        for (auto& kw : m_weatherKW)
            if (lower.find(kw) != std::string::npos)
                result.weather.push_back(kw);

        // Defaults
        if (result.terrain.empty())  result.terrain.push_back("hill");
        if (result.biome.empty())    result.biome.push_back("forest");
        if (result.weather.empty())  result.weather.push_back("clear");

        return result;
    }

    const std::unordered_map<std::string, TerrainKeyword>& terrainDict() const { return m_terrainKW; }
    const std::unordered_map<std::string, BiomeKeyword>&   biomeDict()   const { return m_biomeKW;   }
    const std::vector<std::string>& structureKeywords() const { return m_structKW;   }
    const std::vector<std::string>& weatherKeywords()   const { return m_weatherKW;  }

private:
    std::unordered_map<std::string, TerrainKeyword> m_terrainKW;
    std::unordered_map<std::string, BiomeKeyword>   m_biomeKW;
    std::vector<std::string> m_structKW;
    std::vector<std::string> m_weatherKW;

    void buildDictionaries() {
        // Terrain keywords: name → {maxHeight, roughness}
        m_terrainKW = {
            {"mountain", {"mountain", 0.9f, 0.7f}},  {"hill",     {"hill",     0.5f, 0.4f}},
            {"valley",   {"valley",   0.2f, 0.15f}},  {"island",   {"island",   0.6f, 0.3f}},
            {"flat",     {"flat",     0.1f, 0.05f}},  {"canyon",   {"canyon",   0.8f, 0.6f}},
            {"peak",     {"peak",     1.0f, 0.8f}},   {"cliff",    {"cliff",    0.85f,0.7f}},
            {"plateau",  {"plateau",  0.4f, 0.35f}},  {"ridge",    {"ridge",    0.7f, 0.55f}},
            {"cave",     {"cave",     0.3f, 0.2f}},   {"crater",   {"crater",   0.6f, 0.5f}},
            {"volcano",  {"volcano",  0.95f,0.8f}},   {"mesa",     {"mesa",     0.45f,0.4f}},
            // ── New terrain types ──
            {"dune",     {"dune",     0.35f,0.25f}},  {"swamp",    {"swamp",    0.15f,0.1f}},
            {"tundra",   {"tundra",   0.3f, 0.2f}},   {"ravine",   {"ravine",   0.7f, 0.65f}},
            {"fjord",    {"fjord",    0.75f,0.6f}},   {"atoll",    {"atoll",    0.25f,0.15f}},
            {"badlands", {"badlands", 0.55f,0.5f}},   {"steppe",   {"steppe",   0.2f, 0.12f}},
            {"gorge",    {"gorge",    0.65f,0.55f}},  {"butte",    {"butte",    0.5f, 0.45f}},
            {"archipelago",{"archipelago",0.5f,0.25f}},{"caldera",  {"caldera",  0.7f, 0.6f}},
        };

        // Biome keywords: name → RGB
        auto b = [&](const char* n, float r, float g, float bl) {
            m_biomeKW[n] = {n, r, g, bl};
        };
        b("volcanic",0.80f,0.20f,0.07f); b("obsidian",0.13f,0.13f,0.20f);
        b("lava",    1.00f,0.27f,0.00f); b("ash",     0.33f,0.33f,0.27f);
        b("ice",     0.67f,0.80f,0.93f); b("snow",    0.87f,0.93f,1.00f);
        b("glacial", 0.53f,0.73f,0.87f); b("frozen",  0.60f,0.73f,0.80f);
        b("forest",  0.13f,0.40f,0.20f); b("enchanted",0.27f,0.67f,0.33f);
        b("mushroom",0.53f,0.27f,0.53f); b("fairy",   0.53f,0.80f,0.53f);
        b("desert",  0.80f,0.67f,0.33f); b("sand",    0.87f,0.73f,0.40f);
        b("sandstone",0.80f,0.60f,0.27f);b("oasis",   0.27f,0.67f,0.47f);
        b("floating",0.40f,0.53f,0.80f); b("ocean",   0.13f,0.27f,0.53f);
        b("water",   0.20f,0.40f,0.67f); b("crystal", 0.53f,0.67f,0.87f);
        b("necro",   0.20f,0.27f,0.13f); b("dark",    0.13f,0.13f,0.13f);
        b("tomb",    0.27f,0.20f,0.20f); b("skeletal", 0.53f,0.53f,0.40f);
        b("steam",   0.53f,0.47f,0.27f); b("brass",   0.67f,0.53f,0.20f);
        b("gear",    0.60f,0.53f,0.33f); b("smoke",   0.40f,0.40f,0.40f);
        b("cave",    0.20f,0.20f,0.27f); b("amethyst",0.53f,0.27f,0.67f);
        b("biolum",  0.27f,0.87f,0.67f); b("underground",0.13f,0.13f,0.20f);
        // ── New biome types ──
        b("jungle",  0.07f,0.33f,0.13f); b("tropical", 0.20f,0.53f,0.27f);
        b("savanna", 0.67f,0.60f,0.27f); b("prairie",  0.53f,0.60f,0.27f);
        b("coral",   0.87f,0.47f,0.47f); b("reef",     0.27f,0.60f,0.67f);
        b("tundra",  0.67f,0.73f,0.73f); b("taiga",    0.20f,0.33f,0.27f);
        b("swamp",   0.20f,0.27f,0.13f); b("marsh",    0.33f,0.40f,0.20f);
        b("bamboo",  0.47f,0.60f,0.27f); b("cherry",   0.87f,0.53f,0.60f);
        b("autumn",  0.80f,0.47f,0.13f); b("crimson",  0.67f,0.13f,0.13f);
        b("jade",    0.27f,0.53f,0.40f); b("golden",   0.80f,0.73f,0.27f);
        b("silver",  0.73f,0.73f,0.80f); b("rust",     0.60f,0.33f,0.13f);
        b("moss",    0.33f,0.47f,0.20f); b("lichen",   0.47f,0.53f,0.33f);

        m_structKW = {
            "spire","fortress","cathedral","castle","tower","pyramid",
            "ruins","temple","bridge","dock","throne","tomb","gate",
            "wall","altar","shrine","palace","citadel","lighthouse",
            "windmill","hut","tavern","well","statue","obelisk","arch",
            // ── New structures ──
            "aqueduct","minaret","colosseum","amphitheater","monolith",
            "torii","pagoda","ziggurat","dolmen","barrow","keep",
            "watchtower","cabin","chapel","forge","marketplace"
        };

        m_weatherKW = {
            "storm","rain","snow","mist","fog","aurora","sun","cloud",
            "ash","lightning","wind","clear","overcast","blizzard",
            "scorching","moonlit","twilight","dawn","dusk",
            // ── New weather types ──
            "hail","sandstorm","eclipse","rainbow","tornado",
            "drizzle","hurricane","frost","humid"
        };
    }

    static std::string toLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c){ return (char)std::tolower(c); });
        return out;
    }
};

// ══════════════════════════════════════════════════════════════════════
// HEIGHTMAP GENERATOR
// ══════════════════════════════════════════════════════════════════════

class HeightmapGenerator {
public:
    // Returns grid as row-major float array [GS * GS]
    static std::vector<float> generate(
        const ParseResult& parsed,
        const std::unordered_map<std::string, TerrainKeyword>& terrainDict,
        int gridSize = WPE_GRID_SIZE,
        uint32_t seed = 42)
    {
        float maxH = 0.5f, roughness = 0.4f;
        bool isIsland = false, isVolcano = false, isCanyon = false;
        bool isMesa = false, isDune = false, isSwamp = false;
        bool isArchipelago = false, isCaldera = false;
        bool isRidge = false, isRavine = false, isFjord = false;

        for (auto& tk : parsed.terrain) {
            auto it = terrainDict.find(tk);
            if (it != terrainDict.end()) {
                maxH      = std::max(maxH, it->second.maxHeight);
                roughness = std::max(roughness, it->second.roughness);
            }
            if (tk == "island" || tk == "atoll")  isIsland = true;
            if (tk == "volcano")    isVolcano = true;
            if (tk == "canyon" || tk == "gorge")  isCanyon = true;
            if (tk == "mesa" || tk == "butte")    isMesa = true;
            if (tk == "dune")       isDune = true;
            if (tk == "swamp" || tk == "marsh")   isSwamp = true;
            if (tk == "archipelago") isArchipelago = true;
            if (tk == "caldera")    isCaldera = true;
            if (tk == "ridge")      isRidge = true;
            if (tk == "ravine")     isRavine = true;
            if (tk == "fjord")      isFjord = true;
        }

        const int GS = gridSize;
        const float halfGS = GS / 2.0f;
        const float invGS = 1.0f / (float)GS;
        PerlinNoise perlin(seed);
        PerlinNoise perlin2(seed + 137);  // second noise for variation
        std::vector<float> hmap(GS * GS);

        for (int y = 0; y < GS; y++) {
            for (int x = 0; x < GS; x++) {
                float nx = x * invGS;
                float ny = y * invGS;
                float dx = (x - halfGS) / halfGS;
                float dy = (y - halfGS) / halfGS;
                float distFromCenter = std::sqrt(dx*dx + dy*dy);

                // Base terrain: multi-octave fractal noise
                int octaves = 4 + (int)(roughness * 4);  // 4-8 octaves
                float h = perlin.fbm(nx * 4.0f, ny * 4.0f, octaves, 2.0f, 0.5f);
                h = h * 0.5f + 0.5f;  // normalise to 0..1

                // Add ridged noise for mountain ranges
                if (maxH > 0.6f) {
                    float ridge = perlin.ridged(nx * 3.0f + 7.0f, ny * 3.0f + 3.0f, 4);
                    h = h * 0.6f + ridge * 0.4f;
                }

                // ── Terrain-specific shaping ──

                // Canyon: carve deep linear channels
                if (isCanyon) {
                    float canyonLine = std::abs(perlin2.noise(ny * 2.0f, 0.5f));
                    float carved = std::max(0.0f, 1.0f - canyonLine * 4.0f);
                    h = std::max(h * 0.3f, h - carved * 0.6f);
                }

                // Ridge: emphasise ridged noise along a spine
                if (isRidge) {
                    float spine = perlin.ridged(nx * 2.5f, ny * 2.5f, 5, 2.0f, 0.55f);
                    h = h * 0.4f + spine * 0.6f;
                }

                // Mesa/butte: flatten top, steep sides
                if (isMesa) {
                    float mesaH = 0.55f;
                    if (h > mesaH) h = mesaH + (h - mesaH) * 0.1f;  // flatten top
                    float edge = std::abs(h - mesaH);
                    if (edge < 0.05f) h += 0.02f;  // slight lip at edge
                }

                // Dune: rolling sine waves with drift
                if (isDune) {
                    float dune = std::sin(nx * 12.0f + ny * 3.0f + perlin2.noise(nx * 2.0f, ny * 2.0f) * 4.0f);
                    h = 0.15f + dune * 0.12f + perlin.fbm(nx * 8.0f, ny * 8.0f, 3) * 0.08f;
                }

                // Swamp: very low, flat with subtle noise puddles
                if (isSwamp) {
                    h = 0.12f + perlin.fbm(nx * 6.0f, ny * 6.0f, 3) * 0.1f;
                    float puddle = perlin2.noise(nx * 8.0f, ny * 8.0f);
                    if (puddle < -0.1f) h *= 0.5f;  // low spots = water
                }

                // Ravine: deep V-cuts
                if (isRavine) {
                    float cut = std::abs(perlin2.noise(nx * 1.5f + 3.0f, ny * 3.0f));
                    if (cut < 0.15f) h *= cut / 0.15f * 0.3f;
                }

                // Fjord: canyon + island edge combo
                if (isFjord) {
                    float fjordCut = std::abs(perlin2.noise(ny * 2.5f, nx * 0.5f));
                    if (fjordCut < 0.12f) h *= 0.15f;
                    float edgeFade = 1.0f - distFromCenter * 0.6f;
                    h *= std::clamp(edgeFade, 0.0f, 1.0f);
                }

                // Island shape: fade edges to water
                if (isIsland) {
                    float edgeFade = 1.0f - distFromCenter * 0.85f;
                    edgeFade = std::clamp(edgeFade, 0.0f, 1.0f);
                    edgeFade = edgeFade * edgeFade;  // smoother falloff
                    h *= edgeFade;
                }

                // Archipelago: multiple island blobs
                if (isArchipelago) {
                    float blobs = 0.0f;
                    for (int bi = 0; bi < 5; bi++) {
                        float bx = perlin2.noise((float)bi * 7.3f, 0.0f) * 0.7f;
                        float by = perlin2.noise(0.0f, (float)bi * 11.1f) * 0.7f;
                        float bd = std::sqrt((dx - bx)*(dx - bx) + (dy - by)*(dy - by));
                        blobs += std::max(0.0f, 0.35f - bd);
                    }
                    float mask = std::clamp(blobs * 4.0f, 0.0f, 1.0f);
                    h *= mask;
                }

                // Volcano: central peak with crater
                if (isVolcano) {
                    float cd = distFromCenter;
                    float cone = std::max(0.0f, 1.0f - cd * 2.0f);
                    h += cone * cone * maxH * 0.8f;
                    if (cd < 0.12f) h -= 0.25f;  // crater depression
                    h = std::max(0.0f, h);
                }

                // Caldera: wider, deeper crater
                if (isCaldera) {
                    float cd = distFromCenter;
                    float rim = std::max(0.0f, 1.0f - std::abs(cd - 0.35f) * 6.0f);
                    h += rim * 0.4f;
                    if (cd < 0.25f) h *= cd / 0.25f * 0.3f;
                }

                // Final scaling
                h = std::clamp(h * maxH, 0.0f, 1.0f);
                hmap[y * GS + x] = h;
            }
        }

        return hmap;
    }
};

// ══════════════════════════════════════════════════════════════════════
// BIOME PAINTER
// ══════════════════════════════════════════════════════════════════════

struct BiomeCell {
    float r, g, b;
};

class BiomePainter {
public:
    static std::vector<BiomeCell> paint(
        const std::vector<float>& heightmap,
        const ParseResult& parsed,
        const std::unordered_map<std::string, BiomeKeyword>& biomeDict,
        int gridSize = WPE_GRID_SIZE)
    {
        // Gather biome colours
        std::vector<BiomeCell> palette;
        for (auto& name : parsed.biome) {
            auto it = biomeDict.find(name);
            if (it != biomeDict.end())
                palette.push_back({it->second.r, it->second.g, it->second.b});
        }
        if (palette.empty())
            palette.push_back({0.27f, 0.53f, 0.20f}); // default green

        const int GS = gridSize;
        std::vector<BiomeCell> map(GS * GS);
        PerlinNoise detailNoise(137);  // for biome boundary variation

        // Water, shore, low, mid, high colour zones
        const BiomeCell waterColor  = {0.15f, 0.25f, 0.45f};
        const BiomeCell shoreColor  = {0.65f, 0.60f, 0.45f};
        const BiomeCell snowCap     = {0.90f, 0.92f, 0.95f};

        for (int y = 0; y < GS; y++) {
            for (int x = 0; x < GS; x++) {
                float h = heightmap[y * GS + x];

                // Detail variation to break up biome edges
                float nx = (float)x / GS, ny = (float)y / GS;
                float detail = detailNoise.noise(nx * 8.0f, ny * 8.0f) * 0.15f;

                BiomeCell c;

                if (h < 0.12f) {
                    // Underwater — blend water colour
                    float t = h / 0.12f;
                    c = lerpCell(waterColor, shoreColor, t);
                } else if (h < 0.18f) {
                    // Shore/beach zone
                    float t = (h - 0.12f) / 0.06f;
                    BiomeCell lowBiome = palette[0];
                    c = lerpCell(shoreColor, lowBiome, t);
                } else if (h > 0.85f && palette.size() == 1) {
                    // Snow cap for very high terrain (only if single biome)
                    float t = std::clamp((h - 0.85f) / 0.15f, 0.0f, 1.0f);
                    c = lerpCell(palette[0], snowCap, t);
                } else {
                    // Multi-biome blending by height + noise
                    float blend = h * 2.0f + detail;
                    blend = std::clamp(blend, 0.0f, 1.0f);

                    if (palette.size() == 1) {
                        c = palette[0];
                        // Height-based darkening for depth
                        float darken = 0.7f + h * 0.3f;
                        c.r *= darken; c.g *= darken; c.b *= darken;
                    } else {
                        // Smooth interpolation across palette
                        float fi = blend * (palette.size() - 1);
                        int lo = std::clamp((int)fi, 0, (int)palette.size() - 2);
                        float frac = fi - lo;
                        c = lerpCell(palette[lo], palette[lo + 1], frac);
                    }
                }

                map[y * GS + x] = c;
            }
        }

        return map;
    }

private:
    static BiomeCell lerpCell(const BiomeCell& a, const BiomeCell& b, float t) {
        return {
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t
        };
    }
};

// ══════════════════════════════════════════════════════════════════════
// STRUCTURE PLACER
// ══════════════════════════════════════════════════════════════════════

class StructurePlacer {
public:
    static std::vector<StructureInstance> place(
        const ParseResult& parsed,
        const std::vector<float>& heightmap,
        int gridSize = WPE_GRID_SIZE)
    {
        static const float STRUCT_COLORS[][3] = {
            {0.80f,0.67f,0.40f}, {0.53f,0.53f,0.53f}, {0.67f,0.67f,0.80f},
            {0.80f,0.53f,0.27f}, {0.53f,0.67f,0.87f}, {0.67f,0.47f,0.27f},
            {0.73f,0.73f,0.67f}, {0.40f,0.33f,0.27f}, {0.60f,0.47f,0.60f},
            {0.47f,0.60f,0.47f}
        };
        static const int NUM_COLORS = 10;

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> distPos(2, gridSize - 5);
        std::uniform_real_distribution<float> distSize(8.0f, 20.0f);

        std::vector<StructureInstance> out;

        for (size_t i = 0; i < parsed.structure.size(); i++) {
            bool placed = false;
            for (int attempt = 0; attempt < 50 && !placed; attempt++) {
                int gx = distPos(rng);
                int gy = distPos(rng);
                float h = heightmap[gy * gridSize + gx];
                if (h > 0.15f && h < 0.85f) {
                    int ci = (int)(i % NUM_COLORS);
                    out.push_back({
                        parsed.structure[i], gx, gy, h,
                        distSize(rng),
                        STRUCT_COLORS[ci][0], STRUCT_COLORS[ci][1], STRUCT_COLORS[ci][2]
                    });
                    placed = true;
                }
            }
        }

        return out;
    }
};

// ══════════════════════════════════════════════════════════════════════
// WEATHER SYSTEM
// ══════════════════════════════════════════════════════════════════════

class WeatherSystem {
public:
    static WeatherState resolve(const ParseResult& parsed) {
        WeatherState ws;
        for (auto& w : parsed.weather) {
            if (w == "rain" || w == "drizzle")     ws.hasRain = true;
            if (w == "storm" || w == "hurricane" || w == "tornado") {
                ws.hasRain = true; ws.hasStorm = true;
            }
            if (w == "snow" || w == "blizzard" || w == "frost") ws.hasSnow = true;
            if (w == "hail") { ws.hasSnow = true; ws.hasStorm = true; }
            if (w == "mist" || w == "fog" || w == "humid")      ws.hasMist = true;
            if (w == "aurora")    ws.hasAurora  = true;
            if (w == "ash" || w == "sandstorm")    ws.hasAsh = true;
            if (w == "scorching") ws.isScorching = true;
            if (w == "moonlit" || w == "eclipse")  ws.isMoonlit = true;
            if (w == "dawn" || w == "dusk" || w == "twilight") ws.isDawn = true;
        }
        return ws;
    }

    // Sky gradient colours based on weather
    static void getSkyColors(const WeatherState& ws,
                             float& topR, float& topG, float& topB,
                             float& botR, float& botG, float& botB) {
        if (ws.hasStorm) {
            topR=0.10f; topG=0.10f; topB=0.16f;
            botR=0.16f; botG=0.16f; botB=0.23f;
        } else if (ws.isDawn) {
            topR=0.10f; topG=0.04f; topB=0.16f;
            botR=0.80f; botG=0.40f; botB=0.20f;
        } else if (ws.isMoonlit) {
            topR=0.02f; topG=0.03f; topB=0.09f;
            botR=0.04f; botG=0.06f; botB=0.16f;
        } else if (ws.isScorching) {
            topR=0.16f; topG=0.10f; topB=0.03f;
            botR=0.80f; botG=0.53f; botB=0.20f;
        } else {
            topR=0.04f; topG=0.06f; topB=0.13f;
            botR=0.10f; botG=0.16f; botB=0.27f;
        }
    }
};

// ══════════════════════════════════════════════════════════════════════
// WORLD PROMPT ENGINE — ORCHESTRATOR
// ══════════════════════════════════════════════════════════════════════

class WorldPromptEngine {
public:
    WorldPromptEngine() = default;

    // Preset prompts (same as HTML demo)
    static const char* preset(int index) {
        static const char* PRESETS[] = {
            "A volcanic island with obsidian beaches, lava flows, and a crystal spire at the center surrounded by ash clouds",
            "Frozen mountain peaks with glacial valleys, ice caves, an ancient stone fortress, and aurora borealis overhead",
            "A dense enchanted forest with glowing mushrooms, a fairy circle clearing, twisted ancient trees, and soft mist",
            "Vast desert with sandstone ruins, a buried pyramid half-exposed, oasis with palms, and a scorching sun",
            "Floating islands connected by rope bridges above an endless ocean, waterfalls cascading into the void below",
            "A crumbling necropolis of dark stone tombs, skeletal trees, rivers of green mist, and a towering black cathedral",
            "Steampunk city with brass towers, gear-driven bridges, smoke stacks, airship docks, and a clockwork cathedral",
            "Underground crystal caverns with bioluminescent pools, amethyst pillars, a crystal throne, and dripping stalactites",
            "A bamboo jungle on a misty ridge with a jade pagoda, a torii gate at the peak, cherry blossoms, and drizzle",
            "Crimson badlands under an eclipse with rust monoliths, a crumbling colosseum, and a sandstorm on the horizon",
            "A tundra archipelago of frost-covered atolls with moss and lichen, a lonely lighthouse, and aurora overhead",
            "A savanna caldera with golden grass, acacia-like dolmen rings, a forge at the center, and humid twilight haze",
        };
        return PRESETS[std::clamp(index, 0, (int)PRESET_COUNT - 1)];
    }
    static constexpr int PRESET_COUNT = 12;

    // Start asynchronous generation from a text prompt
    void beginGeneration(const std::string& prompt) {
        m_parsed   = m_parser.parse(prompt);
        m_phase    = GenPhase::HEIGHTMAP;
        m_phasePct = 0.0f;
        m_heightmap.clear();
        m_biomeMap.clear();
        m_structures.clear();
        m_stats = {};
        m_generationCount++;
    }

    // Advance generation by dt seconds.  Returns true while still generating.
    bool tick(float dt) {
        if (m_phase == GenPhase::IDLE || m_phase == GenPhase::COMPLETE)
            return false;

        float speed = (m_phase <= GenPhase::BIOMES) ? 80.0f : 50.0f;
        m_phasePct += dt * speed;

        if (m_phasePct >= 100.0f) {
            m_phasePct = 100.0f;
            advancePhase();
        }

        return m_phase != GenPhase::COMPLETE;
    }

    // Accessors
    GenPhase              phase()      const { return m_phase;      }
    float                 phasePct()   const { return m_phasePct;   }
    const ParseResult&    parsed()     const { return m_parsed;     }
    const WorldStats&     stats()      const { return m_stats;      }
    const WeatherState&   weather()    const { return m_weather;    }
    int                   genCount()   const { return m_generationCount; }

    const std::vector<float>&             heightmap()  const { return m_heightmap;  }
    const std::vector<BiomeCell>&         biomeMap()   const { return m_biomeMap;   }
    const std::vector<StructureInstance>& structures() const { return m_structures; }

    DROSubsystemState currentDRO() const { return getDROForPhase(m_phase); }

    const NLPPromptParser& parser() const { return m_parser; }

    void reset() {
        m_phase = GenPhase::IDLE;
        m_phasePct = 0.0f;
        m_heightmap.clear();
        m_biomeMap.clear();
        m_structures.clear();
        m_weather = {};
        m_parsed  = {};
        m_stats   = {};
    }

private:
    NLPPromptParser m_parser;
    ParseResult     m_parsed;
    GenPhase        m_phase    = GenPhase::IDLE;
    float           m_phasePct = 0.0f;

    std::vector<float>             m_heightmap;
    std::vector<BiomeCell>         m_biomeMap;
    std::vector<StructureInstance> m_structures;
    WeatherState                   m_weather;
    WorldStats                     m_stats;
    int                            m_generationCount = 0;

    void advancePhase() {
        switch (m_phase) {
            case GenPhase::HEIGHTMAP:
                m_heightmap = HeightmapGenerator::generate(m_parsed, m_parser.terrainDict());
                m_stats.tileCount = WPE_GRID_CELLS;
                m_stats.maxElevation = 0.0f;
                for (float h : m_heightmap)
                    m_stats.maxElevation = std::max(m_stats.maxElevation, h);
                m_stats.maxElevation *= 3200.0f; // convert to metres
                m_phase = GenPhase::BIOMES;
                m_phasePct = 0.0f;
                break;

            case GenPhase::BIOMES:
                m_biomeMap = BiomePainter::paint(m_heightmap, m_parsed, m_parser.biomeDict());
                m_stats.biomeCount = (int)m_parsed.biome.size();
                m_phase = GenPhase::STRUCTURES;
                m_phasePct = 0.0f;
                break;

            case GenPhase::STRUCTURES:
                m_structures = StructurePlacer::place(m_parsed, m_heightmap);
                m_stats.structCount = (int)m_structures.size();
                m_phase = GenPhase::WEATHER;
                m_phasePct = 0.0f;
                break;

            case GenPhase::WEATHER:
                m_weather = WeatherSystem::resolve(m_parsed);
                m_phase = GenPhase::COMPLETE;
                m_phasePct = 100.0f;
                break;

            default:
                break;
        }
    }
};

} // namespace BaseX

#endif // BASEX_WORLD_PROMPT_ENGINE_H
