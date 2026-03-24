// BaseXAssetFactory.h — Creates proper UE material/mesh assets for the world builder.
// Materials are saved as .uasset on first editor load, then loaded by path at runtime.

#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UMaterialInterface;
class UDataTable;

namespace BaseXAssets
{
    // Content paths for the generated assets
    static const TCHAR* TerrainMaterialPath   = TEXT("/Game/BaseX/Materials/M_BaseX_Terrain");
    static const TCHAR* WaterMaterialPath     = TEXT("/Game/BaseX/Materials/M_BaseX_Water");
    static const TCHAR* StructureMaterialPath = TEXT("/Game/BaseX/Materials/M_BaseX_Structure");
    static const TCHAR* StructureTablePath    = TEXT("/Game/BaseX/Data/DT_BaseX_Structures");

    // Called on module startup (editor only) — creates materials + DataTable if missing
    void EnsureMaterialsExist();
    void EnsureStructureTableExists();

    // Runtime loaders — returns nullptr if asset doesn't exist yet
    UMaterialInterface* LoadTerrainMaterial();
    UMaterialInterface* LoadWaterMaterial();
    UMaterialInterface* LoadStructureMaterial();

    // Loads the saved structure DataTable .uasset. Creates it in-editor if missing.
    UDataTable* LoadStructureTable();

    // Builds a transient (in-memory) DataTable as fallback when no .uasset exists.
    UDataTable* BuildDefaultStructureTable();
}
