// BaseXAssetFactory.cpp — Generates terrain/water/structure materials as .uasset

#include "BaseXAssetFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

static UMaterial* CreateAndSaveMaterial(const FString& PackagePath, const FString& AssetName,
    TFunction<void(UMaterial*)> SetupFunc)
{
    FString FullPath = PackagePath + TEXT("/") + AssetName;

    // Already exists?
    if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *FullPath))
        return Existing;

    // Create package
    UPackage* Pkg = CreatePackage(*(PackagePath + TEXT("/") + AssetName));
    Pkg->FullyLoad();

    UMaterial* Mat = NewObject<UMaterial>(Pkg, *AssetName, RF_Public | RF_Standalone);
    SetupFunc(Mat);

    // Compile the material
    Mat->PreEditChange(nullptr);
    Mat->PostEditChange();

    // Save to disk
    FAssetRegistryModule::AssetCreated(Mat);
    Pkg->MarkPackageDirty();

    FString FilePath = FPackageName::LongPackageNameToFilename(
        Pkg->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Pkg, Mat, *FilePath, SaveArgs);

    UE_LOG(LogTemp, Log, TEXT("BaseXAssets: Created material %s"), *FullPath);
    return Mat;
}

static void CreateTerrainMaterial()
{
    CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials"), TEXT("M_BaseX_Terrain"),
        [](UMaterial* Mat)
    {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);

        // VertexColor → BaseColor
        UMaterialExpressionVertexColor* VC = NewObject<UMaterialExpressionVertexColor>(Mat);
        VC->MaterialExpressionEditorX = -300;
        VC->MaterialExpressionEditorY = 0;
        Mat->GetExpressionCollection().AddExpression(VC);

        // Roughness = 0.8
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.8f;
        Rough->MaterialExpressionEditorX = -300;
        Rough->MaterialExpressionEditorY = 200;
        Mat->GetExpressionCollection().AddExpression(Rough);

        // Connect
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, VC);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
    });
}

static void CreateWaterMaterial()
{
    CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials"), TEXT("M_BaseX_Water"),
        [](UMaterial* Mat)
    {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        Mat->BlendMode = BLEND_Translucent;

        // Blue base color
        UMaterialExpressionConstant4Vector* Color = NewObject<UMaterialExpressionConstant4Vector>(Mat);
        Color->Constant = FLinearColor(0.05f, 0.15f, 0.4f, 1.0f);
        Color->MaterialExpressionEditorX = -300;
        Color->MaterialExpressionEditorY = 0;
        Mat->GetExpressionCollection().AddExpression(Color);

        // Opacity = 0.6
        UMaterialExpressionConstant* Opacity = NewObject<UMaterialExpressionConstant>(Mat);
        Opacity->R = 0.6f;
        Opacity->MaterialExpressionEditorX = -300;
        Opacity->MaterialExpressionEditorY = 200;
        Mat->GetExpressionCollection().AddExpression(Opacity);

        // Roughness = 0.1 (shiny water)
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.1f;
        Rough->MaterialExpressionEditorX = -300;
        Rough->MaterialExpressionEditorY = 300;
        Mat->GetExpressionCollection().AddExpression(Rough);

        // Metallic = 0.2
        UMaterialExpressionConstant* Metal = NewObject<UMaterialExpressionConstant>(Mat);
        Metal->R = 0.2f;
        Metal->MaterialExpressionEditorX = -300;
        Metal->MaterialExpressionEditorY = 400;
        Mat->GetExpressionCollection().AddExpression(Metal);

        // Connect
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, Color);
        Mat->GetEditorOnlyData()->Opacity.Connect(0, Opacity);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
        Mat->GetEditorOnlyData()->Metallic.Connect(0, Metal);
    });
}

static void CreateStructureMaterial()
{
    CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials"), TEXT("M_BaseX_Structure"),
        [](UMaterial* Mat)
    {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);

        // VectorParameter "BaseColor" so we can tint per-instance
        UMaterialExpressionVectorParameter* ColorParam =
            NewObject<UMaterialExpressionVectorParameter>(Mat);
        ColorParam->ParameterName = TEXT("BaseColor");
        ColorParam->DefaultValue = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
        ColorParam->MaterialExpressionEditorX = -300;
        ColorParam->MaterialExpressionEditorY = 0;
        Mat->GetExpressionCollection().AddExpression(ColorParam);

        // Roughness = 0.6
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.6f;
        Rough->MaterialExpressionEditorX = -300;
        Rough->MaterialExpressionEditorY = 200;
        Mat->GetExpressionCollection().AddExpression(Rough);

        // Metallic parameter for variety
        UMaterialExpressionScalarParameter* MetalParam =
            NewObject<UMaterialExpressionScalarParameter>(Mat);
        MetalParam->ParameterName = TEXT("Metallic");
        MetalParam->DefaultValue = 0.1f;
        MetalParam->MaterialExpressionEditorX = -300;
        MetalParam->MaterialExpressionEditorY = 300;
        Mat->GetExpressionCollection().AddExpression(MetalParam);

        // Connect
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, ColorParam);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
        Mat->GetEditorOnlyData()->Metallic.Connect(0, MetalParam);
    });
}
#endif // WITH_EDITOR

// ═══════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════

void BaseXAssets::EnsureMaterialsExist()
{
#if WITH_EDITOR
    CreateTerrainMaterial();
    CreateWaterMaterial();
    CreateStructureMaterial();
    UE_LOG(LogTemp, Log, TEXT("BaseXAssets: Material check complete"));
#endif
}

UMaterialInterface* BaseXAssets::LoadTerrainMaterial()
{
    return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Terrain.M_BaseX_Terrain"));
}

UMaterialInterface* BaseXAssets::LoadWaterMaterial()
{
    return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Water.M_BaseX_Water"));
}

UMaterialInterface* BaseXAssets::LoadStructureMaterial()
{
    return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Structure.M_BaseX_Structure"));
}
