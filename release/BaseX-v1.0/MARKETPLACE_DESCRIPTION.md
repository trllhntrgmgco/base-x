# Base-X World Generation Engine v1.0

**NLP Prompt-Driven Procedural World Generation for Unreal Engine 5.7**

© 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO (Dynamic Radix Optimization), LOD, and Base-X Game Engine are patent pending (USPTO).

---

## Short Description (200 char)

Type a sentence, get a world. NLP prompt-driven procedural generation — 186 structures, 26 terrains, 52 biomes, 38 weather effects. Patent-pending DRO precision. Zero Blueprint setup.

## Long Description

### What It Does

Base-X World Generation Engine turns natural language into complete 3D worlds in real-time. Type "a volcanic island with obsidian beaches, a crystal spire, and ash clouds at dawn" and watch it generate — heightmap terrain, biome painting, structure placement, and weather effects — all in seconds, all in-editor.

No node graphs. No Blueprint spaghetti. Just describe what you want.

### How It Works

Drop `ABaseXPromptDemo` into any level and hit Play. A full Slate UI appears with:

- **Prompt Box** — Type any world description in plain English
- **Interactive Tag Buttons** — Click keywords to build prompts visually
- **12 Built-In Presets** — One-click worlds: Volcano, Frozen Peaks, Enchanted Forest, Desert Ruins, Floating Islands, Necropolis, Steampunk City, Crystal Caverns, Bamboo Ridge, Crimson Badlands, Tundra Archipelago, Savanna Caldera
- **Real-Time Progress** — Watch generation across 4 phases: Heightmap → Biomes → Structures → Weather
- **Parse Feedback** — See exactly which keywords the engine recognized

### By the Numbers

| Category | Count | Examples |
|----------|-------|---------|
| **Structure Types** | 186 | Trees (19 species), buildings, ruins, crystals, portals, monuments, props |
| **Terrain Types** | 26 | Mountain, canyon, volcano, fjord, archipelago, mesa, tundra, caldera |
| **Biome Types** | 52 | Unique color palettes per biome with smooth blending |
| **Weather Effects** | 38 | Rain, storm, snow, aurora, eclipse, ash, mist, scorching, moonlit, dawn |

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

**DRO (Dynamic Radix Optimization)** — A novel precision-scaling system that dynamically switches numeric radix bases depending on distance, scale, and LOD requirements. This enables smooth transitions from micro-detail to planetary scale without floating-point artifacts.

### Technical Details

| | |
|---|---|
| **Engine Version** | UE 5.7 |
| **Platforms** | Windows 10/11 x64 |
| **GPU Requirement** | Vulkan-capable |
| **Code Plugin** | C++ (precompiled binary, no source) |
| **Blueprint Support** | Full — all generation exposed as BlueprintCallable |
| **Dependencies** | ProceduralMeshComponent (built-in) |
| **Setup Time** | < 1 minute (drop actor, hit Play) |

### Quick Start

```
1. Enable BaseXWorldGen in Edit → Plugins
2. Drag ABaseXPromptDemo into any level
3. Press Play
4. Type a prompt or click tag buttons → Generate World
```

### What You Get

- Precompiled plugin DLL (no source code)
- Public API headers for C++ integration
- Procedural mesh generation (terrain + structures)
- Dynamic weather system (DirectionalLight + ExponentialHeightFog)
- Full Slate UI (no UMG/Widget Blueprint dependencies)

### Licensing

This plugin is distributed as precompiled binaries under the Base-X Ethical Technology EULA. Source code is proprietary and not included. You may ship games and applications built with this plugin. Reverse engineering, decompilation, or redistribution of the plugin itself is prohibited.

---

**Created by Troll Hunter Gaming**
For licensing inquiries and support: contact Troll Hunter Gaming
