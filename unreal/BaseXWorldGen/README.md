# Base-X World Generation — Unreal Engine Plugin

Complete port of the Base-X `WorldPromptEngine` + `DRO` + ecosystem sky/weather
pipeline into an Unreal Engine 5 plugin for VR world-building.

---

## Plugin Structure

```
unreal/BaseXWorldGen/
├── BaseXWorldGen.uplugin              # Plugin descriptor
├── ThirdParty/BaseX/                  # ORIGINAL Base-X C++ headers (unchanged)
│   ├── DRO.h                          #   Dynamic Radix Optimization
│   ├── DRO.cpp                        #   DRO implementation
│   └── WorldPromptEngine.h            #   NLP → world generation pipeline
└── Source/BaseXWorldGen/
    ├── BaseXWorldGen.Build.cs          # UE build rules
    ├── Public/
    │   ├── BaseXWorldGenModule.h       # Module entry
    │   ├── FBaseXTypes.h               # USTRUCT mirrors of Base-X types
    │   ├── UBaseXWorldBuilder.h        # Core UObject (wraps WorldPromptEngine)
    │   ├── ABaseXWorldMesh.h           # ProceduralMeshComponent terrain actor
    │   ├── ABaseXStructureSpawner.h    # DataTable-driven structure spawning
    │   ├── UBaseXWeatherController.h   # Weather → UE atmosphere bridge
    │   └── ABaseXVRWorldBuilder.h      # VR tablet actor (orchestrator)
    └── Private/
        ├── BaseXWorldGenModule.cpp
        ├── UBaseXWorldBuilder.cpp
        ├── ABaseXWorldMesh.cpp
        ├── ABaseXStructureSpawner.cpp
        ├── UBaseXWeatherController.cpp
        └── ABaseXVRWorldBuilder.cpp
```

---

## Step-by-Step Setup

### 1. Create Unreal Project

```
1. Open Unreal Engine 5.3+ (or 5.4 for better VR)
2. New Project → Games → VR Template (or Blank + add VR pawn later)
3. C++ project, name it "BaseXWorld" (or anything)
4. Engine creates:  BaseXWorld/Source/BaseXWorld/
```

### 2. Copy Plugin Into Project

```powershell
# From your Base-X Engine repo:
Copy-Item -Recurse "unreal\BaseXWorldGen" "C:\UE_Projects\BaseXWorld\Plugins\BaseXWorldGen"
```

Your project should now look like:
```
BaseXWorld/
├── Plugins/
│   └── BaseXWorldGen/
│       ├── BaseXWorldGen.uplugin
│       ├── ThirdParty/BaseX/...
│       └── Source/BaseXWorldGen/...
├── Source/BaseXWorld/...
└── BaseXWorld.uproject
```

### 3. Enable the Plugin

Add to your `.uproject`:
```json
"Plugins": [
    { "Name": "BaseXWorldGen", "Enabled": true },
    { "Name": "ProceduralMeshComponent", "Enabled": true },
    { "Name": "Niagara", "Enabled": true }
]
```

Add to your game module's `Build.cs`:
```csharp
PublicDependencyModuleNames.Add("BaseXWorldGen");
```

### 4. Regenerate Project Files

```powershell
# From the UE project root:
& "C:\Program Files\Epic Games\UE_5.3\Engine\Build\BatchFiles\GenerateProjectFiles.bat" BaseXWorld.uproject
```

Or right-click `.uproject` → "Generate Visual Studio project files"

### 5. Build

Open the `.sln` in Visual Studio, build `Development Editor | Win64`.
The Base-X C++ headers compile directly — no modifications needed.

---

## Quick Test (No VR Required)

### Minimal Blueprint Test

1. Open UE Editor
2. Create a new Actor Blueprint: `BP_WorldTest`
3. Add component: `BaseXWeatherController`
4. In the Event Graph:

```
BeginPlay:
  ┌─ Create Object (UBaseXWorldBuilder) → store as variable "Builder"
  ├─ Builder.GenerateFromPrompt("mountain forest tower mist")
  └─ Set Timer by Event (0.016, looping) → "TickGen"

TickGen:
  ┌─ Builder.TickGeneration(0.016)
  ├─ Branch: Builder.IsComplete?
  │   ├─ Yes: Spawn Actor (ABaseXWorldMesh) → call BuildFromWorldBuilder(Builder)
  │   │       Spawn Actor (ABaseXStructureSpawner) → call SpawnStructures(Builder)
  │   │       WeatherController.ApplyWeatherState(Builder.GetWeatherState())
  │   │       Clear Timer
  │   └─ No: continue
```

5. Drop `BP_WorldTest` in level
6. Drop a `DirectionalLight` and `ExponentialHeightFog` — assign them to the WeatherController
7. Hit Play — world generates in ~2 seconds, terrain mesh appears

### What You'll See

- 64×64 ProceduralMesh terrain with biome vertex colors
- Water plane at the configurable water level
- Structure actors spawned at valid terrain positions
- Sun orbiting with day/night cycle
- Fog density responding to weather state

---

## VR Setup

### 1. Configure the VR Pawn

Use the VR Template pawn, or create one with:
- `MotionControllerComponent` (left + right hands)
- `WidgetInteractionComponent` on dominant hand (for tablet UI)

### 2. Create the Tablet UMG Widget

Create `WBP_BaseXTablet` (Widget Blueprint) with:

| Element | Binding |
|---------|---------|
| TextBox "PromptInput" | Bind to `ABaseXVRWorldBuilder::CurrentPrompt` |
| Buttons 1-8 "Preset1..8" | OnClick → `LoadPreset(0..7)` |
| Button "Generate" | OnClick → `Generate()` |
| Button "Clear" | OnClick → `ClearWorld()` |
| Slider "StartHour" | OnChanged → `SetStartHour(Value)` |
| Slider "DaySpeed" | OnChanged → `SetDaySpeed(Value)` |
| ProgressBar | Bind to `GetProgress() / 100` |
| Tag grid (terrain/biome/structure/weather) | Populate from `GetTerrainKeywords()` etc. |

### 3. Place the VR World Builder

1. Drop `ABaseXVRWorldBuilder` in your level
2. Set `TabletWidgetClass` → `WBP_BaseXTablet`
3. Set `WorldMeshClass` → `ABaseXWorldMesh` (or a Blueprint child of it)
4. Set `StructureSpawnerClass` → `ABaseXStructureSpawner`
5. Assign `SunLight` and `Fog` references on the WeatherController
6. Hit VR Preview — point at tablet, interact with triggers

---

## Architecture: What's Identical to Base-X, What's UE-Only

| Layer | Files | Source |
|-------|-------|--------|
| **Pure C++ (unchanged)** | `DRO.h`, `DRO.cpp`, `WorldPromptEngine.h` | Copied verbatim from `include/BaseX/` |
| **Type bridge** | `FBaseXTypes.h` | USTRUCT mirrors of Base-X structs |
| **Core wrapper** | `UBaseXWorldBuilder.h/.cpp` | Wraps `WorldPromptEngine` in UObject via PIMPL |
| **Terrain mesh** | `ABaseXWorldMesh.h/.cpp` | Replaces `renderTerrain()` GL calls with ProceduralMesh |
| **Structures** | `ABaseXStructureSpawner.h/.cpp` | Replaces `renderStructures()` with actor spawning |
| **Weather** | `UBaseXWeatherController.h/.cpp` | Replaces `renderSky()`/`updateWeather()` with UE atmosphere |
| **VR orchestrator** | `ABaseXVRWorldBuilder.h/.cpp` | Replaces GLFW main loop + prompt screen |

### Keeping In Sync

When you update `WorldPromptEngine.h` or `DRO.h` in the Base-X repo:

```powershell
Copy-Item "include\BaseX\DRO.h" "unreal\BaseXWorldGen\ThirdParty\BaseX\"
Copy-Item "include\BaseX\WorldPromptEngine.h" "unreal\BaseXWorldGen\ThirdParty\BaseX\"
Copy-Item "src\physics\DRO.cpp" "unreal\BaseXWorldGen\ThirdParty\BaseX\"
```

The UE bridge layer doesn't change unless you add new public API to the engine.

---

## Next Steps (Future Features)

- [ ] **Foliage**: Wire tree placement to UE Foliage system or PCG framework
- [ ] **Niagara weather**: Spawn rain/snow/mist particle systems from WeatherController
- [ ] **Landscape import**: Convert heightmap to UE Landscape for LOD streaming
- [ ] **Multiplayer**: Replicate prompt + generation seed for shared world building
- [ ] **Save/Load**: Serialize `WorldPromptEngine` state to disk
- [ ] **AI enhancement**: Wire `NLPPromptParser` to local LLM for richer parsing
