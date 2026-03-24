// ABaseXStructureSpawner.h — Reads structure placements from the world builder
// and spawns actors (from a DataTable or mesh map) at the correct positions.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "FBaseXTypes.h"
#include "ABaseXStructureSpawner.generated.h"

class UBaseXWorldBuilder;

// Row in the structure mesh lookup table
USTRUCT(BlueprintType)
struct FBaseXStructureMeshRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    TSoftObjectPtr<UStaticMesh> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float ScaleMultiplier = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    FVector PivotOffset = FVector::ZeroVector;
};

UCLASS(BlueprintType, Blueprintable)
class BASEXWORLDGEN_API ABaseXStructureSpawner : public AActor
{
    GENERATED_BODY()

public:
    ABaseXStructureSpawner();

    // Call after generation completes. Spawns structure actors.
    UFUNCTION(BlueprintCallable, Category="BaseX|Structures")
    void SpawnStructures(UBaseXWorldBuilder* Builder);

    UFUNCTION(BlueprintCallable, Category="BaseX|Structures")
    void ClearStructures();

    // Optional: DataTable mapping structure type name → mesh.
    // Row names should match WPE keywords: "tower", "castle", etc.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Structures")
    TObjectPtr<UDataTable> StructureMeshTable;

    // Fallback mesh when no DataTable entry is found
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Structures")
    TObjectPtr<UStaticMesh> FallbackMesh;

    // Fallback material tinted by structure colour
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Structures")
    TObjectPtr<UMaterialInterface> FallbackMaterial;

private:
    UPROPERTY()
    TArray<TObjectPtr<AActor>> SpawnedActors;
};
