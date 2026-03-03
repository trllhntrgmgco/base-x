// BaseXAssetFactory.h — Creates proper UE material assets for the world builder.
// Materials are saved as .uasset on first editor load, then loaded by path at runtime.

#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UMaterialInterface;

namespace BaseXAssets
{
    // Content paths for the generated materials
    static const TCHAR* TerrainMaterialPath  = TEXT("/Game/BaseX/Materials/M_BaseX_Terrain");
    static const TCHAR* WaterMaterialPath    = TEXT("/Game/BaseX/Materials/M_BaseX_Water");
    static const TCHAR* StructureMaterialPath = TEXT("/Game/BaseX/Materials/M_BaseX_Structure");

    // Called on module startup (editor only) — creates materials if missing
    void EnsureMaterialsExist();

    // Runtime loaders — returns nullptr if asset doesn't exist yet
    UMaterialInterface* LoadTerrainMaterial();
    UMaterialInterface* LoadWaterMaterial();
    UMaterialInterface* LoadStructureMaterial();
}
