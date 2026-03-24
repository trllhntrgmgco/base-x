# Production Workflow: Modular C++ → Unreal Composition

**Philosophy:** Each game aspect developed independently in lightweight C++ demos, then composed in Unreal only for final testing/packaging.

---

## Directory Structure

```
Base-X-Game-Engine/
├── src/demos/
│   ├── NEC/                          # Necrotopia VR demos
│   │   ├── NEC_001_Limbo_glfw.cpp           # Realm generation
│   │   ├── NEC_101_MeleeCombat_glfw.cpp     # Combat system
│   │   ├── NEC_201_FireMagic_glfw.cpp       # Magic system
│   │   └── ...
│   ├── BNT/                          # Bible NT VR demos
│   ├── SPT/                          # Sports VR demos
│   └── NEC_CMakeLists.txt            # Build config per track
│
├── export/                           # JSON exports for Unreal
│   ├── NecrotopiaVR/
│   │   ├── Limbo.json
│   │   ├── Combat.json
│   │   └── ...
│   └── BibleNT_VR/
│
└── sync_to_unreal.ps1                # Sync exports + plugin

NecrotopiaVR/                         # Unreal project (separate repo location)
├── Content/NEC/Realms/Imported/      # Auto-imported JSON data
├── Plugins/BaseXWorldGen/            # Synced from Base-X-Engine
└── Source/...                        # Game-specific code
```

---

## Workflow Steps

### 1. Design System in C++ (Fast Iteration)

**Example: Designing Limbo realm**

```bash
cd Base-X-Game-Engine
code src/demos/NEC/NEC_001_Limbo_glfw.cpp

# Edit realm design:
# - Terrain type
# - Structure placement
# - Biome colors
# - Weather effects

# Build and run
cmake --build build --target NEC_001_Limbo_glfw
./build/demos/NEC_001_Limbo_glfw
```

**Benefits:**
- ✅ **<1 second** compile time (vs 2-3 minutes Unreal)
- ✅ Instant visual feedback in GLFW window
- ✅ Standard C++ debugger (GDB/MSVC)
- ✅ Git-friendly source code
- ✅ Unit testable

---

### 2. Export Level Data

**In C++ demo:**

```cpp
// After designing and previewing
world.ExportToJSON("../../export/NecrotopiaVR/Limbo.json");
```

**JSON format:**

```json
{
  "realm": "Limbo",
  "circle": 1,
  "size": { "x": 128, "y": 128 },
  "biomes": [
    { "type": "limbo_grey", "bounds": [[0,0], [128,128]] }
  ],
  "structures": [
    { "type": "castle_ruins", "pos": [64, 64], "density": 0.2 }
  ],
  "weather": { "type": "mist", "intensity": 0.8 }
}
```

---

### 3. Sync to Unreal Project

**Run sync script:**

```powershell
.\sync_to_unreal.ps1
```

**What it does:**
1. Syncs BaseXWorldGen plugin → `NecrotopiaVR/Plugins/`
2. Copies JSON exports → `NecrotopiaVR/Content/NEC/Realms/Imported/`
3. Updates timestamps

---

### 4. Import in Unreal (On-Demand)

**Open NecrotopiaVR.uproject:**

```cpp
// Blueprint or C++ in Unreal
UBaseXLevelImporter::ImportRealmFromJSON("Limbo.json");
```

**This:**
- Reads JSON data
- Uses `UBaseXWorldBuilder` to spawn structures
- Applies biomes, weather, lighting
- Creates playable level

---

### 5. Test in VR

- Put on VR headset
- Walk through generated realm
- Identify issues
- **Go back to step 1** to fix in C++ (fast iteration)

---

## Demo Categories

### NEC (Necrotopia VR)

| Range | Category | Examples |
|-------|----------|----------|
| **001-009** | Realm Generation | Limbo, Lust, Violence realms |
| **101-110** | Combat System | Melee, magic, demon AI |
| **201-210** | Magic System | Fire, ice, dark, holy spells |
| **301-310** | Character System | Creation, inventory, progression |
| **401-410** | Portal/Navigation | Realm transitions, teleportation |

### BNT (Bible NT VR)

| Range | Category | Examples |
|-------|----------|----------|
| **001-009** | Environment Gen | Holy Land, Sea of Galilee |
| **101-110** | Miracle System | Water to wine, healing |
| **201-210** | Narrative System | Sermon on Mount, Passion |

### SPT (Sports VR)

| Range | Category | Examples |
|-------|----------|----------|
| **001-009** | Court/Field Gen | Basketball court, soccer pitch |
| **101-110** | Physics System | Ball handling, collision |

---

## Build Commands

```bash
# Build single demo
cmake --build build --target NEC_001_Limbo_glfw

# Build all NEC demos
cmake --build build --target nec_all

# Build all BNT demos
cmake --build build --target bnt_all

# Build everything
cmake --build build
```

---

## Development Timeline Example

### June 2026 - Necrotopia VR Vertical Slice

**Week 1 (Jun 1-7):**

| Day | C++ Demo Work | Unreal Test |
|-----|---------------|-------------|
| Mon | NEC_001 Limbo design | - |
| Tue | NEC_002 Lust design | - |
| Wed | NEC_007 Violence design | - |
| Thu | NEC_101 Melee combat | - |
| Fri | NEC_104 Boss arena | **Sync & VR test all** |

**Week 2 (Jun 8-14):**

| Day | C++ Demo Work | Unreal Test |
|-----|---------------|-------------|
| Mon | NEC_401 Portal system | - |
| Tue | NEC_301 Character creation | - |
| Wed | NEC_302 Inventory VR | - |
| Thu | Polish based on feedback | - |
| Fri | Final integration | **Package vertical slice** |

**Result:** 6 vertical slice demos ready for presentation.

---

## Advantages Over Pure Unreal Development

| Aspect | C++ Demo | Unreal Editor |
|--------|----------|---------------|
| **Iteration Speed** | <1 second | 2-3 minutes |
| **Debugging** | Standard debugger | UE logs + crashes |
| **Version Control** | Clean diffs | Binary assets |
| **Collaboration** | Easy merge | Lock conflicts |
| **Testing** | Unit tests | Manual play |
| **Cost** | Zero | High (if using marketplace assets) |

---

## When to Use Unreal

✅ **Do use Unreal for:**
- Final VR testing with headset
- Visual quality validation
- Performance profiling
- Packaging demos for submission
- Integrating marketplace assets (Megascans, etc.)

❌ **Don't use Unreal for:**
- Iterating on game logic
- Tweaking structure placement
- Adjusting biome colors
- Testing combat formulas
- Rapid prototyping

---

## File Naming Convention

```
{TRACK}_{ID}_{Name}_glfw.cpp

Examples:
NEC_001_Limbo_glfw.cpp           # Necrotopia, realm #1
NEC_101_MeleeCombat_glfw.cpp     # Necrotopia, combat system
BNT_001_Galilee_glfw.cpp         # Bible NT, environment #1
SPT_001_Basketball_glfw.cpp      # Sports, court #1
```

**Suffix `_glfw`:** Indicates graphical demo (requires GLFW)

---

## Global Improvements

**Improve one system → All demos benefit immediately**

Example: Better tree generation algorithm

1. Update `src/core/StructureFactory.cpp`
2. Rebuild all demos: `cmake --build build`
3. **Every demo** that uses trees now looks better
4. Sync to Unreal: All Unreal projects get the improvement

**No need to manually update 39 different Unreal levels!**

---

Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
