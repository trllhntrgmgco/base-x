// ABaseXPromptDemo.h — Complete in-game prompt UI demo.
// Place in level → hit Play → type prompts → worlds generate.
// Uses Slate (C++ UI) so no Blueprint widget assets are needed.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Widgets/SCompoundWidget.h"
#include "SBaseXHub.h"
#include "SBaseXAssetMapper.h"
#include "ABaseXPromptDemo.generated.h"

class UBaseXWorldBuilder;
class ABaseXWorldMesh;
class ABaseXStructureSpawner;
class UBaseXWeatherController;
class ADirectionalLight;
class AExponentialHeightFog;

// ══════════════════════════════════════════════════════════════════════
// ACTOR — manages lifecycle, generation, and world spawning
// ══════════════════════════════════════════════════════════════════════

UCLASS(BlueprintType, Blueprintable)
class BASEXWORLDUE_API ABaseXPromptDemo : public AActor
{
    GENERATED_BODY()

public:
    ABaseXPromptDemo();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    void RequestGenerate(const FString& Prompt);
    void RequestClear();
    FString GetPresetPrompt(int32 Index) const;
    int32 GetPresetCount() const;
    UBaseXWorldBuilder* GetBuilder() const { return Builder; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float CellWorldSize = 400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX")
    float HeightScale = 4000.f;

private:
    UPROPERTY()
    TObjectPtr<UBaseXWorldBuilder> Builder;

    UPROPERTY()
    TObjectPtr<ABaseXWorldMesh> SpawnedMesh;

    UPROPERTY()
    TObjectPtr<ABaseXStructureSpawner> SpawnedStructures;

    UPROPERTY()
    TObjectPtr<UBaseXWeatherController> WeatherCtrl;

    UPROPERTY()
    TObjectPtr<ADirectionalLight> SpawnedSun;

    UPROPERTY()
    TObjectPtr<AExponentialHeightFog> SpawnedFog;

    TSharedPtr<SBaseXHub> Hub;
    TSharedPtr<SWidget> ViewportWrapper;
    bool bGenerating = false;

    void EnsureSceneLighting();
};
