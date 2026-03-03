#include "BaseXWorldGenModule.h"
#include "BaseXAssetFactory.h"

#define LOCTEXT_NAMESPACE "FBaseXWorldGenModule"

void FBaseXWorldGenModule::StartupModule()
{
    UE_LOG(LogTemp, Log, TEXT("BaseXWorldGen: Module loaded — DRO + WorldPromptEngine ready"));

    // Create material assets on first editor load
    BaseXAssets::EnsureMaterialsExist();
}

void FBaseXWorldGenModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBaseXWorldGenModule, BaseXWorldGen)
