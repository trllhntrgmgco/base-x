#include "BaseXWorldGenModule.h"
#include "BaseXAssetFactory.h"
#include "BaseXMeshFactory.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FBaseXWorldGenModule"

void FBaseXWorldGenModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("BaseXWorldGen: Module loaded — DRO + WorldPromptEngine ready"));

#if WITH_EDITOR
    // Defer asset creation until engine is fully initialized
    // (GEngine is nullptr during StartupModule)
    FCoreDelegates::OnPostEngineInit.AddLambda([]()
    {
        UE_LOG(LogTemp, Log, TEXT("BaseXWorldGen: Post-engine-init — creating assets..."));
        BaseXAssets::EnsureMaterialsExist();
    });
#endif
}

void FBaseXWorldGenModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBaseXWorldGenModule, BaseXWorldGen)
