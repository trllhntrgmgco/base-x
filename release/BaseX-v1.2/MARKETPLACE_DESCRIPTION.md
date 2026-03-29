# Base-X World Generation Engine v1.2

**Smart Prompt NLP · Density Control · Exact Counts · Weather Intensity**

**NLP Prompt-Driven Procedural World Generation for Unreal Engine 5.7**

© 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO (Dynamic Radix Optimization), LOD, and Base-X Game Engine are patent pending (USPTO).

---

## Short Description (200 char)

Type a sentence, get a world. Now with smart NLP — say "2 oaks at the center" or "dense forest" and get exact counts, density control, and intensity-driven weather. Patent-pending DRO precision.

## Long Description

### What It Does

Base-X World Generation Engine turns natural language into complete 3D worlds in real-time. Type "a dense enchanted forest with 3 castles at the center, glowing mushrooms, and rolling thunder" and watch it generate — heightmap terrain, biome painting, structure placement, and weather effects — all in seconds, all in-editor.

No node graphs. No Blueprint spaghetti. Just describe what you want.

**New in v1.2:** The prompt engine now understands *how many* and *how dense*. Say "2 trees" and get exactly two. Say "dense forest" and watch the tree count triple. Say "castle at the center" and it lands right where you pointed. Weather keywords now produce graduated intensities — drizzle feels different from monsoon.

### What's New in v1.2

#### Smart Number Parsing
Your words, your counts:
- **"2 oaks"** → exactly 2 oak trees placed
- **"a dozen pines"** → exactly 12 pine trees
- **"three towers on the north edge"** → 3 towers clustered near the north
- **"single lighthouse at the center"** → 1 lighthouse, dead center
- **30 number words** recognized: one through twenty, dozen, hundred, few, many, several, handful, pair, trio...
- **Plural handling**: "trees", "castles", "boulders" auto-matched to keywords
- No number? Default category counts apply (trees: 6-21, rocks: 3-11, buildings: 2-6)

#### Density-Aware Placement
One word changes everything:
- **"dense forest"** → 2× the default tree/plant count
- **"lush tropical island"** → 3× vegetation density
- **"sparse tundra"** → 0.25× — just a few scattered objects
- **"thick jungle"** → 1.5× — noticeably fuller
- Exact counts override density — "2 trees in a dense forest" places exactly 2 trees
- 17 density modifiers recognized across 6 levels

| Keyword | Density |
|---------|---------|
| sparse, barren, desolate, empty | 0.25× |
| scattered, thin | 0.5× |
| *(default — no modifier)* | 1.0× |
| thick, sprawling, vast | 1.5× |
| dense, packed, crowded | 2.0× |
| lush, overgrown, abundant, teeming | 3.0× |

#### Positional Placement
Tell the engine *where*:
- **"castle at the center"** → placed at map center with natural scatter
- **"lighthouse on the edge"** → placed along a random map border
- **"oaks on the north"** → clustered toward the top of the map
- 18 position words mapped to 7 regions: center, north, south, east, west, edge, corner
- Natural jitter prevents grid-aligned placement

#### Intensity-Driven Weather
v1.1 weather was on/off. v1.2 weather is a spectrum:
- **"drizzle"** → rain intensity 0.2 — light misting
- **"rain"** → rain intensity 0.5 — steady downpour
- **"monsoon"** → rain intensity 0.9 — torrential
- **"rain and fog"** → stacked: high rain + low visibility + moderate cloud
- 6 intensity channels: rain, snow, wind, cloud cover, visibility, temperature
- Multiple keywords stack and clamp to 0..1
- Boolean flags still computed for v1.0/v1.1 backward compatibility
- UE Weather Controller auto-detects intensity data for smoother transitions

### By the Numbers

| Category | v1.0 | v1.1 | v1.2 |
|----------|------|------|------|
| **Structure Types** | 186 | 186 | 186 |
| **Terrain Types** | 26 | 26 | 26 |
| **Biome Types** | 52 | 52 | 52 |
| **Weather Effects** | 38 | 38 | 38 |
| **Weather Intensity Channels** | — | — | **6** |
| **Number Words Recognized** | — | — | **30** |
| **Density Modifiers** | — | — | **17** |
| **Position Hint Words** | — | — | **18** |
| **Built-In Presets** | 12 | 24 | 24 |
| **Terrain Sizes** | 1 | 6 | 6 |
| **Asset Mapper** | — | 186 slots | 186 slots |
| **UI Panels** | 1 | 3 tabs | 3 tabs |

### Structure Categories (11 tabs, 186 keywords)

- **Landmark** (26) — Spire, fortress, cathedral, castle, tower, pyramid, temple, palace, citadel, lighthouse...
- **Buildings** (22) — Hut, tavern, cabin, chapel, forge, marketplace, windmill, inn, barracks...
- **Walls/Paths** (23) — Bridge, dock, gate, wall, arch, torii, rampart, drawbridge, colonnade...
- **Low/Dome** (12) — Altar, shrine, dolmen, dome, well, fountain, sundial, gazebo, pergola...
- **Monuments** (16) — Throne, statue, obelisk, monolith, pillar, runestone, mausoleum, tomb...
- **Fantasy** (8) — Portal, crystal orb, enchanted well, fairy ring, summoning circle, rift...
- **Props** (18) — Torch, lantern, brazier, campfire, banner, signpost, cart, barrel, cannon...
- **Ruins** (8) — Ruins, rubble, crumbled wall, broken pillar, collapsed tower, shipwreck...
- **Trees** (19) — Pine, spruce, cedar, oak, willow, birch, maple, palm, acacia, baobab, mangrove...
- **Vegetation** (15) — Shrub, hedge, fern, flower, vine, cattail, lily pad, moss, mushroom cluster...
- **Geological** (19) — Rock, boulder, outcrop, cave mouth, stalagmite, crystal cluster, geyser, lava pool...

### Patent-Pending Technology

**DRO (Dynamic Radix Optimization)** — A novel precision-scaling system that dynamically switches numeric radix bases depending on distance, scale, and LOD requirements. This enables smooth transitions from micro-detail to planetary scale without floating-point artifacts. Powers terrain sizing, weather intensity scaling, and placement density calculations.

### Technical Details

| | |
|---|---|
| **Engine Version** | UE 5.7 |
| **Platforms** | Windows 10/11 x64 |
| **GPU Requirement** | Vulkan-capable |
| **Code Plugin** | C++ (precompiled binary, no source) |
| **Blueprint Support** | Full — all generation + weather intensity exposed as BlueprintCallable |
| **Dependencies** | ProceduralMeshComponent, AssetRegistry (both built-in) |
| **Setup Time** | < 1 minute (drop actor, hit Play) |
| **Mapping Persistence** | INI file in Saved/BaseX/ |

### Quick Start

```
1. Enable BaseXWorldGen in Edit → Plugins
2. Drag ABaseXPromptDemo into any level
3. Press Play
4. [World Prompt] tab — type a prompt with counts and density → Generate World
5. [Asset Mapper] tab — Scan Project → Auto-Match → assign your meshes
6. Try: "a dense forest with 2 castles at the center and rolling thunder"
```

### What You Get

- Precompiled plugin DLL (no source code)
- Public API headers for C++ integration
- Procedural mesh generation (terrain + structures)
- **Smart NLP** with number parsing, density modifiers, and positional hints
- **Intensity-driven weather** with 6 graduated channels
- Dynamic weather system (DirectionalLight + ExponentialHeightFog)
- Full Slate Hub UI with 3 tabs (no UMG/Widget Blueprint dependencies)
- Asset Mapper with project scanner, auto-match, and save/load
- 6 terrain size presets with automatic world scaling
- 24 one-click world presets

### Upgrade from v1.1

Drop-in replacement. All v1.1 prompts, presets, and generated worlds work identically. New features are additive:
- Default density is NORMAL (1.0×) — matching v1.1 placement counts exactly
- Weather intensity values default to v1.1-equivalent; boolean flags still computed
- UE Weather Controller auto-detects v1.2 data; falls back to v1.1 path if absent
- No new UI tabs — NLP improvements are automatic from existing prompt input
- No new asset mappings required — same 186 keyword structure

### Licensing

This plugin is distributed as precompiled binaries under the Base-X Ethical Technology EULA. Source code is proprietary and not included. You may ship games and applications built with this plugin. Reverse engineering, decompilation, or redistribution of the plugin itself is prohibited.

---

**Created by Troll Hunter Gaming**
For licensing inquiries and support: contact Troll Hunter Gaming
