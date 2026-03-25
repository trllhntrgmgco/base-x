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
    for (auto& Chunk : TerrainChunks)
    {
        if (IsValid(Chunk))
            Chunk->DestroyComponent();
    }
    TerrainChunks.Empty();
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

    // ── Resolve terrain material once ──
    UMaterialInterface* TerrainMat = TerrainMaterial;
    if (!TerrainMat)
    {
        TerrainMat = BaseXAssets::LoadTerrainMaterial();
        if (!TerrainMat)
            TerrainMat = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
        UE_LOG(LogTemp, Log, TEXT("BaseXWorldMesh: Terrain material = %s"),
            TerrainMat ? *TerrainMat->GetName() : TEXT("none"));
    }

    // ── Build terrain in chunks for per-chunk frustum culling ──
    // Each chunk is a separate ProceduralMeshComponent so the renderer
    // can skip off-screen chunks entirely — critical for large grids.
    constexpr int32 ChunkCells = 64;
    const int32 NumChunksPerSide = FMath::Max(1, (GS - 1 + ChunkCells - 1) / ChunkCells);
    int32 TotalTerrainVerts = 0;

    for (int32 CZ = 0; CZ < NumChunksPerSide; CZ++)
    {
        for (int32 CX = 0; CX < NumChunksPerSide; CX++)
        {
            const int32 StartX = CX * ChunkCells;
            const int32 StartZ = CZ * ChunkCells;
            const int32 EndX = FMath::Min(StartX + ChunkCells, GS - 1);
            const int32 EndZ = FMath::Min(StartZ + ChunkCells, GS - 1);
            const int32 ChunkW = EndX - StartX + 1;
            const int32 ChunkH = EndZ - StartZ + 1;
            if (ChunkW < 2 || ChunkH < 2) continue;

            const int32 NumVerts = ChunkW * ChunkH;
            TArray<FVector> Verts;    Verts.SetNum(NumVerts);
            TArray<FVector> Normals;  Normals.SetNum(NumVerts);
            TArray<FVector2D> UV0;    UV0.SetNum(NumVerts);
            TArray<FLinearColor> Colors; Colors.SetNum(NumVerts);
            TArray<int32> Tris;

            for (int32 LZ = 0; LZ < ChunkH; LZ++)
            {
                for (int32 LX = 0; LX < ChunkW; LX++)
                {
                    const int32 GX = StartX + LX;
                    const int32 GZ = StartZ + LZ;
                    const int32 GIdx = GZ * GS + GX;
                    const int32 LIdx = LZ * ChunkW + LX;

                    float H = HeightRaw[GIdx] * HScale;
                    Verts[LIdx] = FVector(GX * CellSize - HalfWorld, GZ * CellSize - HalfWorld, H);
                    UV0[LIdx] = FVector2D((float)GX / GS, (float)GZ / GS);
                    Colors[LIdx] = ColorRaw[GIdx];

                    // Per-vertex normal from finite differences
                    int32 XL = FMath::Max(0, GX - 1), XR = FMath::Min(GS - 1, GX + 1);
                    int32 ZD = FMath::Max(0, GZ - 1), ZU = FMath::Min(GS - 1, GZ + 1);
                    float Nx = (HeightRaw[GZ * GS + XL] - HeightRaw[GZ * GS + XR]) * HScale;
                    float Nz = (HeightRaw[ZD * GS + GX] - HeightRaw[ZU * GS + GX]) * HScale;
                    FVector N(Nx, Nz, 2.f * CellSize);
                    N.Normalize();
                    Normals[LIdx] = N;
                }
            }

            Tris.Reserve((ChunkW - 1) * (ChunkH - 1) * 6);
            for (int32 LZ = 0; LZ < ChunkH - 1; LZ++)
            {
                for (int32 LX = 0; LX < ChunkW - 1; LX++)
                {
                    int32 V00 = LZ * ChunkW + LX;
                    int32 V10 = V00 + 1;
                    int32 V01 = (LZ + 1) * ChunkW + LX;
                    int32 V11 = V01 + 1;
                    Tris.Add(V00); Tris.Add(V01); Tris.Add(V10);
                    Tris.Add(V10); Tris.Add(V01); Tris.Add(V11);
                }
            }

            UProceduralMeshComponent* ChunkPMC = NewObject<UProceduralMeshComponent>(this);
            ChunkPMC->SetupAttachment(GetRootComponent());
            ChunkPMC->RegisterComponent();
            ChunkPMC->CreateMeshSection_LinearColor(
                0, Verts, Tris, Normals, UV0, Colors, TArray<FProcMeshTangent>(), bEnableCollision);
            if (TerrainMat)
                ChunkPMC->SetMaterial(0, TerrainMat);
            TerrainChunks.Add(ChunkPMC);
            TotalTerrainVerts += NumVerts;
        }
    }

    // ── Water plane — tessellated grid for smooth material-driven waves ──
    constexpr int32 WaterTess = 48;  // subdivisions per axis
    float WaterPadding = CellSize * 2.f;
    float WaterExtent = HalfWorld + WaterPadding;
    float WaterStep = (WaterExtent * 2.f) / WaterTess;

    int32 WaterVertCount = (WaterTess + 1) * (WaterTess + 1);
    TArray<FVector> WVerts;
    TArray<FVector> WNormals;
    TArray<FVector2D> WUV;
    TArray<FLinearColor> WColors;
    TArray<int32> WTris;

    WVerts.SetNum(WaterVertCount);
    WNormals.SetNum(WaterVertCount);
    WUV.SetNum(WaterVertCount);
    WColors.SetNum(WaterVertCount);

    for (int32 Zy = 0; Zy <= WaterTess; Zy++)
    {
        for (int32 Xx = 0; Xx <= WaterTess; Xx++)
        {
            int32 Idx = Zy * (WaterTess + 1) + Xx;
            float Wx = -WaterExtent + Xx * WaterStep;
            float Wy = -WaterExtent + Zy * WaterStep;
            WVerts[Idx] = FVector(Wx, Wy, WaterH);
            WNormals[Idx] = FVector::UpVector;
            WUV[Idx] = FVector2D((float)Xx / WaterTess * 4.f, (float)Zy / WaterTess * 4.f);
            WColors[Idx] = FLinearColor(0.1f, 0.3f, 0.6f, 0.6f);
        }
    }

    WTris.Reserve(WaterTess * WaterTess * 6);
    for (int32 Zy = 0; Zy < WaterTess; Zy++)
    {
        for (int32 Xx = 0; Xx < WaterTess; Xx++)
        {
            int32 V00 = Zy * (WaterTess + 1) + Xx;
            int32 V10 = V00 + 1;
            int32 V01 = (Zy + 1) * (WaterTess + 1) + Xx;
            int32 V11 = V01 + 1;
            WTris.Add(V00); WTris.Add(V01); WTris.Add(V10);
            WTris.Add(V10); WTris.Add(V01); WTris.Add(V11);
        }
    }

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

    UE_LOG(LogTemp, Log, TEXT("BaseXWorldMesh: %d terrain chunks (%d verts), water %dx%d, %d structures pending"),
           TerrainChunks.Num(), TotalTerrainVerts, WaterTess, WaterTess, Builder->GetStructurePlacements().Num());
}
