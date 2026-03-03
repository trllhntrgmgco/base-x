// ABaseXVRWorldBuilder.h — VR world-builder tablet actor.
// Place in level; it creates a WidgetComponent with the prompt UI,
// manages the UBaseXWorldBuilder lifecycle, and spawns/clears the world.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABaseXVRWorldBuilder.generated.h"

class UBaseXWorldBuilder;
class ABaseXWorldMesh;
class ABaseXStructureSpawner;
class UBaseXWeatherController;
class UWidgetComponent;
class UUserWidget;

UCLASS(BlueprintType, Blueprintable)
class BASEXWORLDGEN_API ABaseXVRWorldBuilder : public AActor
{
    GENERATED_BODY()

public:
    ABaseXVRWorldBuilder();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ── Blueprint-callable actions (wire to VR UI buttons) ──

    UFUNCTION(BlueprintCallable, Category="BaseX|VR")
    void SetPrompt(const FString& Prompt);

    UFUNCTION(BlueprintCallable, Category="BaseX|VR")
    void LoadPreset(int32 Index);

    UFUNCTION(BlueprintCallable, Category="BaseX|VR")
    void Generate();

    UFUNCTION(BlueprintCallable, Category="BaseX|VR")
    void ClearWorld();

    UFUNCTION(BlueprintCallable, Category="BaseX|VR")
    void SetStartHour(float Hour);

    UFUNCTION(BlueprintCallable, Category="BaseX|VR")
    void SetDaySpeed(float Speed);

    // ── State queries for UI binding ──

    UFUNCTION(BlueprintPure, Category="BaseX|VR")
    bool IsGenerating() const;

    UFUNCTION(BlueprintPure, Category="BaseX|VR")
    float GetProgress() const;

    UFUNCTION(BlueprintPure, Category="BaseX|VR")
    FString GetCurrentPrompt() const { return CurrentPrompt; }

    // ── Scene references ──

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|VR")
    TSubclassOf<ABaseXWorldMesh> WorldMeshClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|VR")
    TSubclassOf<ABaseXStructureSpawner> StructureSpawnerClass;

    // The UMG widget class for the tablet UI (create in editor)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|VR")
    TSubclassOf<UUserWidget> TabletWidgetClass;

    // Spawn location offset for the generated world
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|VR")
    FVector WorldSpawnOffset = FVector(0, 0, 0);

    // Start hour (0-24)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|VR", meta=(ClampMin="0", ClampMax="24"))
    float StartHour = 10.f;

    // Day speed multiplier
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|VR")
    float DaySpeedMultiplier = 1.f;

    // ── Internal objects ──

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|VR")
    TObjectPtr<UBaseXWorldBuilder> WorldBuilder;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|VR")
    TObjectPtr<UWidgetComponent> TabletWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|VR")
    TObjectPtr<UBaseXWeatherController> WeatherController;

private:
    FString CurrentPrompt;

    UPROPERTY()
    TObjectPtr<ABaseXWorldMesh> SpawnedMesh;

    UPROPERTY()
    TObjectPtr<ABaseXStructureSpawner> SpawnedStructures;

    bool bWasGenerating = false;
};
