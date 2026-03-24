# Necrotopia VR - Hell Campaign Demos

**Track:** NEC - Necrotopia VR  
**Focus:** Single-player VR campaign to conquer hell  
**Target:** June 2026 vertical slice  

---

## Demo Structure

Each demo focuses on **one aspect** of the game:

### Realm Generation (NEC_001-009)
- `NEC_001_Limbo_glfw.cpp` - Grey plains, ancient ruins
- `NEC_002_Lust_glfw.cpp` - Storm winds, passion fires
- `NEC_003_Gluttony_glfw.cpp` - Toxic swamps, decay
- `NEC_004_Greed_glfw.cpp` - Golden vaults, endless treasures
- `NEC_005_Wrath_glfw.cpp` - Rivers of blood, war fields
- `NEC_006_Heresy_glfw.cpp` - Burning tombs, flame walls
- `NEC_007_Violence_glfw.cpp` - Iron forests, boiling rivers
- `NEC_008_Fraud_glfw.cpp` - Labyrinth bridges, darkness
- `NEC_009_Treachery_glfw.cpp` - Frozen lake, ice spires

### Combat System (NEC_101-110)
- `NEC_101_MeleeCombat_glfw.cpp` - VR weapon wielding
- `NEC_102_MagicCasting_glfw.cpp` - Spell gesture system
- `NEC_103_DemonAI_glfw.cpp` - Enemy behavior patterns
- `NEC_104_BossArena_glfw.cpp` - Boss fight mechanics
- `NEC_105_DamageSystem_glfw.cpp` - Hit detection, health
- `NEC_106_WeaponUpgrades_glfw.cpp` - Progression system

### Magic System (NEC_201-210)
- `NEC_201_FireMagic_glfw.cpp` - Pyromancy spells
- `NEC_202_IceMagic_glfw.cpp` - Cryomancy spells
- `NEC_203_DarkMagic_glfw.cpp` - Necromancy spells
- `NEC_204_HolyMagic_glfw.cpp` - Divine spells
- `NEC_205_SpellCombos_glfw.cpp` - Multi-element combos

### Character Systems (NEC_301-310)
- `NEC_301_CharCreation_glfw.cpp` - Archetype selection
- `NEC_302_Inventory_glfw.cpp` - VR inventory UI
- `NEC_303_Progression_glfw.cpp` - Leveling, skills
- `NEC_304_Equipment_glfw.cpp` - Armor, weapons

### Portal & Navigation (NEC_401-410)
- `NEC_401_RealmPortals_glfw.cpp` - Circle transitions
- `NEC_402_Teleportation_glfw.cpp` - VR movement
- `NEC_403_Checkpoints_glfw.cpp` - Save system

---

## Workflow

### 1. Design in C++ (Fast)
```bash
cd Base-X-Game-Engine
cmake --build build --target NEC_001_Limbo_glfw
./build/demos/NEC_001_Limbo_glfw
```

Preview instantly in GLFW window, iterate design.

### 2. Export Level Data
```cpp
world.ExportToJSON("../../export/NecrotopiaVR/Limbo.json");
```

### 3. Sync to Unreal (When Ready)
```powershell
.\sync_to_unreal.ps1
```

### 4. Test in VR
Open NecrotopiaVR.uproject, import JSON, test in VR headset.

---

## Build All NEC Demos

```bash
cmake --build build --target nec_all
```

This builds all Necrotopia demos at once.

---

## Production Schedule Integration

| Week | Demos | Export To | Unreal Test |
|------|-------|-----------|-------------|
| Jun W1 | NEC_001-003 (3 realms) | Limbo.json, Lust.json, Gluttony.json | Hell Realm Generator demo |
| Jun W1 | NEC_101-103 (combat) | Combat.json | VR Combat System demo |
| Jun W1 | NEC_104 (boss) | BossArena.json | Boss Arena Generator demo |
| Jun W2 | NEC_401 (portals) | Portals.json | Realm Portal System demo |
| Jun W2 | NEC_301-302 (character) | Character.json | Character/Inventory VR demo |

---

Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
