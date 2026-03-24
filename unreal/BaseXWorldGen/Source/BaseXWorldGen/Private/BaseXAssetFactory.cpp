// BaseXAssetFactory.cpp - Lazy material creation for terrain/water/structures.
// Materials are created on first Load*() call, not at module startup.

#include "BaseXAssetFactory.h"
#include "BaseXMeshFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

static UMaterial* CreateAndSaveMaterial(const FString& AssetPath, const FString& AssetName, TFunction<void(UMaterial*)> SetupFunc)
{
    FString FullPath = AssetPath + TEXT(".") + AssetName;
    if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *FullPath))
        return Existing;
    if (!GEngine || !IsInGameThread())
        return nullptr;

    UPackage* Pkg = CreatePackage(*AssetPath);
    if (!Pkg) return nullptr;
    Pkg->FullyLoad();

    UMaterial* Mat = NewObject<UMaterial>(Pkg, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    SetupFunc(Mat);
    Mat->PreEditChange(nullptr);
    Mat->PostEditChange();

    FAssetRegistryModule::AssetCreated(Mat);
    Pkg->MarkPackageDirty();
    FString FilePath = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Pkg, Mat, *FilePath, SaveArgs);
    UE_LOG(LogTemp, Log, TEXT("BaseXAssets: Created %s (saved=%d)"), *AssetPath, bSaved);
    return Mat;
}

static UMaterial* EnsureTerrainMaterial()
{
    return CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials/M_BaseX_Terrain"), TEXT("M_BaseX_Terrain"), [](UMaterial* Mat) {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        UMaterialExpressionVertexColor* VC = NewObject<UMaterialExpressionVertexColor>(Mat);
        Mat->GetExpressionCollection().AddExpression(VC);
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.8f;
        Mat->GetExpressionCollection().AddExpression(Rough);
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, VC);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
    });
}

static UMaterial* EnsureWaterMaterial()
{
    return CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials/M_BaseX_Water"), TEXT("M_BaseX_Water"), [](UMaterial* Mat) {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        Mat->BlendMode = BLEND_Translucent;

        // Base color — deep blue-green
        UMaterialExpressionConstant4Vector* Color = NewObject<UMaterialExpressionConstant4Vector>(Mat);
        Color->Constant = FLinearColor(0.02f, 0.12f, 0.35f, 1.0f);
        Mat->GetExpressionCollection().AddExpression(Color);

        // Opacity
        UMaterialExpressionConstant* Opacity = NewObject<UMaterialExpressionConstant>(Mat);
        Opacity->R = 0.55f;
        Mat->GetExpressionCollection().AddExpression(Opacity);

        // Low roughness for reflective water
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.05f;
        Mat->GetExpressionCollection().AddExpression(Rough);

        // Slight metallic sheen for specular highlights
        UMaterialExpressionConstant* Metal = NewObject<UMaterialExpressionConstant>(Mat);
        Metal->R = 0.02f;
        Mat->GetExpressionCollection().AddExpression(Metal);

        // World Position Offset — smooth sine wave displacement
        // Uses world position XY + Time to create layered wave motion
        UMaterialExpressionWorldPosition* WorldPos = NewObject<UMaterialExpressionWorldPosition>(Mat);
        Mat->GetExpressionCollection().AddExpression(WorldPos);

        UMaterialExpressionTime* Time = NewObject<UMaterialExpressionTime>(Mat);
        Mat->GetExpressionCollection().AddExpression(Time);

        // Panner for wave 1 (broad swell)
        UMaterialExpressionPanner* Panner1 = NewObject<UMaterialExpressionPanner>(Mat);
        Panner1->SpeedX = 0.03f;
        Panner1->SpeedY = 0.02f;
        Mat->GetExpressionCollection().AddExpression(Panner1);

        // Panner for wave 2 (cross ripple)
        UMaterialExpressionPanner* Panner2 = NewObject<UMaterialExpressionPanner>(Mat);
        Panner2->SpeedX = -0.02f;
        Panner2->SpeedY = 0.04f;
        Mat->GetExpressionCollection().AddExpression(Panner2);

        // Scale world position for wave frequency
        UMaterialExpressionConstant* WaveScale = NewObject<UMaterialExpressionConstant>(Mat);
        WaveScale->R = 0.002f;  // 1 wave per ~500 UE units
        Mat->GetExpressionCollection().AddExpression(WaveScale);

        // Wave amplitude (Z displacement)
        UMaterialExpressionConstant* WaveAmplitude = NewObject<UMaterialExpressionConstant>(Mat);
        WaveAmplitude->R = 15.0f;  // 15cm wave height
        Mat->GetExpressionCollection().AddExpression(WaveAmplitude);

        // Connect material outputs
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, Color);
        Mat->GetEditorOnlyData()->Opacity.Connect(0, Opacity);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
        Mat->GetEditorOnlyData()->Metallic.Connect(0, Metal);

        // Note: Full WPO sine wave graph requires material graph nodes
        // that are best connected via Blueprint Material Function.
        // The tessellated mesh + these material properties provide the
        // smooth water look. For full WPO, assign a custom Water material
        // via the WaterMaterial property on ABaseXWorldMesh.
    });
}

static UMaterial* EnsureStructureMaterial()
{
    return CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials/M_BaseX_Structure"), TEXT("M_BaseX_Structure"), [](UMaterial* Mat) {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        Mat->bUsedWithInstancedStaticMeshes = true;
        UMaterialExpressionVectorParameter* ColorParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
        ColorParam->ParameterName = TEXT("BaseColor");
        ColorParam->DefaultValue = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
        Mat->GetExpressionCollection().AddExpression(ColorParam);
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.6f;
        Mat->GetExpressionCollection().AddExpression(Rough);
        UMaterialExpressionScalarParameter* MetalParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
        MetalParam->ParameterName = TEXT("Metallic");
        MetalParam->DefaultValue = 0.1f;
        Mat->GetExpressionCollection().AddExpression(MetalParam);
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, ColorParam);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
        Mat->GetEditorOnlyData()->Metallic.Connect(0, MetalParam);
    });
}
#endif

void BaseXAssets::EnsureMaterialsExist()
{
#if WITH_EDITOR
    EnsureTerrainMaterial();
    EnsureWaterMaterial();
    EnsureStructureMaterial();
    BaseXMeshFactory::EnsureStructureMeshesExist();
    EnsureStructureTableExists();
#endif
}

UMaterialInterface* BaseXAssets::LoadTerrainMaterial()
{
    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Terrain.M_BaseX_Terrain"));
#if WITH_EDITOR
    if (!Mat) Mat = EnsureTerrainMaterial();
#endif
    return Mat;
}

UMaterialInterface* BaseXAssets::LoadWaterMaterial()
{
    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Water.M_BaseX_Water"));
#if WITH_EDITOR
    if (!Mat) Mat = EnsureWaterMaterial();
#endif
    return Mat;
}

UMaterialInterface* BaseXAssets::LoadStructureMaterial()
{
    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Structure.M_BaseX_Structure"));
#if WITH_EDITOR
    if (!Mat) Mat = EnsureStructureMaterial();
#endif
    return Mat;
}

// ══════════════════════════════════════════════════════════════════════
// STRUCTURE DATATABLE — persistent .uasset editable in UE DataTable editor
// ══════════════════════════════════════════════════════════════════════

#include "ABaseXStructureSpawner.h"  // for FBaseXStructureMeshRow
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"

// Shared helper: populates a DataTable with all keyword→BasicShape rows
static void PopulateStructureRows(UDataTable* DT)
{
    UStaticMesh* Cube     = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Sphere   = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cone     = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* Plane    = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

    auto AddRow = [&](const TCHAR* Name, UStaticMesh* FallbackMesh, float Scale, FVector Offset = FVector::ZeroVector)
    {
        FBaseXStructureMeshRow Row;
        // Prefer generated procedural mesh, fall back to BasicShape
        UStaticMesh* Generated = BaseXMeshFactory::LoadMesh(Name);
        Row.Mesh = Generated ? Generated : FallbackMesh;
        Row.ScaleMultiplier = Generated ? 1.0f : Scale;  // generated meshes are pre-sized
        Row.PivotOffset = Generated ? FVector::ZeroVector : Offset;
        DT->AddRow(FName(Name), Row);
    };

    // ── Conifers (tall cone) ──
    for (auto* K : { TEXT("pine"), TEXT("spruce"), TEXT("cedar"), TEXT("fir"),
                     TEXT("redwood"), TEXT("cypress") })
        AddRow(K, Cone, 2.5f, FVector(0, 0, 80));

    // ── Broadleaf trees (sphere canopy) ──
    for (auto* K : { TEXT("oak"), TEXT("willow"), TEXT("birch"), TEXT("maple"),
                     TEXT("elm"), TEXT("banyan"), TEXT("ash_tree"), TEXT("magnolia") })
        AddRow(K, Sphere, 1.5f, FVector(0, 0, 120));

    // ── Tropical trees (tall sphere) ──
    for (auto* K : { TEXT("palm"), TEXT("acacia"), TEXT("baobab"), TEXT("mangrove") })
        AddRow(K, Sphere, 1.8f, FVector(0, 0, 140));

    // ── Generic tree / stump / log ──
    AddRow(TEXT("tree"), Cone, 2.0f, FVector(0, 0, 80));
    AddRow(TEXT("stump"), Cylinder, 0.3f);
    AddRow(TEXT("log"), Cylinder, 0.4f, FVector(0, 0, 10));

    // ── Small vegetation ──
    for (auto* K : { TEXT("shrub"), TEXT("bush"), TEXT("hedge"), TEXT("fern"),
                     TEXT("flower"), TEXT("garden"), TEXT("topiary"), TEXT("grass_tuft"),
                     TEXT("vine"), TEXT("moss_patch"), TEXT("fairy_ring"), TEXT("flower_bed"),
                     TEXT("root") })
        AddRow(K, Sphere, 0.4f);

    // ── Water plants (flat) ──
    for (auto* K : { TEXT("cattail"), TEXT("lily_pad"), TEXT("tidepool") })
        AddRow(K, Plane, 0.3f, FVector(0, 0, 2));

    // ── Mushrooms ──
    AddRow(TEXT("mushroom_cluster"), Sphere, 0.4f);

    // ── Rocks / boulders ──
    for (auto* K : { TEXT("rock"), TEXT("boulder"), TEXT("outcrop"), TEXT("cliff_face"),
                     TEXT("sea_stack"), TEXT("rock_arch") })
        AddRow(K, Sphere, 0.8f);

    // ── Cave / crystal (tall narrow) ──
    for (auto* K : { TEXT("stalagmite"), TEXT("stalactite"), TEXT("crystal_cluster"),
                     TEXT("crystal_spire"), TEXT("geode") })
        AddRow(K, Cone, 1.2f, FVector(0, 0, 40));

    // ── Thermal / water features (flat glow) ──
    for (auto* K : { TEXT("hot_spring"), TEXT("geyser"), TEXT("lava_pool"),
                     TEXT("volcanic_vent"), TEXT("cave_mouth") })
        AddRow(K, Sphere, 0.6f);

    // ── Coral ──
    AddRow(TEXT("coral_formation"), Sphere, 0.5f);

    // ── Tall architecture (towers, columns) ──
    for (auto* K : { TEXT("tower"), TEXT("spire"), TEXT("obelisk"), TEXT("minaret"),
                     TEXT("lighthouse"), TEXT("monolith"), TEXT("pagoda"), TEXT("pillar"),
                     TEXT("torii"), TEXT("column"), TEXT("bell_tower"), TEXT("lamp_post"),
                     TEXT("collapsed_tower") })
        AddRow(K, Cylinder, 2.0f, FVector(0, 0, 60));

    // ── Large buildings ──
    for (auto* K : { TEXT("castle"), TEXT("fortress"), TEXT("cathedral"), TEXT("palace"),
                     TEXT("citadel"), TEXT("temple"), TEXT("keep"), TEXT("colosseum"),
                     TEXT("amphitheater"), TEXT("arena"), TEXT("monastery"), TEXT("mansion"),
                     TEXT("villa"), TEXT("warehouse"), TEXT("mausoleum"), TEXT("barn"),
                     TEXT("stable"), TEXT("granary"), TEXT("barracks") })
        AddRow(K, Cube, 1.8f, FVector(0, 0, 50));

    // ── Small buildings ──
    for (auto* K : { TEXT("hut"), TEXT("cabin"), TEXT("cottage"), TEXT("shack"),
                     TEXT("shed"), TEXT("farmhouse"), TEXT("chapel"), TEXT("forge"),
                     TEXT("workshop"), TEXT("smithy"), TEXT("tavern"), TEXT("inn"),
                     TEXT("bakery"), TEXT("apothecary"), TEXT("guardhouse"), TEXT("outhouse"),
                     TEXT("boathouse"), TEXT("market_stall"), TEXT("lean_to"), TEXT("windmill"),
                     TEXT("tomb"), TEXT("shrine"), TEXT("marketplace") })
        AddRow(K, Cube, 0.9f, FVector(0, 0, 30));

    // ── Flat / platform structures ──
    for (auto* K : { TEXT("bridge"), TEXT("dock"), TEXT("pier"), TEXT("jetty"),
                     TEXT("wharf"), TEXT("platform"), TEXT("terrace"), TEXT("stairway"),
                     TEXT("aqueduct"), TEXT("colonnade"), TEXT("drawbridge") })
        AddRow(K, Cube, 1.5f);

    // ── Walls / fences ──
    for (auto* K : { TEXT("wall"), TEXT("fence"), TEXT("rampart"), TEXT("palisade"),
                     TEXT("parapet"), TEXT("battlement"), TEXT("gatehouse"), TEXT("gate"),
                     TEXT("arch"), TEXT("archway"), TEXT("portcullis"), TEXT("ruin_arch") })
        AddRow(K, Cube, 1.2f, FVector(0, 0, 40));

    // ── Domed / low circular ──
    for (auto* K : { TEXT("dome"), TEXT("cupola"), TEXT("altar"), TEXT("dolmen"),
                     TEXT("barrow"), TEXT("well"), TEXT("fountain"), TEXT("bird_bath"),
                     TEXT("sundial"), TEXT("gazebo"), TEXT("pergola"), TEXT("summoning_circle") })
        AddRow(K, Sphere, 0.6f);

    // ── Pyramids / ziggurats ──
    AddRow(TEXT("pyramid"), Cone, 2.0f, FVector(0, 0, 30));
    AddRow(TEXT("ziggurat"), Cone, 2.0f, FVector(0, 0, 30));

    // ── Tents ──
    AddRow(TEXT("tent"), Cone, 0.7f);

    // ── Small props (crates, barrels) ──
    for (auto* K : { TEXT("crate"), TEXT("chest"), TEXT("barrel"), TEXT("cart"),
                     TEXT("wagon"), TEXT("anchor"), TEXT("sarcophagus") })
        AddRow(K, Cube, 0.3f);

    // ── Light sources / orbs ──
    for (auto* K : { TEXT("torch"), TEXT("lantern"), TEXT("brazier"), TEXT("campfire"),
                     TEXT("bonfire"), TEXT("soul_lantern"), TEXT("crystal_orb") })
        AddRow(K, Sphere, 0.2f, FVector(0, 0, 20));

    // ── Signs / markers / graves ──
    for (auto* K : { TEXT("signpost"), TEXT("banner"), TEXT("flag"), TEXT("gravestone"),
                     TEXT("headstone"), TEXT("cross"), TEXT("memorial"), TEXT("cenotaph"),
                     TEXT("runestone"), TEXT("ward_stone") })
        AddRow(K, Cube, 0.5f, FVector(0, 0, 20));

    // ── Fantasy portals / rifts ──
    AddRow(TEXT("portal"), Cylinder, 1.8f, FVector(0, 0, 60));
    AddRow(TEXT("rift"), Cylinder, 1.8f, FVector(0, 0, 60));
    AddRow(TEXT("enchanted_well"), Sphere, 0.5f);

    // ── Siege weapons ──
    for (auto* K : { TEXT("cannon"), TEXT("catapult"), TEXT("ballista") })
        AddRow(K, Cube, 0.5f);

    // ── Ruins / rubble ──
    for (auto* K : { TEXT("ruins"), TEXT("rubble"), TEXT("crumbled_wall"),
                     TEXT("broken_pillar"), TEXT("shipwreck"), TEXT("sunken_ship") })
        AddRow(K, Cube, 0.7f);

    // ── Statues / thrones ──
    AddRow(TEXT("statue"), Cylinder, 1.2f, FVector(0, 0, 50));
    AddRow(TEXT("throne"), Cube, 0.6f, FVector(0, 0, 15));
}

// Editor-only: create and save a persistent DataTable .uasset
#if WITH_EDITOR
void BaseXAssets::EnsureStructureTableExists()
{
    const FString AssetPath = TEXT("/Game/BaseX/Data/DT_BaseX_Structures");
    const FString AssetName = TEXT("DT_BaseX_Structures");
    const FString FullPath  = AssetPath + TEXT(".") + AssetName;

    if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, *FullPath))
    {
        UE_LOG(LogTemp, Log, TEXT("BaseXAssets: Structure DataTable exists at %s (%d rows)"),
            *AssetPath, Existing->GetRowMap().Num());
        return;
    }

    if (!GEngine || !IsInGameThread()) return;

    UPackage* Pkg = CreatePackage(*AssetPath);
    if (!Pkg) return;
    Pkg->FullyLoad();

    UDataTable* DT = NewObject<UDataTable>(Pkg, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    DT->RowStruct = FBaseXStructureMeshRow::StaticStruct();

    PopulateStructureRows(DT);

    FAssetRegistryModule::AssetCreated(DT);
    Pkg->MarkPackageDirty();

    FString FilePath = FPackageName::LongPackageNameToFilename(
        Pkg->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Pkg, DT, *FilePath, SaveArgs);

    UE_LOG(LogTemp, Warning, TEXT("BaseXAssets: Created structure DataTable at %s (%d rows, saved=%d)"),
        *AssetPath, DT->GetRowMap().Num(), bSaved);
    UE_LOG(LogTemp, Warning, TEXT("  >> Open in UE: Content Browser → BaseX/Data/DT_BaseX_Structures"));
    UE_LOG(LogTemp, Warning, TEXT("  >> Replace Mesh column with your custom .uasset meshes"));
}
#else
void BaseXAssets::EnsureStructureTableExists() {}
#endif

// Runtime loader: load saved .uasset, fallback to in-memory table
UDataTable* BaseXAssets::LoadStructureTable()
{
    const FString FullPath = TEXT("/Game/BaseX/Data/DT_BaseX_Structures.DT_BaseX_Structures");
    UDataTable* DT = LoadObject<UDataTable>(nullptr, *FullPath);
    if (DT)
    {
        UE_LOG(LogTemp, Log, TEXT("BaseXAssets: Loaded saved structure DataTable (%d rows)"),
            DT->GetRowMap().Num());
        return DT;
    }
#if WITH_EDITOR
    EnsureStructureTableExists();
    DT = LoadObject<UDataTable>(nullptr, *FullPath);
    if (DT) return DT;
#endif
    // Ultimate fallback: transient in-memory table
    return BuildDefaultStructureTable();
}

// In-memory fallback (no .uasset)
UDataTable* BaseXAssets::BuildDefaultStructureTable()
{
    static TWeakObjectPtr<UDataTable> CachedTable;
    if (CachedTable.IsValid()) return CachedTable.Get();

    UDataTable* DT = NewObject<UDataTable>(GetTransientPackage(), TEXT("BX_DefaultStructureTable"));
    DT->RowStruct = FBaseXStructureMeshRow::StaticStruct();
    DT->AddToRoot();

    PopulateStructureRows(DT);

    CachedTable = DT;
    UE_LOG(LogTemp, Log, TEXT("BaseXAssets: Built transient structure DataTable (%d rows)"),
        DT->GetRowMap().Num());
    return DT;
}
