// SBaseXHub.h - Unified tabbed UI hub for Base-X v1.1
// Single panel with tabs: World Prompt | Asset Mapper | Settings
//
// (c) 2026 Keenan Dunham and Troll Hunter Gaming. All Rights Reserved.
// DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ABaseXPromptDemo;
class UBaseXWorldBuilder;
class UDataTable;
class SBaseXAssetMapper;
class SWidgetSwitcher;
class STextBlock;
class SEditableTextBox;
class SButton;
class SWrapBox;

// ======================================================================
// HUB - Tabbed container for all Base-X panels
// ======================================================================

class SBaseXHub : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBaseXHub) {}
        SLATE_ARGUMENT(ABaseXPromptDemo*, Owner)
        SLATE_ARGUMENT(UBaseXWorldBuilder*, Builder)
        SLATE_ARGUMENT(UDataTable*, MeshTable)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // Prompt panel delegation
    void SetProgress(float Pct, const FString& Phase);
    void SetStats(const FString& StatsText);
    void SetParseInfo(const FString& Text);
    void SetGenerating(bool bGen);

    // Access the asset mapper for terrain size at generation time
    TSharedPtr<SBaseXAssetMapper> GetAssetMapper() const { return AssetMapper; }

    // Switch to a specific tab by index
    void SwitchToTab(int32 TabIndex);

private:
    ABaseXPromptDemo* Owner = nullptr;
    UBaseXWorldBuilder* Builder = nullptr;

    // Tab system
    int32 ActiveTabIndex = 0;
    TSharedPtr<SWidgetSwitcher> TabSwitcher;
    TArray<TSharedPtr<SButton>> TabButtons;

    // Sub-panels
    TSharedPtr<SBaseXAssetMapper> AssetMapper;

    // Prompt panel widgets (embedded directly — no separate class needed)
    TSharedPtr<SEditableTextBox> PromptBox;
    TSharedPtr<STextBlock> PhaseText;
    TSharedPtr<STextBlock> StatsText;
    TSharedPtr<STextBlock> ParseText;
    TSharedPtr<SButton> GenerateBtn;
    TSharedPtr<SWrapBox> StructTagBox;
    float CurrentProgress = 0.f;
    int32 StructTabIndex = -1;

    // Tab definitions
    struct FTabDef { FString Label; FLinearColor Color; };
    TArray<FTabDef> Tabs;

    // Build helpers
    TSharedRef<SWidget> BuildPromptTab();
    TSharedRef<SWidget> BuildSettingsTab();
    TSharedRef<SWrapBox> BuildTagRow(const TArray<FString>& Tags, const FLinearColor& Color);

    // Prompt panel callbacks
    FReply OnGenerateClicked();
    FReply OnClearClicked();
    FReply OnPresetClicked(int32 Index);
    void OnTagClicked(const FString& Keyword);
    void RebuildStructTags();

    // Structure subcategory definitions
    struct FStructTab { FString Label; TArray<FString> Keywords; };
    TArray<FStructTab> StructTabs;
    void BuildStructTabData();

    // Tab switching
    void UpdateTabHighlights();
};
