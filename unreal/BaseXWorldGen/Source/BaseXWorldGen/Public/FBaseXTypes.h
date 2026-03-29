// FBaseXTypes.h — UE-friendly struct wrappers around Base-X pure C++ types
// These mirror the types in WorldPromptEngine.h / DRO.h but with USTRUCT
// so they're visible in Blueprints, details panels, and data tables.

#pragma once

#include "CoreMinimal.h"
#include "FBaseXTypes.generated.h"

// ══════════════════════════════════════════════════════════════════════
// STRUCTURE PLACEMENT (mirrors BaseX::StructureInstance)
// ══════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct BASEXWORLDGEN_API FBaseXStructurePlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    FString Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    int32 GridX = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    int32 GridY = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float Height = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float Size = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    FLinearColor Color = FLinearColor::White;
};

// ══════════════════════════════════════════════════════════════════════
// WEATHER STATE (mirrors BaseX::WeatherState)
// ══════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct BASEXWORLDGEN_API FBaseXWeatherState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bHasRain = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bHasSnow = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bHasStorm = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bHasMist = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bHasAurora = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bHasAsh = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bIsScorching = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bIsMoonlit = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    bool bIsDawn = false;

    // v1.2: intensity values for smoother weather control
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float RainIntensity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float SnowIntensity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float WindIntensity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float CloudCover = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float Visibility = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float Temperature = 0.5f;
};

// ══════════════════════════════════════════════════════════════════════
// WORLD STATS (mirrors BaseX::WorldStats)
// ══════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct BASEXWORLDGEN_API FBaseXWorldStats
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    int32 TileCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    int32 StructureCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    int32 BiomeCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    float MaxElevationMetres = 0.f;
};

// ══════════════════════════════════════════════════════════════════════
// PARSE RESULT (mirrors BaseX::ParseResult)
// ══════════════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct BASEXWORLDGEN_API FBaseXParseResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    TArray<FString> TerrainKeywords;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    TArray<FString> BiomeKeywords;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    TArray<FString> StructureKeywords;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    TArray<FString> WeatherKeywords;

    // v1.2: density level parsed from prompt
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    int32 DensityLevel = 2;  // 0=Sparse..5=Lush, default 2=Normal

    // v1.2: number of structures with exact count overrides
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX")
    int32 ExactCountOverrides = 0;
};

// ══════════════════════════════════════════════════════════════════════
// DRO RADIX INFO (mirrors BaseX::RadixBase + metrics)
// ══════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class EBaseXRadixBase : uint8
{
    None   = 0   UMETA(DisplayName = "None"),
    Base4  = 4   UMETA(DisplayName = "Base-4 (Distant)"),
    Base8  = 8   UMETA(DisplayName = "Base-8 (Far)"),
    Base16 = 16  UMETA(DisplayName = "Base-16 (Room)"),
    Base32 = 32  UMETA(DisplayName = "Base-32 (Near)"),
    Base64 = 64  UMETA(DisplayName = "Base-64 (Handheld)")
};

USTRUCT(BlueprintType)
struct BASEXWORLDGEN_API FBaseXDROMetrics
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|DRO")
    EBaseXRadixBase CurrentRadix = EBaseXRadixBase::Base16;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|DRO")
    int64 RadixSwitches = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|DRO")
    float AverageError = 0.f;
};

// ══════════════════════════════════════════════════════════════════════
// GENERATION PHASE (mirrors BaseX::GenPhase)
// ══════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class EBaseXGenPhase : uint8
{
    Idle       = 0,
    Heightmap  = 1,
    Biomes     = 2,
    Structures = 3,
    Weather    = 4,
    Complete   = 5
};
