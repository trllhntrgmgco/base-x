# Base-X World Generation Engine v1.2 — Changelog

© 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
DRO (Dynamic Radix Optimization), LOD, and Base-X Game Engine are patent pending (USPTO).

---

## v1.2 — Smart Prompt NLP, Density, Exact Counts & Weather Intensity

### NLP Prompt Parser Upgrades

#### Number Specificity
- **Digit parsing**: "2 trees", "5 rocks", "1 castle" → places exactly that number
- **Word parsing**: "two oaks", "three towers", "a dozen pines" → exact count
- **Plural handling**: "trees", "castles", "boulders" → auto-depluralized to match keywords
- **Fallback**: structures without a number prefix use category defaults (trees 6-21, rocks 3-11, buildings 2-6)

Supported number words:
`a`, `an`, `one`, `single`, `lone`, `two`, `couple`, `pair`, `three`, `trio`,
`four`, `five`, `six`, `seven`, `eight`, `nine`, `ten`, `eleven`, `twelve`,
`dozen`, `fifteen`, `twenty`, `hundred`, `several`, `handful`, `few`, `many`, `some`

#### Density Modifiers
- **NLP-parsed density** from prompt adjectives scales all default placement counts
- Exact number overrides (`"2 trees"`) are **not** scaled — you get exactly what you asked for

| Modifier | Multiplier | Example Prompt |
|----------|-----------|----------------|
| `sparse`, `barren`, `desolate`, `empty` | 0.25× | "a sparse forest on a hill" |
| `scattered`, `thin` | 0.5× | "scattered pines on a ridge" |
| *(default)* | 1.0× | "a forest with oaks and birch" |
| `thick`, `sprawling`, `vast` | 1.5× | "thick jungle with vines" |
| `dense`, `packed`, `crowded` | 2.0× | "a dense enchanted forest" |
| `lush`, `overgrown`, `abundant`, `teeming` | 3.0× | "a lush tropical island" |

#### Positional Placement Hints
- **NLP-parsed position** from spatial keywords directs structure placement
- Structures with position hints cluster around the indicated region with natural scatter

| Keyword | Grid Region |
|---------|-------------|
| `center`, `centre`, `middle`, `central`, `summit`, `peak`, `hilltop` | Center of map ± scatter |
| `north` | Top edge region |
| `south` | Bottom edge region |
| `east` | Right edge region |
| `west` | Left edge region |
| `edge`, `border`, `perimeter`, `shore`, `coast`, `cliff` | Random edge |
| `corner` | Random corner |

Example: *"a castle at the center with 3 oaks on the north edge"*

### Weather System Upgrades

#### Intensity-Based Weather
v1.1 used boolean flags (`hasRain = true/false`). v1.2 adds **float intensity values** that stack from multiple weather keywords:

| Field | Range | Description |
|-------|-------|-------------|
| `rainIntensity` | 0..1 | Drizzle=0.2, Rain=0.5, Storm=0.7, Monsoon=0.9 |
| `snowIntensity` | 0..1 | Frost=0.15, Snow=0.5, Blizzard=0.9 |
| `windIntensity` | 0..1 | Wind=0.3, Storm=0.6, Hurricane=1.0 |
| `cloudCover` | 0..1 | Overcast=0.5, Rain=0.7, Storm=0.9 |
| `visibility` | 0..1 | 1.0=clear, reduced by fog/mist/blizzard/sandstorm |
| `temperature` | 0..1 | 0.5=neutral, +scorching/sun, -snow/frost/blizzard |

- Multiple weather keywords **stack** (e.g., "rain and fog" → higher cloud + lower visibility)
- Values are clamped to 0..1 after accumulation
- Boolean flags still computed for v1.0/v1.1 backward compatibility

#### UE Weather Controller
- `UBaseXWeatherController::ApplyWeatherState()` now detects v1.2 intensity data
- When present, uses float targets instead of boolean-derived hard-coded values
- Falls back to v1.1 boolean path for backward compatibility
- Enhanced logging shows all intensity values

### Structure Placement Upgrades

- **Exact count enforcement**: `StructureRequest` entries with `exactCount > 0` override category defaults
- **Density scaling**: all default placement counts multiplied by `DensityLevel` factor
- **Positional clustering**: `positionFromHint()` places structures near the requested map region with natural jitter
- Exact counts bypass density scaling — "2 trees in a dense forest" places exactly 2 trees (not 4)

### Data Type Changes

#### C++ Core (`WorldPromptEngine.h`)
- `WeatherState` — added 6 float intensity fields
- `DensityLevel` enum — `SPARSE` through `LUSH` (6 levels)
- `StructureRequest` struct — per-structure type/count/position
- `ParseResult` — added `density` field + `structureRequests` vector

#### UE Types (`FBaseXTypes.h`)
- `FBaseXWeatherState` — added `RainIntensity`, `SnowIntensity`, `WindIntensity`, `CloudCover`, `Visibility`, `Temperature`
- `FBaseXParseResult` — added `DensityLevel`, `ExactCountOverrides`

### By the Numbers

| Category | v1.0 | v1.1 | v1.2 |
|----------|------|------|------|
| **Structure Types** | 186 | 186 | 186 |
| **Terrain Types** | 26 | 26 | 26 |
| **Biome Types** | 52 | 52 | 52 |
| **Weather Effects** | 38 | 38 | 38 |
| **Weather Intensity Fields** | — | — | **6** |
| **Number Words Recognized** | — | — | **30** |
| **Density Modifiers** | — | — | **17** |
| **Position Hint Words** | — | — | **18** |
| **Built-In Presets** | 12 | 24 | 24 |
| **Terrain Sizes** | 1 | 6 | 6 |
| **Asset Mapper** | — | 186 slots | 186 slots |

### Upgrade from v1.1

Drop-in replacement. All v1.1 prompts, presets, and generated worlds work identically:
- Boolean weather flags still computed — existing Blueprint logic unaffected
- Default density is `NORMAL` (1.0×) — matching v1.1 placement counts exactly
- Structure counts unchanged unless prompt contains number words or density modifiers
- `FBaseXWeatherState` new fields default to v1.1-equivalent values
- `UBaseXWeatherController` auto-detects v1.2 data; falls back to v1.1 path otherwise
