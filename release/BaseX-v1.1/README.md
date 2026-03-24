# Base-X World Generation Engine - Unreal Plugin v1.1.0

Copyright 2026 Keenan Dunham and Troll Hunter Gaming. All Rights Reserved.
DRO (Dynamic Radix Optimization), LOD, and Base-X Game Engine are patent pending (USPTO).

## What's New in v1.1

### Unified Hub UI
Single tabbed panel replaces the separate overlay approach:
- **World Prompt** tab — prompt box, keyword tags, presets, generation controls
- **Asset Mapper** tab — scan, assign, and manage your mesh library
- **Settings** tab — engine info, keyword inventory, version

### Bring Your Own Assets (Asset Mapper)
- Scan your project to discover all StaticMesh assets (+ Engine defaults)
- 186 keyword buttons with one-click mesh assignment
- Auto-match: fuzzy name matching maps your assets to keywords automatically
- Save/Load mappings that persist across sessions
- Per-keyword clear/reassign with visual feedback (green = assigned)
- Category tabs filter keywords (Landmark, Buildings, Trees, etc.)
- Search bar filters both keywords and discovered assets

### Terrain Size Selector
- 6 size presets: Tiny (16x16) through Epic (512x512)
- Each size produces a uniquely scaled world
- World scale adjusts automatically with tile count
- Saved with asset mappings for per-project consistency

### 24 Built-In Presets (12 new)
- v1.0: Volcano, Frozen, Enchanted, Desert, Floating, Necropolis, Steampunk, Crystal, Bamboo, Badlands, Tundra, Savanna
- v1.1: Fjord, Lava Canyon, Swamp, Highland, Coral Reef, Battlefield, Monastery, Dark Elf, Frontier, Ice Cave, Mesa, Haunted

### Compatibility
- Works with Megascans, Marketplace packs, or custom Blender imports
- Fallback chain: User mesh -> Procedural mesh -> BasicShape primitive
- Zero additional package size (your assets, your storage)

## Overview

NLP prompt-driven procedural world generation for Unreal Engine 5.7.
Type a natural language description and get a complete 3D world with terrain,
biomes, structures, and weather - all generated in real-time.

## Features

- 186 structure types with procedural meshes (trees, buildings, ruins, crystals)
- 26 terrain types (mountain, canyon, volcano, fjord, archipelago)
- 52 biome types with unique color palettes
- 38 weather effects (rain, storm, snow, aurora, eclipse)
- DRO precision scaling - patent-pending radix optimization
- 24 built-in presets - one-click world generation
- 6 terrain size presets (16x16 to 512x512)
- Asset Mapper with project scanner and auto-match
- Tabbed Hub UI - click keywords to build prompts

## Quick Start

1. Enable the plugin in Edit > Plugins
2. Drop ABaseXPromptDemo into any level
3. Hit Play
4. Use the World Prompt tab to type or click-build a prompt
5. Switch to Asset Mapper to assign your own meshes
6. Choose a terrain size and Generate World

## System Requirements

- Unreal Engine 5.7
- Windows 10/11 x64
- Vulkan-capable GPU

## Legal

This plugin is distributed as precompiled binaries.
Source code is proprietary and not included.
Reverse engineering, decompilation, or redistribution is prohibited.

For licensing inquiries: contact Troll Hunter Gaming
