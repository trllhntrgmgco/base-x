// ABaseXWorldMesh.cpp — Builds ProceduralMeshComponent from Base-X heightmap

#include "ABaseXWorldMesh.h"
#include "UBaseXWorldBuilder.h"
#include "BaseXAssetFactory.h"
#include "ProceduralMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

ABaseXWorldMesh::ABaseXWorldMesh()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
    TerrainMesh->SetupAttachment(Root);

    WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
    WaterMesh->SetupAttachment(Root);
}

void ABaseXWorldMesh::Clear()
{
    TerrainMesh->ClearAllMeshSections();
    WaterMesh->ClearAllMeshSections();
}

void ABaseXWorldMesh::BuildFromWorldBuilder(UBaseXWorldBuilder* Builder)
{
    if (!Builder || !Builder->IsComplete())
    {
        UE_LOG(LogTemp, Warning, TEXT("BaseXWorldMesh: Builder not ready"));
        return;
    }

    Clear();

    const int32 GS = Builder->GetGridSize();
    const float CellSize = Builder->CellWorldSize;
    const float HScale = Builder->HeightScale;
    const float WaterH = Builder->WaterLevel * HScale;
    const float HalfWorld = (GS - 1) * CellSize * 0.5f;  // center the grid

    const TArray<float>& HeightRaw = Builder->GetHeightmapRaw();
    const TArray<FLinearColor>& ColorRaw = Builder->GetBiomeMapRaw();

    // ── Terrain vertices ──
    TArray<FVector> Verts;
    TArray<FVector> Normals;
    TArray<FColor> Colors;
    TArray<FVector2D> UV0;
    TArray<int32> Tris;

    const int32 NumVerts = GS * GS;
    Verts.SetNum(NumVerts);
    Normals.SetNum(NumVerts);
    Colors.SetNum(NumVerts);
    UV0.SetNum(NumVerts);

    for (int32 Z = 0; Z < GS; Z++)
    {
        for (int32 X = 0; X < GS; X++)
        {
            int32 Idx = Z * GS + X;
            float H = HeightRaw[Idx] * HScale;

            Verts[Idx] = FVector(X * CellSize - HalfWorld, Z * CellSize - HalfWorld, H);
            UV0[Idx] = FVector2D((float)X / GS, (float)Z / GS);

            FLinearColor C = ColorRaw[Idx];
            Colors[Idx] = C.ToFColor(false);

            // Per-vertex normal from finite differences
            int32 XL = FMath::Max(0, X - 1), XR = FMath::Min(GS - 1, X + 1);
            int32 ZD = FMath::Max(0, Z - 1), ZU = FMath::Min(GS - 1, Z + 1);
            float Nx = (HeightRaw[Z * GS + XL] - HeightRaw[Z * GS + XR]) * HScale;
            float Nz = (HeightRaw[ZD * GS + X] - HeightRaw[ZU * GS + X]) * HScale;
            FVector N(Nx, Nz, 2.f * CellSize);
            N.Normalize();
            Normals[Idx] = N;
        }
    }

    // ── Terrain triangles ──
    Tris.Reserve((GS - 1) * (GS - 1) * 6);
    for (int32 Z = 0; Z < GS - 1; Z++)
    {
        for (int32 X = 0; X < GS - 1; X++)
        {
            int32 V00 = Z * GS + X;
            int32 V10 = V00 + 1;
            int32 V01 = (Z + 1) * GS + X;
            int32 V11 = V01 + 1;
            Tris.Add(V00); Tris.Add(V01); Tris.Add(V10);
            Tris.Add(V10); Tris.Add(V01); Tris.Add(V11);
        }
    }

    TArray<FProcMeshTangent> Tangents; // auto-calculated

    // Pass vertex colors as LinearColor array for the mesh section
    TArray<FLinearColor> LinearColors;
    LinearColors.SetNum(NumVerts);
    for (int32 I = 0; I < NumVerts; I++)
    {
        LinearColors[I] = FLinearColor(Colors[I]);
    }

    TerrainMesh->CreateMeshSection_LinearColor(
        0, Verts, Tris, Normals, UV0, LinearColors, Tangents, bEnableCollision);

    // Apply material — proper lit material with vertex color → BaseColor
    if (!TerrainMaterial)
    {
        UMaterialInterface* Mat = BaseXAssets::LoadTerrainMaterial();
        if (!Mat)
            Mat = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
        if (Mat)
            TerrainMesh->SetMaterial(0, Mat);
        UE_LOG(LogTemp, Log, TEXT("BaseXWorldMesh: Terrain material = %s"),
            Mat ? *Mat->GetName() : TEXT("none"));
    }
    else
    {
        TerrainMesh->SetMaterial(0, TerrainMaterial);
    }

    // ── Water plane ──
    float WS = (GS - 1) * CellSize;
    float WaterPadding = CellSize * 2.f;
    TArray<FVector> WVerts = {
        FVector(-HalfWorld - WaterPadding, -HalfWorld - WaterPadding, WaterH),
        FVector(HalfWorld + WaterPadding, -HalfWorld - WaterPadding, WaterH),
        FVector(HalfWorld + WaterPadding, HalfWorld + WaterPadding, WaterH),
        FVector(-HalfWorld - WaterPadding, HalfWorld + WaterPadding, WaterH)
    };
    TArray<int32> WTris = { 0, 1, 2, 0, 2, 3 };
    TArray<FVector> WNormals = {
        FVector::UpVector, FVector::UpVector,
        FVector::UpVector, FVector::UpVector
    };
    TArray<FVector2D> WUV = {
        FVector2D(0, 0), FVector2D(4, 0),
        FVector2D(4, 4), FVector2D(0, 4)
    };
    TArray<FLinearColor> WColors = {
        FLinearColor(0.1f, 0.3f, 0.6f, 0.6f),
        FLinearColor(0.1f, 0.3f, 0.6f, 0.6f),
        FLinearColor(0.15f, 0.35f, 0.65f, 0.6f),
        FLinearColor(0.15f, 0.35f, 0.65f, 0.6f)
    };

    WaterMesh->CreateMeshSection_LinearColor(
        0, WVerts, WTris, WNormals, WUV, WColors, TArray<FProcMeshTangent>(), false);

    // Water material — translucent blue
    if (!WaterMaterial)
    {
        UMaterialInterface* Mat = BaseXAssets::LoadWaterMaterial();
        if (!Mat)
            Mat = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
        if (Mat)
            WaterMesh->SetMaterial(0, Mat);
    }
    else
    {
        WaterMesh->SetMaterial(0, WaterMaterial);
    }
    WaterMesh->SetCastShadow(false);

    UE_LOG(LogTemp, Log, TEXT("BaseXWorldMesh: Built %d verts, %d tris, %d trees pending"),
           NumVerts, Tris.Num() / 3, Builder->GetStructurePlacements().Num());
}
