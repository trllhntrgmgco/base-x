// UBaseXWorldBuilder.h — Core UObject wrapper around BaseX::WorldPromptEngine
// This is the single entry point: feed it a prompt string, tick it, read results.

#pragma once

#include "CoreMinimal.h"
#include "FBaseXTypes.h"
#include "UBaseXWorldBuilder.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldGenerationComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseChanged, EBaseXGenPhase, NewPhase, float, Percent);

UCLASS(BlueprintType, Blueprintable)
class BASEXWORLDGEN_API UBaseXWorldBuilder : public UObject
{
    GENERATED_BODY()

public:
    UBaseXWorldBuilder();
    ~UBaseXWorldBuilder();

    // ── Generation ──

    UFUNCTION(BlueprintCallable, Category="BaseX|World")
    void GenerateFromPrompt(const FString& Prompt);

    UFUNCTION(BlueprintCallable, Category="BaseX|World")
    float TickGeneration(float DeltaTime);

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    bool IsGenerating() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    bool IsComplete() const;

    UFUNCTION(BlueprintCallable, Category="BaseX|World")
    void Reset();

    // ── Results ──

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    EBaseXGenPhase GetCurrentPhase() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    float GetPhasePercent() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    FBaseXWorldStats GetWorldStats() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    FBaseXWeatherState GetWeatherState() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    FBaseXParseResult GetParseResult() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    TArray<FBaseXStructurePlacement> GetStructurePlacements() const;

    // ── Heightmap Access ──

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    int32 GetGridSize() const;

    UFUNCTION(BlueprintCallable, Category="BaseX|World")
    void SetGridSize(int32 NewSize);

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    float GetHeightAt(int32 X, int32 Y) const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    FLinearColor GetBiomeColorAt(int32 X, int32 Y) const;

    // Raw array access for ProceduralMesh building
    const TArray<float>& GetHeightmapRaw() const { return CachedHeightmap; }
    const TArray<FLinearColor>& GetBiomeMapRaw() const { return CachedBiomeColors; }

    // ── Inventory (for prompt screen UI) ──

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    TArray<FString> GetTerrainKeywords() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    TArray<FString> GetBiomeKeywords() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    TArray<FString> GetStructureKeywords() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    TArray<FString> GetWeatherKeywords() const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    FString GetPresetPrompt(int32 Index) const;

    UFUNCTION(BlueprintPure, Category="BaseX|World")
    int32 GetPresetCount() const;

    // ── DRO ──

    UFUNCTION(BlueprintPure, Category="BaseX|DRO")
    FBaseXDROMetrics GetDROMetrics() const;

    UFUNCTION(BlueprintPure, Category="BaseX|DRO")
    EBaseXRadixBase GetOptimalRadix(float Distance) const;

    // ── Events ──

    UPROPERTY(BlueprintAssignable, Category="BaseX|World")
    FOnWorldGenerationComplete OnGenerationComplete;

    UPROPERTY(BlueprintAssignable, Category="BaseX|World")
    FOnPhaseChanged OnPhaseChanged;

    // ── Settings ──

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|World")
    float CellWorldSize = 400.f;  // UE units per grid cell (4m default)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|World")
    float HeightScale = 4000.f;   // Max height in UE units (40m)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|World")
    float WaterLevel = 0.18f;     // Normalised 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|World", meta=(ClampMin="16", ClampMax="256"))
    int32 RequestedGridSize = 64; // User-configurable terrain size in tiles per side

private:
    // Opaque pointer to Base-X pure C++ objects.
    // Raw pointer + explicit delete avoids TUniquePtr incomplete-type
    // issue with UHT-generated constructors.
    struct FImpl;
    FImpl* Impl = nullptr;

    // Cached UE-side copies (filled when generation completes)
    TArray<float> CachedHeightmap;
    TArray<FLinearColor> CachedBiomeColors;

    void CacheResults();
};
