// ABaseXTornadoDemo.cpp — Port of WLD-007 3D Tornado to Unreal Engine.
// Rankine vortex physics, EF0-EF5, DRO precision, InstancedStaticMesh debris.

#include "ABaseXTornadoDemo.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"

static constexpr float BX_PI = 3.14159265f;

// ══════════════════════════════════════════════════════════════════════
// SETUP
// ══════════════════════════════════════════════════════════════════════

ABaseXTornadoDemo::ABaseXTornadoDemo()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Debris uses instanced cubes
    DebrisISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DebrisISM"));
    DebrisISM->SetupAttachment(Root);
    DebrisISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DebrisISM->SetCastShadow(false);
    DebrisISM->NumCustomDataFloats = 4; // R, G, B, A for per-instance color

    // Funnel uses instanced cylinders
    FunnelISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FunnelISM"));
    FunnelISM->SetupAttachment(Root);
    FunnelISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FunnelISM->SetCastShadow(false);
    FunnelISM->NumCustomDataFloats = 4;

    DROBaseCounts.SetNum(5);
}

void ABaseXTornadoDemo::SetupEFScales()
{
    EFScales = {
        { TEXT("EF0"),  65,  85,  50, FLinearColor(0.27f, 0.67f, 0.27f) },
        { TEXT("EF1"),  86, 110, 100, FLinearColor(0.53f, 0.80f, 0.27f) },
        { TEXT("EF2"), 111, 135, 200, FLinearColor(0.80f, 0.80f, 0.27f) },
        { TEXT("EF3"), 136, 165, 300, FLinearColor(1.00f, 0.67f, 0.27f) },
        { TEXT("EF4"), 166, 200, 400, FLinearColor(1.00f, 0.40f, 0.27f) },
        { TEXT("EF5"), 201, 250, 500, FLinearColor(1.00f, 0.27f, 0.27f) }
    };
}

void ABaseXTornadoDemo::BeginPlay()
{
    Super::BeginPlay();

    SetupEFScales();
    Rng.Initialize(42);

    // Load engine cube for debris
    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh)
        DebrisISM->SetStaticMesh(CubeMesh);

    // Load engine cylinder for funnel
    UStaticMesh* CylMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylMesh)
        FunnelISM->SetStaticMesh(CylMesh);

    // Create material that reads per-instance custom data for color
    UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/BaseX/Materials/M_BaseX_Structure.M_BaseX_Structure"));
    if (!BaseMat)
        BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);

    if (BaseMat)
    {
        UMaterialInstanceDynamic* DebrisMat = UMaterialInstanceDynamic::Create(BaseMat, this);
        DebrisISM->SetMaterial(0, DebrisMat);

        UMaterialInstanceDynamic* FunnelMat = UMaterialInstanceDynamic::Create(BaseMat, this);
        FunnelMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.5f, 0.55f, 0.6f, 0.3f));
        FunnelISM->SetMaterial(0, FunnelMat);
    }

    // Apply initial EF scale
    if (EFScales.IsValidIndex(EFScale))
    {
        RotationSpeed = (EFScales[EFScale].WindMin + EFScales[EFScale].WindMax) * 0.5f;
        FunnelWidth = EFScales[EFScale].FunnelWidth;
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXTornadoDemo: %s active, %d max particles, scale=%.0f"),
        EFScales.IsValidIndex(EFScale) ? *EFScales[EFScale].Name : TEXT("EF?"),
        MaxParticles, WorldScale);

    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow,
            FString::Printf(TEXT("TORNADO DEMO: %s | %d-%d MPH | 1-6=EF scale"),
                EFScales.IsValidIndex(EFScale) ? *EFScales[EFScale].Name : TEXT("?"),
                EFScales.IsValidIndex(EFScale) ? EFScales[EFScale].WindMin : 0,
                EFScales.IsValidIndex(EFScale) ? EFScales[EFScale].WindMax : 0));
}

// ══════════════════════════════════════════════════════════════════════
// RANKINE VORTEX PHYSICS
// ══════════════════════════════════════════════════════════════════════

void ABaseXTornadoDemo::VortexForce(const FVector& Pos, float CenterX, float Radius,
                                     FVector& OutForce) const
{
    float dx = Pos.X - CenterX;
    float dz = Pos.Y;  // UE Y = GL Z
    float dist = FMath::Sqrt(dx * dx + dz * dz);

    if (dist < 0.1f * WorldScale)
    {
        OutForce = FVector::ZeroVector;
        return;
    }

    float core = Radius * 0.3f;
    float tang = dist < core
        ? (RotationSpeed / 100.f) * (dist / core)
        : (RotationSpeed / 100.f) * (core / dist);

    float angle = FMath::Atan2(dz, dx);

    OutForce.X = -FMath::Sin(angle) * tang;
    OutForce.Y =  FMath::Cos(angle) * tang;

    // Inward pull
    float inward = dist < Radius ? 0.5f * (1.f - dist / Radius) : 0.f;
    OutForce.X -= (dx / dist) * inward;
    OutForce.Y -= (dz / dist) * inward;

    // Updraft
    OutForce.Z = dist < core ? 3.0f * (1.f - dist / core) : 0.f;

    OutForce *= WorldScale;
}

// ══════════════════════════════════════════════════════════════════════
// SPAWN + UPDATE
// ══════════════════════════════════════════════════════════════════════

void ABaseXTornadoDemo::SpawnParticles(float DeltaTime)
{
    if (!bShowDebris) return;

    float rate = EFScale * 15.0f * DeltaTime;
    float funnelR = FunnelWidth * 0.01f * WorldScale;

    for (int32 i = 0; i < (int32)rate && Particles.Num() < MaxParticles; i++)
    {
        FTornadoParticle P;
        float a = Rng.FRand() * 2.f * BX_PI;
        float r = Rng.FRand() * funnelR * 0.5f;

        P.Position = FVector(
            TornadoX + FMath::Cos(a) * r,
            FMath::Sin(a) * r,
            0.f
        );
        P.Velocity = FVector::ZeroVector;
        P.Rotation = Rng.FRand() * 2.f * BX_PI;
        P.Life = 1.f;

        float tr = Rng.FRand();
        if      (tr < 0.50f) { P.Type = 0; P.Size = 15.f; }   // dust
        else if (tr < 0.70f) { P.Type = 1; P.Size = 25.f; }   // leaf
        else if (tr < 0.85f) { P.Type = 2; P.Size = 35.f; }   // wood
        else if (tr < 0.95f) { P.Type = 3; P.Size = 50.f; }   // brick
        else                 { P.Type = 4; P.Size = 70.f; }   // car

        P.DROBase = 32;
        Particles.Add(P);
    }
}

void ABaseXTornadoDemo::UpdateParticles(float DeltaTime, const FVector& CamPos)
{
    float funnelR = FunnelWidth * 0.01f * WorldScale;

    for (int32 i = 0; i < 5; i++)
        DROBaseCounts[i] = 0;

    for (int32 i = Particles.Num() - 1; i >= 0; i--)
    {
        FTornadoParticle& P = Particles[i];

        FVector Force;
        VortexForce(P.Position, TornadoX, funnelR, Force);

        P.Velocity += Force * DeltaTime * 2.f;
        P.Velocity.Z -= 0.8f * WorldScale * DeltaTime;  // gravity
        P.Velocity *= 0.98f;

        P.Position += P.Velocity * DeltaTime * 5.f;
        P.Rotation += DeltaTime * 2.f;

        // Ground bounce
        if (P.Position.Z < 0.f)
        {
            P.Position.Z = 0.f;
            P.Velocity.Z *= -0.3f;
            P.Life -= 0.05f;
        }

        // DRO: distance to camera → pick radix base
        float dist = FVector::Dist(P.Position, CamPos - GetActorLocation());
        if      (dist < 2000.f)  P.DROBase = 64;
        else if (dist < 5000.f)  P.DROBase = 32;
        else if (dist < 10000.f) P.DROBase = 16;
        else if (dist < 20000.f) P.DROBase = 8;
        else                     P.DROBase = 4;

        switch (P.DROBase)
        {
            case 64: DROBaseCounts[0]++; break;
            case 32: DROBaseCounts[1]++; break;
            case 16: DROBaseCounts[2]++; break;
            case 8:  DROBaseCounts[3]++; break;
            default: DROBaseCounts[4]++; break;
        }

        // Distance from funnel center
        float ddist = FMath::Sqrt(
            FMath::Square(P.Position.X - TornadoX) +
            FMath::Square(P.Position.Y));
        if (ddist > funnelR * 2.f) P.Life -= 0.01f * DeltaTime;
        if (P.Position.Z > 25.f * WorldScale) P.Life -= 0.02f * DeltaTime;

        // Remove dead particles
        float bounds = 50.f * WorldScale;
        if (P.Life <= 0.f || P.Position.Z > 30.f * WorldScale
            || FMath::Abs(P.Position.X) > bounds
            || FMath::Abs(P.Position.Y) > bounds)
        {
            Particles.RemoveAtSwap(i);
        }
    }

    ParticleCount = Particles.Num();
}

// ══════════════════════════════════════════════════════════════════════
// INSTANCE REBUILDS
// ══════════════════════════════════════════════════════════════════════

static FLinearColor DROBaseColor(int32 Base)
{
    switch (Base)
    {
        case 64: return FLinearColor(1.f, 0.15f, 0.15f);
        case 32: return FLinearColor(1.f, 0.80f, 0.15f);
        case 16: return FLinearColor(0.15f, 1.f, 0.30f);
        case 8:  return FLinearColor(0.20f, 0.40f, 1.f);
        default: return FLinearColor(0.45f, 0.45f, 0.45f);
    }
}

void ABaseXTornadoDemo::RebuildInstances()
{
    DebrisISM->ClearInstances();

    FVector ActorLoc = GetActorLocation();

    for (const FTornadoParticle& P : Particles)
    {
        FTransform T;
        T.SetLocation(ActorLoc + P.Position);
        T.SetScale3D(FVector(P.Size / 100.f));  // BasicShapes are 100uu
        T.SetRotation(FQuat(FRotator(0, FMath::RadiansToDegrees(P.Rotation), 0)));

        int32 Idx = DebrisISM->AddInstance(T, false);

        FLinearColor C = DROBaseColor(P.DROBase);
        float Alpha = (0.4f + (P.DROBase / 64.f) * 0.6f) * P.Life;
        DebrisISM->SetCustomDataValue(Idx, 0, C.R);
        DebrisISM->SetCustomDataValue(Idx, 1, C.G);
        DebrisISM->SetCustomDataValue(Idx, 2, C.B);
        DebrisISM->SetCustomDataValue(Idx, 3, Alpha);
    }

    DebrisISM->MarkRenderStateDirty();
}

void ABaseXTornadoDemo::BuildFunnelInstances()
{
    FunnelISM->ClearInstances();

    float funnelR = FunnelWidth * 0.01f * WorldScale;
    int32 Rings = 20;
    int32 Segs = 16;
    FVector ActorLoc = GetActorLocation();

    for (int32 r = 0; r < Rings; r++)
    {
        float t = r / (float)Rings;
        float y = t * 20.f * WorldScale;
        float radius = funnelR * (1.f - t * 0.7f);
        float spiral = TornadoAngle + t * 8.f;

        for (int32 s = 0; s < Segs; s++)
        {
            float a = 2.f * BX_PI * s / Segs;
            float wobble = FMath::Sin(a * 3.f + spiral) * radius * 0.15f;

            FVector Pos(
                TornadoX + FMath::Cos(a + spiral) * (radius + wobble),
                FMath::Sin(a + spiral) * (radius + wobble),
                y
            );

            FTransform T;
            T.SetLocation(ActorLoc + Pos);
            float segScale = (radius / WorldScale) * 0.04f;
            T.SetScale3D(FVector(segScale, segScale, WorldScale * 0.05f));

            int32 Idx = FunnelISM->AddInstance(T, false);
            float alpha = 0.12f + 0.08f * FMath::Sin(t * BX_PI);
            FunnelISM->SetCustomDataValue(Idx, 0, 0.5f);
            FunnelISM->SetCustomDataValue(Idx, 1, 0.55f);
            FunnelISM->SetCustomDataValue(Idx, 2, 0.6f);
            FunnelISM->SetCustomDataValue(Idx, 3, alpha);
        }
    }

    FunnelISM->MarkRenderStateDirty();
}

// ══════════════════════════════════════════════════════════════════════
// TICK
// ══════════════════════════════════════════════════════════════════════

void ABaseXTornadoDemo::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    DeltaTime = FMath::Min(DeltaTime, 0.05f);

    ElapsedTime += DeltaTime;
    TornadoAngle += RotationSpeed * 0.01f * DeltaTime;
    TornadoX += TornadoMoveSpeed * DeltaTime;
    if (TornadoX > 30.f * WorldScale) TornadoX = -30.f * WorldScale;

    // Get camera position for DRO
    FVector CamPos = GetActorLocation();
    APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (CamMgr)
        CamPos = CamMgr->GetCameraLocation();

    // Input: number keys for EF scale
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        for (int32 Key = 0; Key < 6; Key++)
        {
            if (PC->WasInputKeyJustPressed(FKey(*FString::Printf(TEXT("%d"), Key + 1))))
            {
                EFScale = Key;
                if (EFScales.IsValidIndex(EFScale))
                {
                    RotationSpeed = (EFScales[EFScale].WindMin + EFScales[EFScale].WindMax) * 0.5f;
                    FunnelWidth = EFScales[EFScale].FunnelWidth;
                }
                if (GEngine)
                    GEngine->AddOnScreenDebugMessage(1, 3.f,
                        EFScales[EFScale].Color.ToFColor(true),
                        FString::Printf(TEXT("%s — %d-%d MPH"),
                            *EFScales[EFScale].Name,
                            EFScales[EFScale].WindMin,
                            EFScales[EFScale].WindMax));
            }
        }
    }

    SpawnParticles(DeltaTime);
    UpdateParticles(DeltaTime, CamPos);
    RebuildInstances();
    BuildFunnelInstances();

    // HUD stats
    if (GEngine && FMath::Fmod(ElapsedTime, 0.5f) < DeltaTime)
    {
        int32 Total = FMath::Max(1, ParticleCount);
        GEngine->AddOnScreenDebugMessage(10, 0.6f, FColor::Cyan,
            FString::Printf(TEXT("TORNADO %s | Particles: %d | Wind: %.0f MPH"),
                EFScales.IsValidIndex(EFScale) ? *EFScales[EFScale].Name : TEXT("?"),
                ParticleCount, RotationSpeed));
        GEngine->AddOnScreenDebugMessage(11, 0.6f, FColor::Red,
            FString::Printf(TEXT("DRO  B64:%d%%  B32:%d%%  B16:%d%%  B8:%d%%  B4:%d%%"),
                DROBaseCounts[0] * 100 / Total,
                DROBaseCounts[1] * 100 / Total,
                DROBaseCounts[2] * 100 / Total,
                DROBaseCounts[3] * 100 / Total,
                DROBaseCounts[4] * 100 / Total));
    }
}
