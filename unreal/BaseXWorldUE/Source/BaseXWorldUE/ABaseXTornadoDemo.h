// ABaseXTornadoDemo.h — Port of WLD-007 3D Tornado to Unreal Engine.
// Rankine vortex physics, EF0-EF5 scales, DRO precision, debris particles.
// Place in level → Hit Play → tornado spawns with interactive controls.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABaseXTornadoDemo.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;

// EF Scale definition
USTRUCT(BlueprintType)
struct FBaseXEFScale
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FString Name;
    UPROPERTY(EditAnywhere) int32 WindMin = 65;
    UPROPERTY(EditAnywhere) int32 WindMax = 85;
    UPROPERTY(EditAnywhere) int32 FunnelWidth = 50;
    UPROPERTY(EditAnywhere) FLinearColor Color = FLinearColor::White;
};

// Single debris particle
struct FTornadoParticle
{
    FVector Position;
    FVector Velocity;
    float Size;
    float Life;
    float Rotation;
    int32 DROBase;   // 64,32,16,8,4
    int32 Type;      // 0=dust 1=leaf 2=wood 3=brick 4=car
};

UCLASS(BlueprintType, Blueprintable)
class BASEXWORLDUE_API ABaseXTornadoDemo : public AActor
{
    GENERATED_BODY()

public:
    ABaseXTornadoDemo();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // ── Tornado parameters (editable in Details) ──

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado", meta=(ClampMin="0", ClampMax="5"))
    int32 EFScale = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado")
    float RotationSpeed = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado")
    float FunnelWidth = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado")
    bool bShowDebris = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado")
    int32 MaxParticles = 3000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado")
    float TornadoMoveSpeed = 50.f;  // UE units/sec

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tornado")
    float WorldScale = 100.f;  // GL units → UE units multiplier

    // ── Runtime stats ──

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tornado|Stats")
    int32 ParticleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tornado|Stats")
    TArray<int32> DROBaseCounts;  // [0]=B64, [1]=B32, [2]=B16, [3]=B8, [4]=B4

private:
    // Funnel mesh rings (InstancedStaticMesh for the funnel segments)
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> DebrisISM;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> FunnelISM;

    TArray<FTornadoParticle> Particles;
    TArray<FBaseXEFScale> EFScales;

    float TornadoX = 0.f;
    float TornadoAngle = 0.f;
    float ElapsedTime = 0.f;

    FRandomStream Rng;

    void SpawnParticles(float DeltaTime);
    void UpdateParticles(float DeltaTime, const FVector& CamPos);
    void VortexForce(const FVector& ParticlePos, float CenterX, float Radius,
                     FVector& OutForce) const;
    void RebuildInstances();
    void BuildFunnelInstances();
    void SetupEFScales();
};
