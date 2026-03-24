// BaseXMeshFactory.h — Generates procedural static meshes for structure keywords.
// Creates proper UStaticMesh assets saved to /Game/BaseX/Meshes/ so they appear
// in the Content Browser and can be referenced by the structure DataTable.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

namespace BaseXMeshFactory
{
    // Ensures all structure meshes exist as saved .uassets under /Game/BaseX/Meshes/.
    // Editor-only (creates + saves). Idempotent — skips meshes that already exist.
    void EnsureStructureMeshesExist();

    // Load a generated mesh by keyword name (e.g. "pine", "castle", "torch").
    // Returns nullptr if the mesh hasn't been generated yet.
    UStaticMesh* LoadMesh(const FString& Keyword);
}
