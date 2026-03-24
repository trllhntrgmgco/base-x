# Base-X Engine - Export Directory

This folder contains level data exported from C++ demos for import into Unreal projects.

## Structure

```
export/
├── NecrotopiaVR/           # Necrotopia VR exports
│   ├── Limbo.json
│   ├── Violence.json
│   ├── Combat.json
│   └── ...
├── BibleNT_VR/             # Bible NT VR exports
├── SportsVR/               # Sports VR exports
└── ...
```

## Workflow

1. **Design in C++ demo** → Fast iteration
2. **Export to JSON** → `world.ExportToJSON("export/ProjectName/Level.json")`
3. **Sync to Unreal** → `.\sync_to_unreal.ps1`
4. **Import in Unreal** → Blueprint: `ImportRealmFromJSON("Level.json")`

---

Copyright 2026 Keenan Dunham & Troll Hunter Gaming. All Rights Reserved.
