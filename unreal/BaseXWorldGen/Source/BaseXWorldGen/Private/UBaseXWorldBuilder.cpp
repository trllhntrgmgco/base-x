// UBaseXWorldBuilder.cpp — Implementation bridging BaseX::WorldPromptEngine → UE

#include "UBaseXWorldBuilder.h"

// ── Base-X pure C++ headers (no UE dependency) ──
#include "BaseX/DRO.h"
#include "BaseX/WorldPromptEngine.h"

// ══════════════════════════════════════════════════════════════════════
// PIMPL — hides Base-X types from UHT
// ══════════════════════════════════════════════════════════════════════

struct UBaseXWorldBuilder::FImpl
{
    BaseX::WorldPromptEngine Engine;
    BaseX::DROManager DRO;
    EBaseXGenPhase LastBroadcastPhase = EBaseXGenPhase::Idle;

    FImpl()
    {
        DRO.initializeVRProfile();
    }
};

// ══════════════════════════════════════════════════════════════════════
// CONSTRUCTION
// ══════════════════════════════════════════════════════════════════════

UBaseXWorldBuilder::UBaseXWorldBuilder()
{
    Impl = new FImpl();
}

UBaseXWorldBuilder::~UBaseXWorldBuilder()
{
    delete Impl;
    Impl = nullptr;
}

// ══════════════════════════════════════════════════════════════════════
// GENERATION
// ══════════════════════════════════════════════════════════════════════

void UBaseXWorldBuilder::GenerateFromPrompt(const FString& Prompt)
{
    std::string StdPrompt(TCHAR_TO_UTF8(*Prompt));
    Impl->Engine.beginGeneration(StdPrompt);
    Impl->LastBroadcastPhase = EBaseXGenPhase::Heightmap;
    CachedHeightmap.Empty();
    CachedBiomeColors.Empty();
}

float UBaseXWorldBuilder::TickGeneration(float DeltaTime)
{
    if (!IsGenerating()) return 100.f;

    bool StillGoing = Impl->Engine.tick(DeltaTime);

    // Phase change events
    EBaseXGenPhase Current = GetCurrentPhase();
    if (Current != Impl->LastBroadcastPhase)
    {
        Impl->LastBroadcastPhase = Current;
        OnPhaseChanged.Broadcast(Current, Impl->Engine.phasePct());
    }

    if (!StillGoing && Impl->Engine.phase() == BaseX::GenPhase::COMPLETE)
    {
        CacheResults();
        OnGenerationComplete.Broadcast();
    }

    return Impl->Engine.phasePct();
}

bool UBaseXWorldBuilder::IsGenerating() const
{
    auto P = Impl->Engine.phase();
    return P != BaseX::GenPhase::IDLE && P != BaseX::GenPhase::COMPLETE;
}

bool UBaseXWorldBuilder::IsComplete() const
{
    return Impl->Engine.phase() == BaseX::GenPhase::COMPLETE;
}

void UBaseXWorldBuilder::Reset()
{
    Impl->Engine.reset();
    CachedHeightmap.Empty();
    CachedBiomeColors.Empty();
}

// ══════════════════════════════════════════════════════════════════════
// RESULTS
// ══════════════════════════════════════════════════════════════════════

EBaseXGenPhase UBaseXWorldBuilder::GetCurrentPhase() const
{
    return static_cast<EBaseXGenPhase>(static_cast<uint8>(Impl->Engine.phase()));
}

float UBaseXWorldBuilder::GetPhasePercent() const
{
    return Impl->Engine.phasePct();
}

FBaseXWorldStats UBaseXWorldBuilder::GetWorldStats() const
{
    const auto& S = Impl->Engine.stats();
    FBaseXWorldStats Out;
    Out.TileCount = S.tileCount;
    Out.StructureCount = S.structCount;
    Out.BiomeCount = S.biomeCount;
    Out.MaxElevationMetres = S.maxElevation;
    return Out;
}

FBaseXWeatherState UBaseXWorldBuilder::GetWeatherState() const
{
    const auto& W = Impl->Engine.weather();
    FBaseXWeatherState Out;
    Out.bHasRain = W.hasRain;
    Out.bHasSnow = W.hasSnow;
    Out.bHasStorm = W.hasStorm;
    Out.bHasMist = W.hasMist;
    Out.bHasAurora = W.hasAurora;
    Out.bIsScorching = W.isScorching;
    Out.bIsMoonlit = W.isMoonlit;
    Out.bIsDawn = W.isDawn;
    return Out;
}

FBaseXParseResult UBaseXWorldBuilder::GetParseResult() const
{
    const auto& P = Impl->Engine.parsed();
    FBaseXParseResult Out;
    for (auto& S : P.terrain)   Out.TerrainKeywords.Add(UTF8_TO_TCHAR(S.c_str()));
    for (auto& S : P.biome)     Out.BiomeKeywords.Add(UTF8_TO_TCHAR(S.c_str()));
    for (auto& S : P.structure) Out.StructureKeywords.Add(UTF8_TO_TCHAR(S.c_str()));
    for (auto& S : P.weather)   Out.WeatherKeywords.Add(UTF8_TO_TCHAR(S.c_str()));
    return Out;
}

TArray<FBaseXStructurePlacement> UBaseXWorldBuilder::GetStructurePlacements() const
{
    TArray<FBaseXStructurePlacement> Out;
    for (const auto& S : Impl->Engine.structures())
    {
        FBaseXStructurePlacement P;
        P.Type = UTF8_TO_TCHAR(S.type.c_str());
        P.GridX = S.gx;
        P.GridY = S.gy;
        P.Height = S.height;
        P.Size = S.size;
        P.Color = FLinearColor(S.r, S.g, S.b, 1.f);
        Out.Add(P);
    }
    return Out;
}

// ══════════════════════════════════════════════════════════════════════
// HEIGHTMAP ACCESS
// ══════════════════════════════════════════════════════════════════════

int32 UBaseXWorldBuilder::GetGridSize() const
{
    return BaseX::WPE_GRID_SIZE;
}

float UBaseXWorldBuilder::GetHeightAt(int32 X, int32 Y) const
{
    const int32 GS = BaseX::WPE_GRID_SIZE;
    int32 Idx = Y * GS + X;
    if (CachedHeightmap.IsValidIndex(Idx)) return CachedHeightmap[Idx];
    return 0.f;
}

FLinearColor UBaseXWorldBuilder::GetBiomeColorAt(int32 X, int32 Y) const
{
    const int32 GS = BaseX::WPE_GRID_SIZE;
    int32 Idx = Y * GS + X;
    if (CachedBiomeColors.IsValidIndex(Idx)) return CachedBiomeColors[Idx];
    return FLinearColor::Black;
}

// ══════════════════════════════════════════════════════════════════════
// INVENTORY
// ══════════════════════════════════════════════════════════════════════

TArray<FString> UBaseXWorldBuilder::GetTerrainKeywords() const
{
    TArray<FString> Out;
    for (auto& KV : Impl->Engine.parser().terrainDict())
        Out.Add(UTF8_TO_TCHAR(KV.first.c_str()));
    Out.Sort();
    return Out;
}

TArray<FString> UBaseXWorldBuilder::GetBiomeKeywords() const
{
    TArray<FString> Out;
    for (auto& KV : Impl->Engine.parser().biomeDict())
        Out.Add(UTF8_TO_TCHAR(KV.first.c_str()));
    Out.Sort();
    return Out;
}

TArray<FString> UBaseXWorldBuilder::GetStructureKeywords() const
{
    TArray<FString> Out;
    for (auto& S : Impl->Engine.parser().structureKeywords())
        Out.Add(UTF8_TO_TCHAR(S.c_str()));
    return Out;
}

TArray<FString> UBaseXWorldBuilder::GetWeatherKeywords() const
{
    TArray<FString> Out;
    for (auto& S : Impl->Engine.parser().weatherKeywords())
        Out.Add(UTF8_TO_TCHAR(S.c_str()));
    return Out;
}

FString UBaseXWorldBuilder::GetPresetPrompt(int32 Index) const
{
    return UTF8_TO_TCHAR(BaseX::WorldPromptEngine::preset(Index));
}

int32 UBaseXWorldBuilder::GetPresetCount() const
{
    return BaseX::WorldPromptEngine::PRESET_COUNT;
}

// ══════════════════════════════════════════════════════════════════════
// DRO
// ══════════════════════════════════════════════════════════════════════

FBaseXDROMetrics UBaseXWorldBuilder::GetDROMetrics() const
{
    auto M = Impl->DRO.getMetrics();
    FBaseXDROMetrics Out;
    Out.CurrentRadix = static_cast<EBaseXRadixBase>(static_cast<uint8>(M.currentRadix));
    Out.RadixSwitches = M.radixSwitches;
    Out.AverageError = M.averageError;
    return Out;
}

EBaseXRadixBase UBaseXWorldBuilder::GetOptimalRadix(float Distance) const
{
    return static_cast<EBaseXRadixBase>(static_cast<uint8>(Impl->DRO.getOptimalRadix(Distance)));
}

// ══════════════════════════════════════════════════════════════════════
// CACHE — copy Base-X vectors into UE TArrays
// ══════════════════════════════════════════════════════════════════════

void UBaseXWorldBuilder::CacheResults()
{
    const int32 GS = BaseX::WPE_GRID_SIZE;
    const int32 Total = GS * GS;

    // Heightmap
    const auto& HMap = Impl->Engine.heightmap();
    CachedHeightmap.SetNum(Total);
    for (int32 I = 0; I < Total; I++)
        CachedHeightmap[I] = HMap[I];

    // Biome colors
    const auto& BMap = Impl->Engine.biomeMap();
    CachedBiomeColors.SetNum(Total);
    for (int32 I = 0; I < Total; I++)
        CachedBiomeColors[I] = FLinearColor(BMap[I].r, BMap[I].g, BMap[I].b, 1.f);
}
