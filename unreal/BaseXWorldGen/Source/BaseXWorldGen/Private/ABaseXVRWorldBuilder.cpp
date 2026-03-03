// ABaseXVRWorldBuilder.cpp — VR tablet orchestration logic
// This is the runtime equivalent of the GLFW WPE_001 prompt screen +
// generation + explore flow, but as a single actor with UMG UI.

#include "ABaseXVRWorldBuilder.h"
#include "UBaseXWorldBuilder.h"
#include "ABaseXWorldMesh.h"
#include "ABaseXStructureSpawner.h"
#include "UBaseXWeatherController.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"

ABaseXVRWorldBuilder::ABaseXVRWorldBuilder()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Tablet display panel (UMG widget on a 3D surface)
    TabletWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("TabletWidget"));
    TabletWidget->SetupAttachment(Root);
    TabletWidget->SetDrawSize(FVector2D(800, 600));
    TabletWidget->SetRelativeLocation(FVector(0, 0, 100));
    TabletWidget->SetRelativeRotation(FRotator(-30, 0, 0)); // angled for comfort

    // Weather controller (attached as component)
    WeatherController = CreateDefaultSubobject<UBaseXWeatherController>(TEXT("Weather"));
}

void ABaseXVRWorldBuilder::BeginPlay()
{
    Super::BeginPlay();

    // Create the world builder UObject
    WorldBuilder = NewObject<UBaseXWorldBuilder>(this);

    // Apply the UMG widget class if set
    if (TabletWidgetClass && TabletWidget)
    {
        TabletWidget->SetWidgetClass(TabletWidgetClass);
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXVRWorldBuilder: Ready. %d presets, %d terrain keywords"),
           WorldBuilder->GetPresetCount(), WorldBuilder->GetTerrainKeywords().Num());
}

void ABaseXVRWorldBuilder::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!WorldBuilder) return;

    bool Generating = WorldBuilder->IsGenerating();

    if (Generating)
    {
        WorldBuilder->TickGeneration(DeltaTime);
    }

    // Detect generation completion
    if (bWasGenerating && !Generating && WorldBuilder->IsComplete())
    {
        UE_LOG(LogTemp, Log, TEXT("BaseXVRWorldBuilder: Generation complete, building world..."));

        // Spawn world mesh
        if (WorldMeshClass)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnedMesh = GetWorld()->SpawnActor<ABaseXWorldMesh>(
                WorldMeshClass, GetActorLocation() + WorldSpawnOffset,
                FRotator::ZeroRotator, Params);
            if (SpawnedMesh)
            {
                SpawnedMesh->BuildFromWorldBuilder(WorldBuilder);
            }
        }

        // Spawn structures
        if (StructureSpawnerClass)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            SpawnedStructures = GetWorld()->SpawnActor<ABaseXStructureSpawner>(
                StructureSpawnerClass, GetActorLocation() + WorldSpawnOffset,
                FRotator::ZeroRotator, Params);
            if (SpawnedStructures)
            {
                SpawnedStructures->SpawnStructures(WorldBuilder);
            }
        }

        // Apply weather
        if (WeatherController)
        {
            WeatherController->ApplyWeatherState(WorldBuilder->GetWeatherState());
            WeatherController->TimeOfDay = StartHour;
            WeatherController->DaySpeed = DaySpeedMultiplier;
        }
    }

    bWasGenerating = Generating;
}

// ══════════════════════════════════════════════════════════════════════
// BLUEPRINT-CALLABLE ACTIONS
// ══════════════════════════════════════════════════════════════════════

void ABaseXVRWorldBuilder::SetPrompt(const FString& Prompt)
{
    CurrentPrompt = Prompt;
}

void ABaseXVRWorldBuilder::LoadPreset(int32 Index)
{
    if (WorldBuilder)
        CurrentPrompt = WorldBuilder->GetPresetPrompt(Index);
}

void ABaseXVRWorldBuilder::Generate()
{
    if (!WorldBuilder || CurrentPrompt.IsEmpty()) return;
    ClearWorld();
    WorldBuilder->GenerateFromPrompt(CurrentPrompt);
    bWasGenerating = true;
}

void ABaseXVRWorldBuilder::ClearWorld()
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
    if (WorldBuilder) WorldBuilder->Reset();
}

void ABaseXVRWorldBuilder::SetStartHour(float Hour)
{
    StartHour = FMath::Clamp(Hour, 0.f, 24.f);
}

void ABaseXVRWorldBuilder::SetDaySpeed(float Speed)
{
    DaySpeedMultiplier = Speed;
}

bool ABaseXVRWorldBuilder::IsGenerating() const
{
    return WorldBuilder && WorldBuilder->IsGenerating();
}

float ABaseXVRWorldBuilder::GetProgress() const
{
    return WorldBuilder ? WorldBuilder->GetPhasePercent() : 0.f;
}
