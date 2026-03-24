# Base-X Engine v1.0 — Demo Release

© 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

---

## Included Demos

| Executable | Description |
|------------|-------------|
| `SBX_001_WorldPromptEngine_glfw.exe` | NLP prompt → procedural world generation with DRO precision shifting across 5 pipeline phases (heightmap → biome → structures → weather → complete) |
| `WLD_011_WorldBuilder_glfw.exe` | Holodeck world builder — 5 biomes, object palette, terrain editing, fog/atmosphere, day/night, physics toggle |
| `WLD_012_Ecosystem_glfw.exe` | Living ecosystem — 12 creature types, 8×8 tile grid with multi-LOD DRO, dynamic sky/sun/moon/stars/clouds, tree wind, weather |
| `WPE_001_PromptWorld_glfw.exe` | Prompt-driven walkable 3D world explorer — full inventory tag picker, generation progress, free-camera explore with dynamic atmosphere |

## System Requirements

- Windows 10/11 x64
- Vulkan-capable GPU with up-to-date drivers
- No installation required — all executables are self-contained

## Running

Double-click any `.exe` or launch from a terminal:

```
.\SBX_001_WorldPromptEngine_glfw.exe
```

## Controls

### SBX-001: World Prompt Engine
| Key | Action |
|-----|--------|
| `1`–`8` | Load preset prompt |
| `Enter` | Generate world from prompt |
| `Backspace` | Delete last character |
| `R` | Reset / clear world |
| `ESC` | Quit |
| Type | Edit prompt text |

### WLD-011: World Builder
| Key | Action |
|-----|--------|
| `WASD` | Move camera |
| `Mouse` | Look around / select |
| `LMB` / `RMB` | Place / remove object |
| `1`–`5` | Select biome |
| `T` | Toggle terrain edit |
| `G` | Generate new seed |
| `+` / `-` | Brush size |
| `Tab` | Cycle object palette |
| `F` | Toggle fog |
| `L` | Toggle lighting |
| `R` | Reset world |
| `ESC` | Quit |

### WLD-012: Ecosystem
| Key | Action |
|-----|--------|
| `WASD` | Move camera |
| `Mouse+RMB` | Look around |
| `1`–`9`,`0`,`-`,`=` | Select creature type |
| `Space` | Spawn at cursor |
| `T` | Time speed |
| `G` | Cycle weather |
| `N` | Day/night cycle |
| `P` | Pause |
| `H` | Toggle HUD |
| `F` | Follow mode |
| `Tab` | Select next creature |
| `R` | Reset ecosystem |
| `ESC` | Quit |

### WPE-001: Prompt World Explorer
**Prompt Screen:**
| Key | Action |
|-----|--------|
| Click tags | Toggle into/out of prompt |
| Type | Edit prompt |
| `1`–`8` | Load preset |
| `Enter` | Generate world |
| `Up`/`Down` | Change start hour |
| `+` / `-` | Change day speed |

**Explore Mode:**
| Key | Action |
|-----|--------|
| `WASD` | Move camera |
| `Mouse+RMB` | Look around |
| `Scroll` | Adjust speed |
| `Shift` | Descend |
| `T` | Cycle time speed |
| `G` | Cycle weather override |
| `N` | Toggle day/night |
| `H` | Toggle HUD |
| `P` | Pause time |
| `ESC` | Back to prompt screen |

---

## Technology

These demos are built on the **Base-X Engine** — a VR-first game engine featuring:

- **DRO (Dynamic Radix Optimization)** — Patent-pending precision scaling system that dynamically allocates computational precision across subsystems based on generation phase and player proximity
- **LOD (Level of Detail)** — Patent-pending multi-tier rendering with DRO-driven base selection (B-64 → B-32 → B-16 → B-8)
- **WorldPromptEngine** — NLP-driven procedural world generation pipeline: natural language → heightmap → biomes → structures → weather
- **Vulkan/OpenGL rendering** with GLFW windowing
- **OpenXR VR support** (in full engine builds)

## License

This software is proprietary. Unauthorized copying, distribution, or reverse engineering is prohibited.

© 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO, LOD, and Base-X Game Engine are patent pending (USPTO).
