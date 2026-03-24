# Necrotopia VR - Exported Level Data

Realm layouts, combat data, and magic systems exported from C++ demos.

## Files

- `Limbo.json` - Circle 1 realm layout
- `Lust.json` - Circle 2 realm layout
- `Violence.json` - Circle 7 realm layout
- `Treachery.json` - Circle 9 realm layout
- `Combat.json` - Melee combat system data
- `Magic.json` - Spell casting system data

## Import into Unreal

```cpp
// In NecrotopiaVR Blueprint or C++
UBaseXLevelImporter::ImportRealmFromJSON("Limbo.json");
```

---

Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
