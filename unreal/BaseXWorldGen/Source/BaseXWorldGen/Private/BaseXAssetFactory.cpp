// BaseXAssetFactory.cpp - Lazy material creation for terrain/water/structures.
// Materials are created on first Load*() call, not at module startup.

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
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

static UMaterial* CreateAndSaveMaterial(const FString& AssetPath, const FString& AssetName, TFunction<void(UMaterial*)> SetupFunc)
{
    FString FullPath = AssetPath + TEXT(".") + AssetName;
    if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, *FullPath))
        return Existing;
    if (!GEngine || !IsInGameThread())
        return nullptr;

    UPackage* Pkg = CreatePackage(*AssetPath);
    if (!Pkg) return nullptr;
    Pkg->FullyLoad();

    UMaterial* Mat = NewObject<UMaterial>(Pkg, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    SetupFunc(Mat);
    Mat->PreEditChange(nullptr);
    Mat->PostEditChange();

    FAssetRegistryModule::AssetCreated(Mat);
    Pkg->MarkPackageDirty();
    FString FilePath = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Pkg, Mat, *FilePath, SaveArgs);
    UE_LOG(LogTemp, Log, TEXT("BaseXAssets: Created %s (saved=%d)"), *AssetPath, bSaved);
    return Mat;
}

static UMaterial* EnsureTerrainMaterial()
{
    return CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials/M_BaseX_Terrain"), TEXT("M_BaseX_Terrain"), [](UMaterial* Mat) {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        UMaterialExpressionVertexColor* VC = NewObject<UMaterialExpressionVertexColor>(Mat);
        Mat->GetExpressionCollection().AddExpression(VC);
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.8f;
        Mat->GetExpressionCollection().AddExpression(Rough);
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, VC);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
    });
}

static UMaterial* EnsureWaterMaterial()
{
    return CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials/M_BaseX_Water"), TEXT("M_BaseX_Water"), [](UMaterial* Mat) {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        Mat->BlendMode = BLEND_Translucent;
        UMaterialExpressionConstant4Vector* Color = NewObject<UMaterialExpressionConstant4Vector>(Mat);
        Color->Constant = FLinearColor(0.05f, 0.15f, 0.4f, 1.0f);
        Mat->GetExpressionCollection().AddExpression(Color);
        UMaterialExpressionConstant* Opacity = NewObject<UMaterialExpressionConstant>(Mat);
        Opacity->R = 0.6f;
        Mat->GetExpressionCollection().AddExpression(Opacity);
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.1f;
        Mat->GetExpressionCollection().AddExpression(Rough);
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, Color);
        Mat->GetEditorOnlyData()->Opacity.Connect(0, Opacity);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
    });
}

static UMaterial* EnsureStructureMaterial()
{
    return CreateAndSaveMaterial(TEXT("/Game/BaseX/Materials/M_BaseX_Structure"), TEXT("M_BaseX_Structure"), [](UMaterial* Mat) {
        Mat->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
        Mat->bUsedWithInstancedStaticMeshes = true;
        UMaterialExpressionVectorParameter* ColorParam = NewObject<UMaterialExpressionVectorParameter>(Mat);
        ColorParam->ParameterName = TEXT("BaseColor");
        ColorParam->DefaultValue = FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
        Mat->GetExpressionCollection().AddExpression(ColorParam);
        UMaterialExpressionConstant* Rough = NewObject<UMaterialExpressionConstant>(Mat);
        Rough->R = 0.6f;
        Mat->GetExpressionCollection().AddExpression(Rough);
        UMaterialExpressionScalarParameter* MetalParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
        MetalParam->ParameterName = TEXT("Metallic");
        MetalParam->DefaultValue = 0.1f;
        Mat->GetExpressionCollection().AddExpression(MetalParam);
        Mat->GetEditorOnlyData()->BaseColor.Connect(0, ColorParam);
        Mat->GetEditorOnlyData()->Roughness.Connect(0, Rough);
        Mat->GetEditorOnlyData()->Metallic.Connect(0, MetalParam);
    });
}
#endif

void BaseXAssets::EnsureMaterialsExist()
{
#if WITH_EDITOR
    EnsureTerrainMaterial();
    EnsureWaterMaterial();
    EnsureStructureMaterial();
#endif
}

UMaterialInterface* BaseXAssets::LoadTerrainMaterial()
{
    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Terrain.M_BaseX_Terrain"));
#if WITH_EDITOR
    if (!Mat) Mat = EnsureTerrainMaterial();
#endif
    return Mat;
}

UMaterialInterface* BaseXAssets::LoadWaterMaterial()
{
    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Water.M_BaseX_Water"));
#if WITH_EDITOR
    if (!Mat) Mat = EnsureWaterMaterial();
#endif
    return Mat;
}

UMaterialInterface* BaseXAssets::LoadStructureMaterial()
{
    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/BaseX/Materials/M_BaseX_Structure.M_BaseX_Structure"));
#if WITH_EDITOR
    if (!Mat) Mat = EnsureStructureMaterial();
#endif
    return Mat;
}
