# Base-X World Generation Engine v1.1

**NLP Prompt-Driven Procedural World Generation for Unreal Engine 5.7**

© 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO (Dynamic Radix Optimization), LOD, and Base-X Game Engine are patent pending (USPTO).

---

## Short Description (200 char)

Type a sentence, get a world. Now with Bring Your Own Assets — map your Megascans, Marketplace packs, or custom meshes to 186 keywords. 6 terrain sizes. 24 presets. Patent-pending DRO precision.

## Long Description

### What It Does

Base-X World Generation Engine turns natural language into complete 3D worlds in real-time. Type "a haunted archipelago with shipwrecks, a ruined cathedral, dead trees, and rolling thunder" and watch it generate — heightmap terrain, biome painting, structure placement, and weather effects — all in seconds, all in-editor.

No node graphs. No Blueprint spaghetti. Just describe what you want.

**New in v1.1:** Bring your own Megascans, Marketplace packs, or custom Blender imports. The Asset Mapper scans your project, matches assets to keywords automatically, and lets you assign any mesh to any structure type with one click.

### What's New in v1.1

#### Unified Hub UI
All controls live in a single tabbed panel — no more toggling overlays:
- **World Prompt** tab — prompt box, keyword tags, presets, generation controls
- **Asset Mapper** tab — scan, assign, and manage your mesh library
- **Settings** tab — engine info, keyword counts, version

#### Bring Your Own Assets (Asset Mapper)
Your assets, your worlds, your storage:
- **Project Scanner** — discovers every StaticMesh in your `/Game/` and `/Engine/` content
- **186 Keyword Buttons** — one-click assignment, organized across 11 category tabs
- **Auto-Match** — fuzzy name matching maps your assets to keywords automatically (e.g., `SM_Oak_Large` → `oak`)
- **Save/Load** — mappings persist to `Saved/BaseX/AssetMappings.ini` across sessions
- **Per-Keyword Clear** — red X button to reassign any individual keyword
- **Visual Feedback** — green = assigned, orange = selected, gray = unmapped
- **Search Bar** — filters both keywords and discovered assets simultaneously
- **Zero Package Size** — your meshes are referenced, never duplicated

Works with:
- Quixel Megascans and Bridge imports
- Unreal Marketplace asset packs
- Custom Blender/Maya/3ds Max imports
- UE built-in primitives (Cube, Sphere, Cylinder — everyone has these)

Fallback chain: **User mesh → Procedural mesh → BasicShape primitive**

#### Terrain Size Selector
Every world is unique — now in 6 different scales:

| Preset | Grid | Total Tiles | World Scale | Best For |
|--------|------|-------------|-------------|----------|
| **Tiny** | 16×16 | 256 | 0.5× | Quick prototypes, room-scale |
| **Small** | 32×32 | 1,024 | 0.75× | Arenas, encounter areas |
| **Medium** | 64×64 | 4,096 | 1.0× | Default — balanced worlds |
| **Large** | 128×128 | 16,384 | 1.5× | Open-world zones |
| **Huge** | 256×256 | 65,536 | 2.0× | Expansive landscapes |
| **Epic** | 512×512 | 262,144 | 3.0× | Full open-world maps |

Terrain size is saved with your asset mappings for per-project consistency.

#### 24 Built-In Presets (12 new)
Double the one-click worlds:

| # | Preset | Highlights |
|---|--------|------------|
| 1 | Volcano | Obsidian beaches, lava flows, crystal spire, ash clouds |
| 2 | Frozen | Glacial valleys, ice caves, stone fortress, aurora borealis |
| 3 | Enchanted | Glowing mushrooms, fairy circle, ancient trees, soft mist |
| 4 | Desert | Sandstone ruins, buried pyramid, oasis, scorching sun |
| 5 | Floating | Rope bridges, endless ocean, waterfalls into the void |
| 6 | Necropolis | Dark tombs, skeletal trees, green mist, black cathedral |
| 7 | Steampunk | Brass towers, gear bridges, smoke stacks, airship docks |
| 8 | Crystal | Bioluminescent pools, amethyst pillars, crystal throne |
| 9 | Bamboo | Misty ridge, jade pagoda, torii gate, cherry blossoms |
| 10 | Badlands | Eclipse, rust monoliths, crumbling colosseum, sandstorm |
| 11 | Tundra | Frost atolls, moss, lonely lighthouse, aurora |
| 12 | Savanna | Golden grass, dolmen rings, central forge, twilight haze |
| **13** | **Fjord** | Wooden docks, cliff lighthouse, fishing huts, birch, morning fog |
| **14** | **Lava Canyon** | Lava rivers, obsidian arches, ruined aqueduct, geysers, dawn |
| **15** | **Swamp** | Mangrove roots, cattails, sunken shipwreck, fireflies, mist |
| **16** | **Highland** | Mountain castle, ramparts, drawbridge, bell tower, snow flurries |
| **17** | **Coral Reef** | Tidepools, sea stacks, coral throne, crystal formations, warm drizzle |
| **18** | **Battlefield** | Crumbled walls, cannons, collapsed tower, scorched earth, storm |
| **19** | **Monastery** | Terraced fountain, pergola, flower beds, topiary, golden sunset |
| **20** | **Dark Elf** | Crystal spires, portal rifts, summoning circles, soul lanterns, aurora |
| **21** | **Frontier** | Marketplace, inn, windmill, granary, wagons, signposts, dusty twilight |
| **22** | **Ice Cave** | Stalactites, frozen waterfalls, crystal clusters, geodes, ice altar |
| **23** | **Mesa** | Ziggurat summit, obelisk colonnade, runestones, sundial, acacia |
| **24** | **Haunted** | Shipwrecks, gravestones, ruined cathedral, dead trees, thunder |

### By the Numbers

| Category | v1.0 | v1.1 |
|----------|------|------|
| **Structure Types** | 186 | 186 |
| **Terrain Types** | 26 | 26 |
| **Biome Types** | 52 | 52 |
| **Weather Effects** | 38 | 38 |
| **Built-In Presets** | 12 | **24** |
| **Terrain Sizes** | 1 (64×64) | **6** (16×16 to 512×512) |
| **Asset Mapper** | — | **186 keyword slots** |
| **UI Panels** | 1 | **3 tabs** (Prompt, Asset Mapper, Settings) |
| **Default Asset Library** | — | **Engine built-ins** (auto-discovered) |

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

**DRO (Dynamic Radix Optimization)** — A novel precision-scaling system that dynamically switches numeric radix bases depending on distance, scale, and LOD requirements. This enables smooth transitions from micro-detail to planetary scale without floating-point artifacts. Now powers the new terrain size system, automatically adjusting world scale across 6 size presets.

### Technical Details

| | |
|---|---|
| **Engine Version** | UE 5.7 |
| **Platforms** | Windows 10/11 x64 |
| **GPU Requirement** | Vulkan-capable |
| **Code Plugin** | C++ (precompiled binary, no source) |
| **Blueprint Support** | Full — all generation exposed as BlueprintCallable |
| **Dependencies** | ProceduralMeshComponent, AssetRegistry (both built-in) |
| **Setup Time** | < 1 minute (drop actor, hit Play) |
| **Mapping Persistence** | INI file in Saved/BaseX/ |

### Quick Start

```
1. Enable BaseXWorldGen in Edit → Plugins
2. Drag ABaseXPromptDemo into any level
3. Press Play
4. [World Prompt] tab — type a prompt or click tags → Generate World
5. [Asset Mapper] tab — Scan Project → Auto-Match → assign your meshes
6. Choose terrain size → Generate again with your assets at your scale
```

### What You Get

- Precompiled plugin DLL (no source code)
- Public API headers for C++ integration
- Procedural mesh generation (terrain + structures)
- Dynamic weather system (DirectionalLight + ExponentialHeightFog)
- Full Slate Hub UI with 3 tabs (no UMG/Widget Blueprint dependencies)
- Asset Mapper with project scanner, auto-match, and save/load
- 6 terrain size presets with automatic world scaling
- 24 one-click world presets

### Upgrade from v1.0

Drop-in replacement. All v1.0 prompts, presets, and generated worlds work identically. New features are additive:
- Hub UI replaces the single-panel layout (same controls, now tabbed)
- Asset Mapper is optional — worlds generate with procedural meshes if no assets are mapped
- Terrain size defaults to Medium (64×64), matching v1.0 behavior exactly

### Licensing

This plugin is distributed as precompiled binaries under the Base-X Ethical Technology EULA. Source code is proprietary and not included. You may ship games and applications built with this plugin. Reverse engineering, decompilation, or redistribution of the plugin itself is prohibited.

---

**Created by Troll Hunter Gaming**
For licensing inquiries and support: contact Troll Hunter Gaming
