// SBaseXAssetMapper.h - "Bring Your Own Assets" UI panel for Base-X v1.1
// Scans the user's project for StaticMesh assets, shows all 186 structure
// keywords as assignable buttons, and lets users map any mesh to any keyword.
// Also provides a terrain-size selector (tiles) for unique world dimensions.
//
// (c) 2026 Keenan Dunham and Troll Hunter Gaming. All Rights Reserved.
// DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"

class UBaseXWorldBuilder;
class SWrapBox;
class SScrollBox;
class SEditableTextBox;
class STextBlock;

// ======================================================================
// Single keyword -> mesh assignment
// ======================================================================

struct FBaseXAssetMapping
{
    FString Keyword;
    FString Category;
    TSoftObjectPtr<UStaticMesh> UserMesh;
    TSoftObjectPtr<UStaticMesh> DefaultMesh;
    float ScaleMultiplier = 1.f;
    FVector PivotOffset = FVector::ZeroVector;
    bool bIsUserAssigned = false;
};

// ======================================================================
// Discovered project asset (from Content Browser scan)
// ======================================================================

struct FBaseXDiscoveredAsset
{
    FString AssetName;
    FString AssetPath;
    TSoftObjectPtr<UStaticMesh> Mesh;
    TArray<FString> MatchedKeywords;
};

// ======================================================================
// Terrain size preset
// ======================================================================

struct FBaseXTerrainSize
{
    FString Label;
    int32 GridSize;
    float WorldScale;
};

// ======================================================================
// SLATE PANEL - Asset Mapper + Terrain Size UI
// ======================================================================

class BASEXWORLDGEN_API SBaseXAssetMapper : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBaseXAssetMapper) {}
        SLATE_ARGUMENT(UBaseXWorldBuilder*, Builder)
        SLATE_ARGUMENT(UDataTable*, MeshTable)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void ScanProjectAssets();
    void AutoMatchAssets();
    void ApplyMappingsToDataTable();
    void SaveMappings();
    void LoadMappings();

    int32 GetUserMappedCount() const;
    int32 GetAutoMatchedCount() const;
    int32 GetTotalKeywordCount() const;
    int32 GetSelectedGridSize() const;
    float GetSelectedWorldScale() const;

private:
    UBaseXWorldBuilder* Builder = nullptr;
    UDataTable* MeshTable = nullptr;
    TArray<FBaseXAssetMapping> Mappings;
    TArray<FBaseXDiscoveredAsset> DiscoveredAssets;
    TArray<FBaseXTerrainSize> TerrainSizes;
    int32 SelectedTerrainIndex = 2;
    int32 ActiveCategoryIndex = -1;
    FString SearchFilter;
    TSharedPtr<SScrollBox> KeywordListBox;
    TSharedPtr<SScrollBox> AssetListBox;
    TSharedPtr<SEditableTextBox> SearchBox;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<STextBlock> TerrainInfoText;
    TSharedPtr<SWrapBox> CategoryTabBox;
    TSharedPtr<SWrapBox> TerrainSizeRow;
    int32 SelectedKeywordIndex = INDEX_NONE;

    struct FCategoryDef { FString Label; TArray<FString> Keywords; };
    TArray<FCategoryDef> Categories;

    void BuildCategoryData();
    void BuildTerrainSizes();
    void RebuildKeywordList();
    void RebuildAssetList();
    void UpdateStatus();
    FReply OnScanClicked();
    FReply OnAutoMatchClicked();
    FReply OnApplyClicked();
    FReply OnSaveClicked();
    FReply OnLoadClicked();
    FReply OnClearAllClicked();
    FReply OnTerrainSizeSelected(int32 Index);
    void OnKeywordSelected(int32 Index);
    void OnAssetAssigned(int32 AssetIndex);
    void OnClearKeywordAssignment(int32 KeywordIndex);
    void OnSearchChanged(const FText& NewText);
    float CalculateMatchScore(const FString& AssetName, const FString& Keyword) const;
};
