// ABaseXStructureSpawner.cpp — Spawns structure actors from world builder output

#include "ABaseXStructureSpawner.h"
#include "UBaseXWorldBuilder.h"
#include "BaseXAssetFactory.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ABaseXStructureSpawner::ABaseXStructureSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

void ABaseXStructureSpawner::ClearStructures()
{
    for (auto& Comp : InstancedGroups)
    {
        if (IsValid(Comp))
        {
            Comp->ClearInstances();
            Comp->DestroyComponent();
        }
    }
    InstancedGroups.Empty();
}

void ABaseXStructureSpawner::SpawnStructures(UBaseXWorldBuilder* Builder)
{
    if (!Builder || !Builder->IsComplete()) return;

    ClearStructures();

    // Auto-load the DataTable from saved .uasset if user hasn't assigned one
    if (!StructureMeshTable)
    {
        StructureMeshTable = BaseXAssets::LoadStructureTable();
        UE_LOG(LogTemp, Log, TEXT("BaseXStructureSpawner: Auto-loaded structure DataTable (%d rows)"),
            StructureMeshTable ? StructureMeshTable->GetRowMap().Num() : 0);
    }

    const float CellSize = Builder->CellWorldSize;
    const float HScale = Builder->HeightScale;
    const float HalfWorld = (Builder->GetGridSize() - 1) * CellSize * 0.5f;
    const FVector SpawnerOrigin = GetActorLocation();

    UE_LOG(LogTemp, Log, TEXT("BaseXStructureSpawner: Origin=(%.0f, %.0f, %.0f) CellSize=%.0f HScale=%.0f HalfWorld=%.0f"),
        SpawnerOrigin.X, SpawnerOrigin.Y, SpawnerOrigin.Z, CellSize, HScale, HalfWorld);

    // Engine primitive meshes as fallback
    static UStaticMesh* CubeMesh = nullptr;
    static UStaticMesh* CylinderMesh = nullptr;
    static UStaticMesh* SphereMesh = nullptr;
    static UStaticMesh* ConeMesh = nullptr;
    static UStaticMesh* PlaneMesh = nullptr;
    if (!CubeMesh)
        CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!CylinderMesh)
        CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!SphereMesh)
        SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!ConeMesh)
        ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (!PlaneMesh)
        PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

    // ── Collect instances grouped by mesh for GPU instancing ──
    struct FPendingInstance { FTransform Transform; FLinearColor Color; };
    TMap<UStaticMesh*, TArray<FPendingInstance>> MeshGroups;

    for (const FBaseXStructurePlacement& S : Builder->GetStructurePlacements())
    {
        FVector Location(
            SpawnerOrigin.X + S.GridX * CellSize - HalfWorld,
            SpawnerOrigin.Y + S.GridY * CellSize - HalfWorld,
            SpawnerOrigin.Z + S.Height * HScale
        );

        // Look up mesh from DataTable
        UStaticMesh* Mesh = FallbackMesh;
        float ScaleMult = 1.f;
        FVector PivotOff = FVector::ZeroVector;

        if (StructureMeshTable)
        {
            FName RowName(*S.Type);
            FBaseXStructureMeshRow* Row = StructureMeshTable->FindRow<FBaseXStructureMeshRow>(
                RowName, TEXT("BaseXStructureSpawner"));
            if (Row && !Row->Mesh.IsNull())
            {
                Mesh = Row->Mesh.LoadSynchronous();
                ScaleMult = Row->ScaleMultiplier;
                PivotOff = Row->PivotOffset;
            }
        }

        // Pick shape by structure type — 14 visual categories mapped to UE primitives
        enum EShapeType { Shape_Cube, Shape_Cylinder, Shape_Sphere, Shape_Cone, Shape_Plane };
        EShapeType ShapeType = Shape_Cube;
        float HeightMult = 1.5f;
        float WidthMult = 0.8f;

        if (!Mesh)
        {
            FString T = S.Type.ToLower();

            // ── Nature: Conifers (tall cone) ──
            if (T.Contains(TEXT("pine")) || T.Contains(TEXT("spruce")) || T.Contains(TEXT("cedar"))
                || T.Contains(TEXT("fir")) || T.Contains(TEXT("redwood")) || T.Contains(TEXT("cypress")))
            {
                ShapeType = Shape_Cone;
                Mesh = ConeMesh ? ConeMesh : CylinderMesh;
                HeightMult = 3.5f;
                WidthMult = 0.5f;
            }
            // ── Nature: Broadleaf trees (sphere canopy) ──
            else if (T.Contains(TEXT("oak")) || T.Contains(TEXT("willow")) || T.Contains(TEXT("birch"))
                || T.Contains(TEXT("maple")) || T.Contains(TEXT("elm")) || T.Contains(TEXT("banyan"))
                || T.Contains(TEXT("ash_tree")) || T.Contains(TEXT("magnolia")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 1.8f;
                WidthMult = 1.2f;
            }
            // ── Nature: Tropical / savanna trees (tall sphere) ──
            else if (T.Contains(TEXT("palm")) || T.Contains(TEXT("acacia")) || T.Contains(TEXT("baobab"))
                || T.Contains(TEXT("mangrove")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 2.2f;
                WidthMult = 0.8f;
            }
            // ── Generic tree / stump / log ──
            else if (T.Contains(TEXT("tree")) || T.Contains(TEXT("stump")) || T.Contains(TEXT("log")))
            {
                ShapeType = Shape_Cone;
                Mesh = ConeMesh ? ConeMesh : CylinderMesh;
                HeightMult = 3.0f;
                WidthMult = 0.6f;
            }
            // ── Nature: Small vegetation (small sphere) ──
            else if (T.Contains(TEXT("shrub")) || T.Contains(TEXT("bush")) || T.Contains(TEXT("hedge"))
                || T.Contains(TEXT("fern")) || T.Contains(TEXT("flower")) || T.Contains(TEXT("garden"))
                || T.Contains(TEXT("topiary")) || T.Contains(TEXT("grass_tuft"))
                || T.Contains(TEXT("vine")) || T.Contains(TEXT("moss_patch"))
                || T.Contains(TEXT("fairy_ring")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.4f;
                WidthMult = 0.6f;
            }
            // ── Nature: Water plants (very flat sphere) ──
            else if (T.Contains(TEXT("cattail")) || T.Contains(TEXT("lily_pad"))
                || T.Contains(TEXT("tidepool")))
            {
                ShapeType = Shape_Plane;
                Mesh = PlaneMesh ? PlaneMesh : SphereMesh;
                HeightMult = 0.05f;
                WidthMult = 0.5f;
            }
            // ── Mushrooms ──
            else if (T.Contains(TEXT("mushroom")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.5f;
                WidthMult = 0.6f;
            }
            // ── Geological: Rocks / boulders / outcrops (irregular sphere) ──
            else if (T.Contains(TEXT("rock")) || T.Contains(TEXT("boulder"))
                || T.Contains(TEXT("outcrop")) || T.Contains(TEXT("cliff_face"))
                || T.Contains(TEXT("sea_stack")) || T.Contains(TEXT("rock_arch")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.7f;
                WidthMult = 0.9f;
            }
            // ── Geological: Cave / crystal (tall cylinder/sphere) ──
            else if (T.Contains(TEXT("stalagmite")) || T.Contains(TEXT("stalactite"))
                || T.Contains(TEXT("crystal_cluster")) || T.Contains(TEXT("crystal_spire"))
                || T.Contains(TEXT("geode")))
            {
                ShapeType = Shape_Cone;
                Mesh = ConeMesh ? ConeMesh : CylinderMesh;
                HeightMult = 2.0f;
                WidthMult = 0.3f;
            }
            // ── Geological: Thermal / water features (flat sphere + glow) ──
            else if (T.Contains(TEXT("hot_spring")) || T.Contains(TEXT("geyser"))
                || T.Contains(TEXT("lava_pool")) || T.Contains(TEXT("volcanic_vent"))
                || T.Contains(TEXT("cave_mouth")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.2f;
                WidthMult = 1.0f;
            }
            // ── Geological: Coral ──
            else if (T.Contains(TEXT("coral")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.6f;
                WidthMult = 0.7f;
            }
            // ── Architecture: Tall / Towers / Columns ──
            else if (T.Contains(TEXT("tower")) || T.Contains(TEXT("spire"))
                || T.Contains(TEXT("obelisk")) || T.Contains(TEXT("minaret"))
                || T.Contains(TEXT("lighthouse")) || T.Contains(TEXT("monolith"))
                || T.Contains(TEXT("pagoda")) || T.Contains(TEXT("pillar"))
                || T.Contains(TEXT("torii")) || T.Contains(TEXT("column"))
                || T.Contains(TEXT("bell_tower")) || T.Contains(TEXT("chimney"))
                || T.Contains(TEXT("lamp_post")) || T.Contains(TEXT("flagpole"))
                || T.Contains(TEXT("watchtower")) || T.Contains(TEXT("collapsed_tower")))
            {
                ShapeType = Shape_Cylinder;
                Mesh = CylinderMesh;
                HeightMult = 3.0f;
                WidthMult = 0.3f;
            }
            // ── Architecture: Large buildings (tall wide cube) ──
            else if (T.Contains(TEXT("castle")) || T.Contains(TEXT("fortress"))
                || T.Contains(TEXT("cathedral")) || T.Contains(TEXT("palace"))
                || T.Contains(TEXT("citadel")) || T.Contains(TEXT("temple"))
                || T.Contains(TEXT("keep")) || T.Contains(TEXT("colosseum"))
                || T.Contains(TEXT("amphitheater")) || T.Contains(TEXT("arena"))
                || T.Contains(TEXT("monastery")) || T.Contains(TEXT("mansion"))
                || T.Contains(TEXT("villa")) || T.Contains(TEXT("warehouse"))
                || T.Contains(TEXT("mausoleum")) || T.Contains(TEXT("barn"))
                || T.Contains(TEXT("stable")) || T.Contains(TEXT("granary"))
                || T.Contains(TEXT("barracks")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 2.0f;
                WidthMult = 1.2f;
            }
            // ── Architecture: Small buildings (modest cube) ──
            else if (T.Contains(TEXT("hut")) || T.Contains(TEXT("cabin"))
                || T.Contains(TEXT("cottage")) || T.Contains(TEXT("shack"))
                || T.Contains(TEXT("shed")) || T.Contains(TEXT("farmhouse"))
                || T.Contains(TEXT("chapel")) || T.Contains(TEXT("forge"))
                || T.Contains(TEXT("workshop")) || T.Contains(TEXT("smithy"))
                || T.Contains(TEXT("tavern")) || T.Contains(TEXT("inn"))
                || T.Contains(TEXT("bakery")) || T.Contains(TEXT("apothecary"))
                || T.Contains(TEXT("guardhouse")) || T.Contains(TEXT("outhouse"))
                || T.Contains(TEXT("boathouse")) || T.Contains(TEXT("market_stall"))
                || T.Contains(TEXT("lean_to")) || T.Contains(TEXT("windmill"))
                || T.Contains(TEXT("tomb")) || T.Contains(TEXT("shrine"))
                || T.Contains(TEXT("crypt")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 1.2f;
                WidthMult = 0.8f;
            }
            // ── Architecture: Flat / Platform (very flat cube) ──
            else if (T.Contains(TEXT("bridge")) || T.Contains(TEXT("dock"))
                || T.Contains(TEXT("pier")) || T.Contains(TEXT("jetty"))
                || T.Contains(TEXT("wharf")) || T.Contains(TEXT("platform"))
                || T.Contains(TEXT("terrace")) || T.Contains(TEXT("stairway"))
                || T.Contains(TEXT("aqueduct")) || T.Contains(TEXT("colonnade"))
                || T.Contains(TEXT("drawbridge")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 0.15f;
                WidthMult = 2.5f;
            }
            // ── Architecture: Walls / Fences (tall flat cube) ──
            else if (T.Contains(TEXT("wall")) || T.Contains(TEXT("fence"))
                || T.Contains(TEXT("rampart")) || T.Contains(TEXT("palisade"))
                || T.Contains(TEXT("parapet")) || T.Contains(TEXT("battlement"))
                || T.Contains(TEXT("gatehouse")) || T.Contains(TEXT("gate"))
                || T.Contains(TEXT("arch")) || T.Contains(TEXT("portcullis"))
                || T.Contains(TEXT("ruin_arch")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 2.0f;
                WidthMult = 0.15f;
            }
            // ── Domed / Low circular (squat sphere) ──
            else if (T.Contains(TEXT("dome")) || T.Contains(TEXT("cupola"))
                || T.Contains(TEXT("altar")) || T.Contains(TEXT("dolmen"))
                || T.Contains(TEXT("barrow")) || T.Contains(TEXT("well"))
                || T.Contains(TEXT("fountain")) || T.Contains(TEXT("bird_bath"))
                || T.Contains(TEXT("sundial")) || T.Contains(TEXT("gazebo"))
                || T.Contains(TEXT("pergola")) || T.Contains(TEXT("summoning_circle")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.6f;
                WidthMult = 0.8f;
            }
            // ── Pyramids / Ziggurats (wide cone) ──
            else if (T.Contains(TEXT("pyramid")) || T.Contains(TEXT("ziggurat")))
            {
                ShapeType = Shape_Cone;
                Mesh = ConeMesh ? ConeMesh : CubeMesh;
                HeightMult = 2.0f;
                WidthMult = 0.8f;
            }
            // ── Tents (small cone) ──
            else if (T.Contains(TEXT("tent")))
            {
                ShapeType = Shape_Cone;
                Mesh = ConeMesh ? ConeMesh : CubeMesh;
                HeightMult = 1.0f;
                WidthMult = 0.7f;
            }
            // ── Props: Small objects (tiny cube) ──
            else if (T.Contains(TEXT("crate")) || T.Contains(TEXT("chest"))
                || T.Contains(TEXT("barrel")) || T.Contains(TEXT("cart"))
                || T.Contains(TEXT("wagon")) || T.Contains(TEXT("anchor"))
                || T.Contains(TEXT("sarcophagus")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 0.5f;
                WidthMult = 0.4f;
            }
            // ── Props: Light sources / markers (tiny sphere) ──
            else if (T.Contains(TEXT("torch")) || T.Contains(TEXT("lantern"))
                || T.Contains(TEXT("brazier")) || T.Contains(TEXT("campfire"))
                || T.Contains(TEXT("bonfire")) || T.Contains(TEXT("soul_lantern"))
                || T.Contains(TEXT("crystal_orb")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.3f;
                WidthMult = 0.2f;
            }
            // ── Props: Signs / markers / grave (thin tall cube) ──
            else if (T.Contains(TEXT("signpost")) || T.Contains(TEXT("banner"))
                || T.Contains(TEXT("flag")) || T.Contains(TEXT("gravestone"))
                || T.Contains(TEXT("headstone")) || T.Contains(TEXT("cross"))
                || T.Contains(TEXT("memorial")) || T.Contains(TEXT("cenotaph"))
                || T.Contains(TEXT("runestone")) || T.Contains(TEXT("ward_stone")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 1.0f;
                WidthMult = 0.1f;
            }
            // ── Fantasy: Portals / rifts (tall thin cylinder) ──
            else if (T.Contains(TEXT("portal")) || T.Contains(TEXT("rift")))
            {
                ShapeType = Shape_Cylinder;
                Mesh = CylinderMesh;
                HeightMult = 2.5f;
                WidthMult = 0.05f;
            }
            // ── Siege / military (wide cube) ──
            else if (T.Contains(TEXT("cannon")) || T.Contains(TEXT("catapult"))
                || T.Contains(TEXT("ballista")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 0.6f;
                WidthMult = 0.6f;
            }
            // ── Ruins / rubble (flat irregular) ──
            else if (T.Contains(TEXT("ruins")) || T.Contains(TEXT("rubble"))
                || T.Contains(TEXT("crumbled_wall")) || T.Contains(TEXT("broken_pillar"))
                || T.Contains(TEXT("shipwreck")) || T.Contains(TEXT("sunken_ship")))
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 0.4f;
                WidthMult = 1.0f;
            }
            // ── Default fallback ──
            else
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 1.5f;
                WidthMult = 0.8f;
            }
        }

        if (!Mesh) continue;

        // Offset upward so structures sit ON terrain, not inside it
        float StructHeight = S.Size * 30.f;  // approximate height in UE units
        Location.Z += StructHeight * 0.5f;

        // Scale: BasicShapes are 100uu. S.Size is 8-20. Target visible structures.
        float BaseScale = S.Size * 0.3f * ScaleMult;
        FVector Scale3D(BaseScale * WidthMult, BaseScale * WidthMult, BaseScale * HeightMult);

        FPendingInstance Inst;
        Inst.Transform = FTransform(FQuat::Identity, Location + PivotOff, Scale3D);
        Inst.Color = S.Color;
        MeshGroups.FindOrAdd(Mesh).Add(MoveTemp(Inst));
    }

    // ── Create batched HISMC per unique mesh (massive draw-call reduction) ──
    UMaterialInterface* BaseMat = FallbackMaterial;
    if (!BaseMat)
        BaseMat = BaseXAssets::LoadStructureMaterial();

    int32 TotalInstances = 0;
    for (auto& Pair : MeshGroups)
    {
        UStaticMesh* MeshPtr = Pair.Key;
        TArray<FPendingInstance>& Instances = Pair.Value;

        auto* HISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
        HISMC->SetStaticMesh(MeshPtr);
        HISMC->SetMobility(EComponentMobility::Movable);
        HISMC->SetCastShadow(true);
        HISMC->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
        HISMC->RegisterComponent();

        // Apply representative colour from the first instance in this group
        if (BaseMat)
        {
            UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(BaseMat, this);
            MI->SetVectorParameterValue(TEXT("BaseColor"), Instances[0].Color);
            HISMC->SetMaterial(0, MI);
        }

        for (const auto& Inst : Instances)
        {
            HISMC->AddInstance(Inst.Transform, /*bWorldSpace=*/ true);
        }

        TotalInstances += Instances.Num();
        InstancedGroups.Add(HISMC);
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXStructureSpawner: %d structures in %d batched groups (was %d draw calls)"),
        TotalInstances, InstancedGroups.Num(), TotalInstances);
}
