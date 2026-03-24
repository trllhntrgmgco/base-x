// UBaseXWeatherController.h — Bridges Base-X WeatherState to UE atmosphere systems.
// Drives DirectionalLight (sun), SkyAtmosphere, ExponentialHeightFog, and
// Niagara weather particles based on the parsed weather keywords.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FBaseXTypes.h"
#include "UBaseXWeatherController.generated.h"

class ADirectionalLight;
class ASkyAtmosphere;
class AExponentialHeightFog;
class UNiagaraComponent;

UCLASS(BlueprintType, Blueprintable, ClassGroup=(BaseX), meta=(BlueprintSpawnableComponent))
class BASEXWORLDGEN_API UBaseXWeatherController : public UActorComponent
{
    GENERATED_BODY()

public:
    UBaseXWeatherController();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // Apply a Base-X weather state. Call once after generation, then
    // TickComponent handles smooth transitions.
    UFUNCTION(BlueprintCallable, Category="BaseX|Weather")
    void ApplyWeatherState(const FBaseXWeatherState& State);

    // ── Scene references (assign in editor or find at runtime) ──

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Weather")
    TObjectPtr<ADirectionalLight> SunLight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Weather")
    TObjectPtr<AExponentialHeightFog> Fog;

    // ── Time of day ──

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Weather", meta=(ClampMin="0", ClampMax="24"))
    float TimeOfDay = 10.f;  // hours 0-24

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Weather")
    float DaySpeed = 1.f;    // multiplier (0 = paused)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BaseX|Weather")
    float DayLengthSeconds = 120.f;

    // ── Live weather parameters (smoothly interpolated) ──

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Weather")
    float CloudCover = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Weather")
    float RainIntensity = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Weather")
    float SnowIntensity = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Weather")
    float MistDensity = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Weather")
    float WindStrength = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BaseX|Weather")
    float AmbientBrightness = 1.f;

private:
    // Targets we interpolate toward
    float TargetCloud = 0.f, TargetRain = 0.f, TargetSnow = 0.f;
    float TargetMist = 0.f, TargetWind = 0.f;

    void UpdateSunPosition();
    void UpdateFog();
};
