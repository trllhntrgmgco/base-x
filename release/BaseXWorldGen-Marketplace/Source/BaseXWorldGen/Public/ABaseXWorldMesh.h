// ABaseXWorldMesh.h — Spawnable actor that builds a ProceduralMeshComponent
// from UBaseXWorldBuilder's heightmap + biome color output.
// Drop one in your level and call BuildFromWorldBuilder() after generation.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ABaseXWorldMesh.generated.h"

class UBaseXWorldBuilder;

UCLASS(BlueprintType, Blueprintable)
class BASEXWORLDGEN_API ABaseXWorldMesh : public AActor
{
    GENERATED_BODY()

public:
    ABaseXWorldMesh();

    UFUNCTION(BlueprintCallable, Category="BaseX|Mesh")
    void BuildFromWorldBuilder(UBaseXWorldBuilder* Builder);

    UFUNCTION(BlueprintCallable, Category="BaseX|Mesh")
    void Clear();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Mesh")
    TObjectPtr<UProceduralMeshComponent> TerrainMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Mesh")
    TObjectPtr<UProceduralMeshComponent> WaterMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Mesh")
    TObjectPtr<UMaterialInterface> TerrainMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Mesh")
    TObjectPtr<UMaterialInterface> WaterMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Mesh")
    bool bEnableCollision = true;

private:
    UPROPERTY()
    TArray<TObjectPtr<UProceduralMeshComponent>> TerrainChunks;
};
