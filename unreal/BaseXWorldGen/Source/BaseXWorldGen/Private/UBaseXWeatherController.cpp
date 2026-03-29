// UBaseXWeatherController.cpp — Smooth weather transitions + sun orbit
// Direct port of the Ecosystem WLD-012 updateTime()/updateWeather() logic.

#include "UBaseXWeatherController.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"

UBaseXWeatherController::UBaseXWeatherController()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBaseXWeatherController::ApplyWeatherState(const FBaseXWeatherState& State)
{
    // v1.2: Use intensity floats for primary targets when available,
    // fall back to boolean-derived defaults for v1.0/v1.1 compatibility.
    bool hasIntensityData = (State.RainIntensity > 0.f || State.SnowIntensity > 0.f
                          || State.WindIntensity > 0.f || State.CloudCover > 0.f);

    if (hasIntensityData)
    {
        // v1.2 path: smooth intensity-driven targets
        TargetRain  = State.RainIntensity;
        TargetSnow  = State.SnowIntensity;
        TargetWind  = State.WindIntensity;
        TargetCloud = State.CloudCover;
        TargetMist  = FMath::Max(0.f, 1.f - State.Visibility) * 0.7f;
    }
    else if (State.bHasStorm)
    {
        TargetCloud = 1.f; TargetRain = 1.f; TargetWind = 0.8f;
    }
    else if (State.bHasRain)
    {
        TargetCloud = 0.8f; TargetRain = 0.6f; TargetWind = 0.3f;
    }
    else if (State.bHasSnow)
    {
        TargetCloud = 0.7f; TargetSnow = 0.8f; TargetWind = 0.15f;
    }
    else if (State.bHasAsh)
    {
        TargetCloud = 0.9f; TargetMist = 0.5f; TargetWind = 0.4f;
    }
    else if (State.bHasMist)
    {
        TargetCloud = 0.4f; TargetMist = 0.7f; TargetWind = 0.05f;
    }
    else if (State.bIsScorching)
    {
        TargetCloud = 0.05f; TargetRain = 0.f; TargetMist = 0.f; TargetWind = 0.05f;
    }
    else
    {
        TargetCloud = 0.1f; TargetRain = 0.f; TargetSnow = 0.f;
        TargetMist = 0.f; TargetWind = 0.1f;
    }

    // Time of day hints
    if (State.bIsMoonlit)  TimeOfDay = 22.f;
    if (State.bIsDawn)     TimeOfDay = 6.f;

    // Aurora adds subtle glow (handled in tick via fog color)
    if (State.bHasAurora)  TargetMist = FMath::Max(TargetMist, 0.2f);

    UE_LOG(LogTemp, Log, TEXT("WeatherController v1.2: Cloud=%.2f Rain=%.2f Snow=%.2f Mist=%.2f Wind=%.2f Vis=%.2f Temp=%.2f ToD=%.0f"),
        TargetCloud, TargetRain, TargetSnow, TargetMist, TargetWind,
        State.Visibility, State.Temperature, TimeOfDay);
}

void UBaseXWeatherController::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ── Advance time ──
    if (DaySpeed > 0.f)
    {
        TimeOfDay += (DeltaTime * DaySpeed * 24.f) / DayLengthSeconds;
        if (TimeOfDay >= 24.f) TimeOfDay -= 24.f;
    }

    // ── Smooth weather interpolation ──
    float Rate = DeltaTime * 0.5f;
    CloudCover    += (TargetCloud - CloudCover) * Rate;
    RainIntensity += (TargetRain  - RainIntensity) * Rate;
    SnowIntensity += (TargetSnow  - SnowIntensity) * Rate;
    MistDensity   += (TargetMist  - MistDensity) * Rate;
    WindStrength  += (TargetWind  - WindStrength) * Rate;

    // ── Sun angle (same formula as Ecosystem) ──
    float DayFraction = TimeOfDay / 24.f;
    float SunAngle = (DayFraction - 0.25f) * 2.f * PI;
    float SunHeight = FMath::Sin(SunAngle);

    AmbientBrightness = FMath::Max(0.f, SunHeight) * 0.7f + 0.3f;
    AmbientBrightness *= (1.f - CloudCover * 0.5f);

    UpdateSunPosition();
    UpdateFog();
}

void UBaseXWeatherController::UpdateSunPosition()
{
    if (!SunLight) return;

    float DayFraction = TimeOfDay / 24.f;
    float SunAngle = (DayFraction - 0.25f) * 2.f * PI;

    // Sun orbits: elevation from SunAngle, fixed azimuth offset
    float Elevation = FMath::RadiansToDegrees(SunAngle);
    float Azimuth = 180.f; // south-facing

    SunLight->SetActorRotation(FRotator(-Elevation, Azimuth, 0.f));

    // Intensity follows day curve
    float Intensity = FMath::Max(0.f, FMath::Sin(SunAngle));
    Intensity *= (1.f - CloudCover * 0.6f);
    SunLight->GetLightComponent()->SetIntensity(FMath::Max(0.1f, Intensity * 10.f));

    // Colour: warm at low angles (golden hour), white at noon
    float LowAngle = FMath::Max(0.f, 1.f - FMath::Sin(SunAngle) * 4.f);
    FLinearColor SunColor = FMath::Lerp(
        FLinearColor(1.f, 0.95f, 0.85f),  // noon white
        FLinearColor(1.f, 0.6f, 0.3f),    // golden hour
        LowAngle);
    SunLight->GetLightComponent()->SetLightColor(SunColor);
}

void UBaseXWeatherController::UpdateFog()
{
    if (!Fog) return;

    UExponentialHeightFogComponent* FogComp = Fog->GetComponent();
    if (!FogComp) return;

    // Base fog density + weather additions
    float Density = 0.002f + RainIntensity * 0.008f + MistDensity * 0.015f + SnowIntensity * 0.004f;
    FogComp->SetFogDensity(Density);

    // Fog colour matches sky horizon (simplified)
    float DayF = FMath::Max(0.f, AmbientBrightness);
    FLinearColor FogColor(
        0.45f + DayF * 0.25f,
        0.50f + DayF * 0.25f,
        0.62f + DayF * 0.25f
    );
    FogColor *= (1.f - CloudCover * 0.3f);
    FogComp->SetFogInscatteringColor(FogColor);
}
