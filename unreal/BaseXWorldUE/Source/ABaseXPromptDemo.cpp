// ABaseXPromptDemo.cpp — Full prompt UI + world generation demo

#include "ABaseXPromptDemo.h"
#include "SBaseXHub.h"
#include "UBaseXWorldBuilder.h"
#include "ABaseXWorldMesh.h"
#include "ABaseXStructureSpawner.h"
#include "UBaseXWeatherController.h"
#include "FBaseXTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/LightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Widgets/SWeakWidget.h"

// ══════════════════════════════════════════════════════════════════════
// ACTOR IMPLEMENTATION
// ══════════════════════════════════════════════════════════════════════

ABaseXPromptDemo::ABaseXPromptDemo()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABaseXPromptDemo::BeginPlay()
{
    Super::BeginPlay();

    Builder = NewObject<UBaseXWorldBuilder>(this);
    Builder->CellWorldSize = CellWorldSize;
    Builder->HeightScale = HeightScale;

    // Create and add the unified Hub UI to viewport
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && GEngine && GEngine->GameViewport)
    {
        Hub = SNew(SBaseXHub)
            .Owner(this)
            .Builder(Builder)
            .MeshTable(nullptr);

        ViewportWrapper =
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Top)
            .Padding(0, 20, 0, 0)
            [
                Hub.ToSharedRef()
            ];

        GEngine->GameViewport->AddViewportWidgetContent(
            SNew(SWeakWidget).PossiblyNullContent(ViewportWrapper.ToSharedRef()), 100);

        UE_LOG(LogTemp, Warning, TEXT("BaseXPromptDemo: Hub UI ADDED to viewport"));
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Cyan,
                TEXT("BASE-X HUB ACTIVE — Use tabs: World Prompt | Asset Mapper | Settings"));

        // Show mouse cursor for interaction
        PC->SetShowMouseCursor(true);
        PC->SetInputMode(FInputModeGameAndUI());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("BaseXPromptDemo: FAILED to create UI - no GameViewport"));
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red,
                TEXT("BASE-X ERROR: Could not attach UI to viewport"));
    }

    int32 nT = Builder->GetTerrainKeywords().Num();
    int32 nB = Builder->GetBiomeKeywords().Num();
    int32 nS = Builder->GetStructureKeywords().Num();
    int32 nW = Builder->GetWeatherKeywords().Num();

    UE_LOG(LogTemp, Log, TEXT("BaseXPromptDemo: UI ready. %d presets, %d terrain, %d biome, %d struct, %d weather keywords"),
        Builder->GetPresetCount(), nT, nB, nS, nW);

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow,
            FString::Printf(TEXT("Tag Inventory: %d terrain, %d biome, %d structures, %d weather — scroll panel to browse"),
                nT, nB, nS, nW));
}

void ABaseXPromptDemo::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bGenerating || !Builder) return;

    Builder->TickGeneration(DeltaTime);

    // Update progress UI
    float Pct = Builder->GetPhasePercent();
    FString PhaseName;
    switch (Builder->GetCurrentPhase())
    {
        case EBaseXGenPhase::Heightmap:  PhaseName = TEXT("Generating heightmap..."); break;
        case EBaseXGenPhase::Biomes:     PhaseName = TEXT("Painting biomes..."); break;
        case EBaseXGenPhase::Structures: PhaseName = TEXT("Placing structures..."); break;
        case EBaseXGenPhase::Weather:    PhaseName = TEXT("Resolving weather..."); break;
        case EBaseXGenPhase::Complete:   PhaseName = TEXT("Complete!"); break;
        default:                         PhaseName = TEXT("Idle"); break;
    }

    if (Hub.IsValid())
        Hub->SetProgress(Pct, FString::Printf(TEXT("%s (%.0f%%)"), *PhaseName, Pct));

    // Generation complete
    if (Builder->IsComplete())
    {
        bGenerating = false;
        if (Hub.IsValid()) Hub->SetGenerating(false);

        // Show parse results
        auto Parse = Builder->GetParseResult();
        FString ParseInfo;
        auto Join = [](const TArray<FString>& Arr) {
            FString Out;
            for (auto& S : Arr) { if (!Out.IsEmpty()) Out += TEXT(", "); Out += S; }
            return Out;
        };
        ParseInfo += FString::Printf(TEXT("Terrain: %s\n"), *Join(Parse.TerrainKeywords));
        ParseInfo += FString::Printf(TEXT("Biomes: %s\n"), *Join(Parse.BiomeKeywords));
        ParseInfo += FString::Printf(TEXT("Structures: %s\n"), *Join(Parse.StructureKeywords));
        ParseInfo += FString::Printf(TEXT("Weather: %s"), *Join(Parse.WeatherKeywords));
        if (Hub.IsValid()) Hub->SetParseInfo(ParseInfo);

        // Show stats
        auto Stats = Builder->GetWorldStats();
        FString StatsStr = FString::Printf(
            TEXT("Tiles: %d | Structures: %d | Biomes: %d | Max Elev: %.0fm"),
            Stats.TileCount, Stats.StructureCount, Stats.BiomeCount, Stats.MaxElevationMetres);
        if (Hub.IsValid()) Hub->SetStats(StatsStr);

        // Spawn terrain mesh
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        SpawnedMesh = GetWorld()->SpawnActor<ABaseXWorldMesh>(
            GetActorLocation(), FRotator::ZeroRotator, Params);
        if (SpawnedMesh)
            SpawnedMesh->BuildFromWorldBuilder(Builder);

        // Spawn structures
        SpawnedStructures = GetWorld()->SpawnActor<ABaseXStructureSpawner>(
            GetActorLocation(), FRotator::ZeroRotator, Params);
        if (SpawnedStructures)
            SpawnedStructures->SpawnStructures(Builder);

        // Apply weather — ensure we have sun + fog in the scene
        EnsureSceneLighting();
        if (!WeatherCtrl)
        {
            WeatherCtrl = NewObject<UBaseXWeatherController>(this);
            WeatherCtrl->RegisterComponent();
        }
        WeatherCtrl->SunLight = SpawnedSun;
        WeatherCtrl->Fog = SpawnedFog;
        WeatherCtrl->ApplyWeatherState(Builder->GetWeatherState());

        // Log weather
        auto WS = Builder->GetWeatherState();
        FString WeatherInfo;
        if (WS.bHasStorm)     WeatherInfo += TEXT("STORM ");
        if (WS.bHasRain)      WeatherInfo += TEXT("RAIN ");
        if (WS.bHasSnow)      WeatherInfo += TEXT("SNOW ");
        if (WS.bHasMist)      WeatherInfo += TEXT("MIST ");
        if (WS.bHasAurora)    WeatherInfo += TEXT("AURORA ");
        if (WS.bHasAsh)       WeatherInfo += TEXT("ASH ");
        if (WS.bIsScorching)  WeatherInfo += TEXT("SCORCHING ");
        if (WS.bIsMoonlit)    WeatherInfo += TEXT("MOONLIT ");
        if (WS.bIsDawn)       WeatherInfo += TEXT("DAWN ");
        if (WeatherInfo.IsEmpty()) WeatherInfo = TEXT("CLEAR");
        UE_LOG(LogTemp, Log, TEXT("BaseXPromptDemo: Weather = %s"), *WeatherInfo);

        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green,
                FString::Printf(TEXT("World generated! %d tiles, %d structures | Weather: %s"),
                    Stats.TileCount, Stats.StructureCount, *WeatherInfo));
    }
}

void ABaseXPromptDemo::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->RemoveAllViewportWidgets();
    }
    ViewportWrapper.Reset();
    Hub.Reset();

    Super::EndPlay(Reason);
}

void ABaseXPromptDemo::RequestGenerate(const FString& Prompt)
{
    if (!Builder || Prompt.IsEmpty()) return;

    // Clear previous world
    RequestClear();

    Builder->CellWorldSize = CellWorldSize;
    Builder->HeightScale = HeightScale;

    // Apply terrain size from Asset Mapper if available
    TSharedPtr<SBaseXAssetMapper> AM = Hub.IsValid() ? Hub->GetAssetMapper() : nullptr;
    if (AM.IsValid())
    {
        int32 GridSize = AM->GetSelectedGridSize();
        float WorldScale = AM->GetSelectedWorldScale();
        Builder->SetGridSize(GridSize);
        Builder->CellWorldSize = CellWorldSize * WorldScale;
        UE_LOG(LogTemp, Log, TEXT("BaseXPromptDemo: Using terrain size %dx%d (scale %.1fx)"),
            GridSize, GridSize, WorldScale);
    }

    Builder->GenerateFromPrompt(Prompt);
    bGenerating = true;

    if (Hub.IsValid())
    {
        Hub->SetGenerating(true);
        Hub->SetProgress(0.f, TEXT("Starting generation..."));
        Hub->SetStats(FString());
        Hub->SetParseInfo(FString());
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXPromptDemo: Generating from: %s"), *Prompt);
}

void ABaseXPromptDemo::RequestClear()
{
    if (SpawnedMesh && IsValid(SpawnedMesh))
    {
        SpawnedMesh->Clear();
        SpawnedMesh->Destroy();
        SpawnedMesh = nullptr;
    }
    if (SpawnedStructures && IsValid(SpawnedStructures))
    {
        SpawnedStructures->ClearStructures();
        SpawnedStructures->Destroy();
        SpawnedStructures = nullptr;
    }
    if (Builder) Builder->Reset();
    bGenerating = false;
}

FString ABaseXPromptDemo::GetPresetPrompt(int32 Index) const
{
    return Builder ? Builder->GetPresetPrompt(Index) : FString();
}

int32 ABaseXPromptDemo::GetPresetCount() const
{
    return Builder ? Builder->GetPresetCount() : 0;
}

void ABaseXPromptDemo::EnsureSceneLighting()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // Find or spawn DirectionalLight
    if (!SpawnedSun || !IsValid(SpawnedSun))
    {
        for (TActorIterator<ADirectionalLight> It(World); It; ++It)
        {
            SpawnedSun = *It;
            break;
        }
        if (!SpawnedSun)
        {
            FActorSpawnParameters P;
            P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnedSun = World->SpawnActor<ADirectionalLight>(
                GetActorLocation() + FVector(0, 0, 5000), FRotator(-45, 180, 0), P);
            if (SpawnedSun)
            {
                SpawnedSun->GetLightComponent()->SetIntensity(10.f);
#if WITH_EDITOR
                SpawnedSun->SetActorLabel(TEXT("BX_Sun"));
#endif
                UE_LOG(LogTemp, Log, TEXT("BaseXPromptDemo: Spawned DirectionalLight"));
            }
        }
    }

    // Find or spawn ExponentialHeightFog
    if (!SpawnedFog || !IsValid(SpawnedFog))
    {
        for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
        {
            SpawnedFog = *It;
            break;
        }
        if (!SpawnedFog)
        {
            FActorSpawnParameters P;
            P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnedFog = World->SpawnActor<AExponentialHeightFog>(
                GetActorLocation(), FRotator::ZeroRotator, P);
            if (SpawnedFog)
            {
                SpawnedFog->GetComponent()->SetFogDensity(0.005f);
#if WITH_EDITOR
                SpawnedFog->SetActorLabel(TEXT("BX_Fog"));
#endif
                UE_LOG(LogTemp, Log, TEXT("BaseXPromptDemo: Spawned ExponentialHeightFog"));
            }
        }
    }
}
