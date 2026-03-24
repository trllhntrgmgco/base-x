# Collaboration Quick Reference

## File Ownership (Zero-Conflict Zones)

### ✅ You Work On:
```
src/core/                  # Engine core
src/physics/               # Bullet integration
include/BaseX/             # Public API
unreal/BaseXWorldGen/      # Marketplace plugin
pack_marketplace.ps1       # Plugin packaging
```

### ✅ Berlin Works On:
```
src/demos/NEC/             # All NEC demos
export/NecrotopiaVR/       # Exported JSON data
docs/NEC_*.md              # NEC documentation
```

### ⚠️ Coordinate Before Editing:
```
CMakeLists.txt
src/demos/CMakeLists.txt
README.md
vcpkg.json
```

---

## Daily Workflow

### Your Morning (9 AM EST):
```powershell
git checkout master
git pull origin master           # Get Berlin's overnight work
git checkout -b feature/keenan-v1.2-spatial
# Work on core engine / plugin...
git push origin feature/keenan-v1.2-spatial
# Open PR
```

### Berlin's Morning (9 AM CET = 3 AM EST):
```sh
git checkout master
git pull origin master           # Get your overnight work
git checkout -b feature/berlin-nec-002-lust
# Work on NEC demos...
git push origin feature/berlin-nec-002-lust
# Open PR
```

### Overlap Window (3 PM - 11 PM CET = 9 AM - 5 PM EST):
- Live collaboration via Slack Huddles
- VS Code Live Share for pair programming
- Real-time code reviews

---

## Communication Shortcuts

### Before Editing Shared File:
```
Slack #base-x-engine:
@Berlin: "Editing CMakeLists.txt for next 15 min"
@Keenan: "👍 OK"
```

### When PR Ready:
```
GitHub auto-notifies based on CODEOWNERS
Berlin's NEC PR → @Keenan reviews
Your core PR → @Berlin sees it (optional review)
```

### Emergency (Both in Same File):
```
Slack: "Berlin, join Live Share: https://..."
# Work together in real-time
```

---

## Conflict Resolution (Rare)

### If CMakeLists.txt Conflict:
```sh
git checkout master
git pull origin master
git checkout feature/berlin-nec-002
git merge master
# Resolve conflict (keep both changes)
git add CMakeLists.txt
git commit -m "Merge master, resolve conflict"
git push
```

**Most conflicts auto-resolve** because you work in different directories.

---

## Build Verification

### Before Opening PR:
```powershell
# Build locally
cmake --build build --target nec_all

# If success, push
git push origin feature/berlin-nec-002-lust

# CI will auto-build again and notify Slack
```

---

## File Lock (Critical Files Only)

### If editing proprietary DRO code:
```powershell
git lfs lock src/core/DRO.cpp
# Edit...
git commit -am "Update DRO algorithm"
git lfs unlock src/core/DRO.cpp
```

Berlin **cannot** edit while locked.

---

**Summary:** Different directories = different worlds = zero conflicts 🎯
