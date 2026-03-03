// ABaseXStructureSpawner.cpp — Spawns structure actors from world builder output

#include "ABaseXStructureSpawner.h"
#include "UBaseXWorldBuilder.h"
#include "BaseXAssetFactory.h"
#include "Components/StaticMeshComponent.h"
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
    for (auto& A : SpawnedActors)
    {
        if (IsValid(A)) A->Destroy();
    }
    SpawnedActors.Empty();
}

void ABaseXStructureSpawner::SpawnStructures(UBaseXWorldBuilder* Builder)
{
    if (!Builder || !Builder->IsComplete()) return;

    ClearStructures();

    const float CellSize = Builder->CellWorldSize;
    const float HScale = Builder->HeightScale;
    const float HalfWorld = (Builder->GetGridSize() - 1) * CellSize * 0.5f;

    // Engine primitive meshes as fallback
    static UStaticMesh* CubeMesh = nullptr;
    static UStaticMesh* CylinderMesh = nullptr;
    static UStaticMesh* SphereMesh = nullptr;
    static UStaticMesh* ConeMesh = nullptr;
    if (!CubeMesh)
        CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!CylinderMesh)
        CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!SphereMesh)
        SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!ConeMesh)
        ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));

    for (const FBaseXStructurePlacement& S : Builder->GetStructurePlacements())
    {
        FVector Location(
            S.GridX * CellSize - HalfWorld,
            S.GridY * CellSize - HalfWorld,
            S.Height * HScale
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

        // Pick shape by structure type
        enum EShapeType { Shape_Cube, Shape_Cylinder, Shape_Sphere, Shape_Cone };
        EShapeType ShapeType = Shape_Cube;
        float HeightMult = 1.5f;   // how tall relative to base
        float WidthMult = 0.8f;    // how wide

        if (!Mesh)
        {
            FString T = S.Type.ToLower();

            // Nature/foliage — cone trees, sphere shrubs, sphere rocks
            if (T.Contains(TEXT("tree")) || T.Contains(TEXT("stump")) || T.Contains(TEXT("log")))
            {
                ShapeType = Shape_Cone;
                Mesh = ConeMesh ? ConeMesh : CylinderMesh;
                HeightMult = 3.0f;
                WidthMult = 0.6f;
            }
            else if (T.Contains(TEXT("shrub")) || T.Contains(TEXT("mushroom")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.5f;
                WidthMult = 0.6f;
            }
            else if (T.Contains(TEXT("rock")) || T.Contains(TEXT("boulder")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.7f;
                WidthMult = 0.8f;
            }
            // Tall structures
            else if (T.Contains(TEXT("tower")) || T.Contains(TEXT("spire"))
                || T.Contains(TEXT("obelisk")) || T.Contains(TEXT("minaret"))
                || T.Contains(TEXT("lighthouse")) || T.Contains(TEXT("monolith"))
                || T.Contains(TEXT("pagoda")) || T.Contains(TEXT("pillar"))
                || T.Contains(TEXT("torii")))
            {
                ShapeType = Shape_Cylinder;
                Mesh = CylinderMesh;
                HeightMult = 3.0f;
                WidthMult = 0.3f;
            }
            // Domed / low structures
            else if (T.Contains(TEXT("dome")) || T.Contains(TEXT("well"))
                || T.Contains(TEXT("altar")) || T.Contains(TEXT("dolmen"))
                || T.Contains(TEXT("barrow")))
            {
                ShapeType = Shape_Sphere;
                Mesh = SphereMesh;
                HeightMult = 0.6f;
                WidthMult = 0.8f;
            }
            // Pyramids
            else if (T.Contains(TEXT("pyramid")) || T.Contains(TEXT("ziggurat")))
            {
                ShapeType = Shape_Cone;
                Mesh = ConeMesh ? ConeMesh : CubeMesh;
                HeightMult = 2.0f;
                WidthMult = 0.8f;
            }
            else
            {
                ShapeType = Shape_Cube;
                Mesh = CubeMesh;
                HeightMult = 1.5f;
                WidthMult = 0.8f;
            }
        }

        if (!Mesh) continue;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // Offset upward so structures sit ON terrain, not inside it
        float StructHeight = S.Size * 30.f;  // approximate height in UE units
        Location.Z += StructHeight * 0.5f;

        AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(
            Location + PivotOff, FRotator::ZeroRotator, Params);

        if (Actor)
        {
            Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);

            // Scale: BasicShapes are 100uu. S.Size is 8-20. Target visible structures.
            float BaseScale = S.Size * 0.3f * ScaleMult;
            FVector Scale3D(BaseScale * WidthMult, BaseScale * WidthMult, BaseScale * HeightMult);
            Actor->SetActorScale3D(Scale3D);
            Actor->SetActorLabel(FString::Printf(TEXT("BX_%s"), *S.Type));

            // Apply colour tint via proper structure material
            UMaterialInterface* BaseMat = FallbackMaterial;
            if (!BaseMat)
                BaseMat = BaseXAssets::LoadStructureMaterial();
            if (!BaseMat)
                BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);

            UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(BaseMat, Actor);
            if (MI)
            {
                MI->SetVectorParameterValue(TEXT("BaseColor"), S.Color);
                Actor->GetStaticMeshComponent()->SetMaterial(0, MI);
            }

            SpawnedActors.Add(Actor);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXStructureSpawner: Spawned %d structures"), SpawnedActors.Num());
}
