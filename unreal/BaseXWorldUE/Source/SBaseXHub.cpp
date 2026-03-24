// SBaseXHub.cpp - Unified tabbed UI hub for Base-X v1.1
// (c) 2026 Keenan Dunham and Troll Hunter Gaming. All Rights Reserved.
// DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

#include "SBaseXHub.h"
#include "ABaseXPromptDemo.h"
#include "SBaseXAssetMapper.h"
#include "UBaseXWorldBuilder.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

// ======================================================================
// STRUCT TABS (same data as SBaseXPromptPanel)
// ======================================================================

void SBaseXHub::BuildStructTabData()
{
    struct C { const TCHAR* L; std::initializer_list<const TCHAR*> KW; };
    C Cats[] = {
        {TEXT("Landmark"), {TEXT("spire"),TEXT("fortress"),TEXT("cathedral"),TEXT("castle"),TEXT("tower"),TEXT("pyramid"),
            TEXT("temple"),TEXT("palace"),TEXT("citadel"),TEXT("lighthouse"),TEXT("colosseum"),TEXT("amphitheater"),
            TEXT("keep"),TEXT("pagoda"),TEXT("ziggurat"),TEXT("minaret"),TEXT("aqueduct"),TEXT("monastery"),TEXT("arena"),
            TEXT("mansion"),TEXT("villa"),TEXT("warehouse"),TEXT("granary"),TEXT("barn"),TEXT("stable"),TEXT("bell_tower")}},
        {TEXT("Buildings"), {TEXT("hut"),TEXT("tavern"),TEXT("cabin"),TEXT("chapel"),TEXT("forge"),TEXT("marketplace"),
            TEXT("windmill"),TEXT("cottage"),TEXT("farmhouse"),TEXT("shack"),TEXT("shed"),TEXT("workshop"),TEXT("smithy"),
            TEXT("market_stall"),TEXT("tent"),TEXT("lean_to"),TEXT("outhouse"),TEXT("guardhouse"),TEXT("barracks"),
            TEXT("inn"),TEXT("bakery"),TEXT("apothecary")}},
        {TEXT("Walls/Paths"), {TEXT("bridge"),TEXT("dock"),TEXT("gate"),TEXT("wall"),TEXT("arch"),TEXT("archway"),TEXT("torii"),
            TEXT("pier"),TEXT("jetty"),TEXT("wharf"),TEXT("boathouse"),TEXT("platform"),TEXT("stairway"),TEXT("rampart"),
            TEXT("gatehouse"),TEXT("drawbridge"),TEXT("colonnade"),TEXT("terrace"),TEXT("fence"),TEXT("palisade"),
            TEXT("parapet"),TEXT("battlement"),TEXT("portcullis")}},
        {TEXT("Low/Dome"), {TEXT("altar"),TEXT("shrine"),TEXT("dolmen"),TEXT("barrow"),TEXT("dome"),TEXT("cupola"),
            TEXT("well"),TEXT("fountain"),TEXT("bird_bath"),TEXT("sundial"),TEXT("gazebo"),TEXT("pergola")}},
        {TEXT("Monuments"), {TEXT("throne"),TEXT("statue"),TEXT("obelisk"),TEXT("monolith"),TEXT("pillar"),TEXT("column"),
            TEXT("runestone"),TEXT("ward_stone"),TEXT("memorial"),TEXT("cenotaph"),TEXT("cross"),TEXT("gravestone"),
            TEXT("headstone"),TEXT("sarcophagus"),TEXT("mausoleum"),TEXT("tomb")}},
        {TEXT("Fantasy"), {TEXT("portal"),TEXT("crystal_orb"),TEXT("enchanted_well"),TEXT("fairy_ring"),
            TEXT("summoning_circle"),TEXT("rift"),TEXT("crystal_spire"),TEXT("soul_lantern")}},
        {TEXT("Props"), {TEXT("torch"),TEXT("lantern"),TEXT("brazier"),TEXT("campfire"),TEXT("bonfire"),TEXT("lamp_post"),
            TEXT("banner"),TEXT("flag"),TEXT("signpost"),TEXT("cart"),TEXT("wagon"),TEXT("barrel"),TEXT("crate"),TEXT("chest"),
            TEXT("anchor"),TEXT("cannon"),TEXT("catapult"),TEXT("ballista")}},
        {TEXT("Ruins"), {TEXT("ruins"),TEXT("rubble"),TEXT("crumbled_wall"),TEXT("broken_pillar"),TEXT("ruin_arch"),
            TEXT("collapsed_tower"),TEXT("sunken_ship"),TEXT("shipwreck")}},
        {TEXT("Trees"), {TEXT("tree"),TEXT("pine"),TEXT("spruce"),TEXT("cedar"),TEXT("fir"),TEXT("redwood"),TEXT("cypress"),
            TEXT("oak"),TEXT("willow"),TEXT("birch"),TEXT("maple"),TEXT("palm"),TEXT("acacia"),TEXT("baobab"),TEXT("mangrove"),
            TEXT("elm"),TEXT("banyan"),TEXT("ash_tree"),TEXT("magnolia")}},
        {TEXT("Vegetation"), {TEXT("shrub"),TEXT("bush"),TEXT("hedge"),TEXT("fern"),TEXT("grass_tuft"),TEXT("flower"),
            TEXT("vine"),TEXT("cattail"),TEXT("lily_pad"),TEXT("moss_patch"),TEXT("root"),TEXT("mushroom_cluster"),
            TEXT("flower_bed"),TEXT("garden"),TEXT("topiary")}},
        {TEXT("Geological"), {TEXT("rock"),TEXT("boulder"),TEXT("stump"),TEXT("log"),TEXT("outcrop"),TEXT("cliff_face"),
            TEXT("cave_mouth"),TEXT("stalagmite"),TEXT("stalactite"),TEXT("crystal_cluster"),TEXT("geode"),
            TEXT("hot_spring"),TEXT("geyser"),TEXT("lava_pool"),TEXT("volcanic_vent"),TEXT("coral_formation"),
            TEXT("sea_stack"),TEXT("rock_arch"),TEXT("tidepool")}},
    };
    StructTabs.Empty();
    for (auto& Cat : Cats)
    {
        FStructTab Tab;
        Tab.Label = Cat.L;
        for (auto* K : Cat.KW) Tab.Keywords.Add(K);
        StructTabs.Add(MoveTemp(Tab));
    }
}

// ======================================================================
// CONSTRUCT
// ======================================================================

void SBaseXHub::Construct(const FArguments& InArgs)
{
    Owner = InArgs._Owner;
    Builder = InArgs._Builder;

    BuildStructTabData();

    // Define tabs
    Tabs.Add({TEXT("World Prompt"), FLinearColor(0.2f, 1.0f, 1.0f)});
    Tabs.Add({TEXT("Asset Mapper"), FLinearColor(1.0f, 0.6f, 0.2f)});
    Tabs.Add({TEXT("Settings"),     FLinearColor(0.7f, 0.7f, 0.7f)});

    // Build Asset Mapper sub-panel
    AssetMapper = SNew(SBaseXAssetMapper)
        .Builder(Builder)
        .MeshTable(InArgs._MeshTable);

    // Tab bar
    TSharedRef<SHorizontalBox> TabBar = SNew(SHorizontalBox);
    for (int32 i = 0; i < Tabs.Num(); i++)
    {
        TSharedPtr<SButton> Btn;
        TabBar->AddSlot().AutoWidth().Padding(0, 0, 2, 0)
        [
            SAssignNew(Btn, SButton)
            .OnClicked_Lambda([this, i]() { SwitchToTab(i); return FReply::Handled(); })
            .ContentPadding(FMargin(14, 6))
            .ButtonColorAndOpacity(i == 0
                ? FLinearColor(0.08f, 0.12f, 0.18f)
                : FLinearColor(0.04f, 0.04f, 0.06f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(Tabs[i].Label))
                .Font(FCoreStyle::GetDefaultFontStyle(i == 0 ? "Bold" : "Regular", 12))
                .ColorAndOpacity(Tabs[i].Color)
            ]
        ];
        TabButtons.Add(Btn);
    }

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0.02f, 0.025f, 0.06f, 0.97f))
        .Padding(0)
        [
            SNew(SVerticalBox)

            // Tab bar
            + SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
            [
                TabBar
            ]

            // Tab content
            + SVerticalBox::Slot().FillHeight(1.f).Padding(0)
            [
                SAssignNew(TabSwitcher, SWidgetSwitcher)
                .WidgetIndex(0)

                // Tab 0: World Prompt
                + SWidgetSwitcher::Slot()
                [
                    BuildPromptTab()
                ]

                // Tab 1: Asset Mapper
                + SWidgetSwitcher::Slot()
                [
                    AssetMapper.ToSharedRef()
                ]

                // Tab 2: Settings
                + SWidgetSwitcher::Slot()
                [
                    BuildSettingsTab()
                ]
            ]
        ]
    ];
}

// ======================================================================
// TAB SWITCHING
// ======================================================================

void SBaseXHub::SwitchToTab(int32 TabIndex)
{
    ActiveTabIndex = FMath::Clamp(TabIndex, 0, Tabs.Num() - 1);
    if (TabSwitcher.IsValid())
        TabSwitcher->SetActiveWidgetIndex(ActiveTabIndex);
    UpdateTabHighlights();
}

void SBaseXHub::UpdateTabHighlights()
{
    for (int32 i = 0; i < TabButtons.Num(); i++)
    {
        if (TabButtons[i].IsValid())
        {
            TabButtons[i]->SetColorAndOpacity(
                i == ActiveTabIndex
                    ? FLinearColor::White
                    : FLinearColor(0.6f, 0.6f, 0.6f));
        }
    }
}

// ======================================================================
// PROMPT TAB
// ======================================================================

TSharedRef<SWidget> SBaseXHub::BuildPromptTab()
{
    // Preset labels (v1.0 + v1.1)
    static const char* PresetLabels[] = {
        "Volcano", "Frozen", "Enchanted", "Desert",
        "Floating", "Necropolis", "Steampunk", "Crystal",
        "Bamboo", "Badlands", "Tundra", "Savanna",
        "Fjord", "Lava Canyon", "Swamp", "Highland",
        "Coral Reef", "Battlefield", "Monastery", "Dark Elf",
        "Frontier", "Ice Cave", "Mesa", "Haunted"
    };

    int32 NumPresets = Owner ? Owner->GetPresetCount() : 24;

    TSharedRef<SWrapBox> PresetRow = SNew(SWrapBox).UseAllottedSize(true);
    for (int32 i = 0; i < NumPresets; i++)
    {
        PresetRow->AddSlot().Padding(2)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, i]() { return OnPresetClicked(i); })
            .ContentPadding(FMargin(8, 4))
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString(PresetLabels[i])))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            ]
        ];
    }

    // Keyword lists
    TArray<FString> TerrainKW, BiomeKW, WeatherKW;
    if (Builder)
    {
        TerrainKW = Builder->GetTerrainKeywords();
        BiomeKW   = Builder->GetBiomeKeywords();
        WeatherKW = Builder->GetWeatherKeywords();
    }
    TerrainKW.Sort();
    BiomeKW.Sort();

    // Structure tab buttons
    TSharedRef<SWrapBox> StructTabRow = SNew(SWrapBox).UseAllottedSize(true);
    StructTabRow->AddSlot().Padding(1)
    [
        SNew(SButton)
        .OnClicked_Lambda([this]() { StructTabIndex = -1; RebuildStructTags(); return FReply::Handled(); })
        .ContentPadding(FMargin(6, 2))
        .ButtonColorAndOpacity(FLinearColor(0.2f, 0.18f, 0.1f))
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("All")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            .ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.4f))
        ]
    ];
    for (int32 ti = 0; ti < StructTabs.Num(); ti++)
    {
        StructTabRow->AddSlot().Padding(1)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, ti]() { StructTabIndex = ti; RebuildStructTags(); return FReply::Handled(); })
            .ContentPadding(FMargin(6, 2))
            [
                SNew(STextBlock).Text(FText::FromString(StructTabs[ti].Label))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FLinearColor(0.8f, 0.7f, 0.5f))
            ]
        ];
    }

    return SNew(SBox)
        .Padding(12)
        .WidthOverride(700)
        .MaxDesiredHeight(700)
        [
            SNew(SScrollBox)

            // Title
            + SScrollBox::Slot().Padding(0, 0, 0, 6)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("\u2588 BASE-X WORLD PROMPT ENGINE")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                .ColorAndOpacity(FLinearColor(0.2f, 1.0f, 1.0f))
            ]

            // Prompt input
            + SScrollBox::Slot().Padding(0, 0, 0, 4)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Describe your world (or click tags below):")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 6)
            [
                SAssignNew(PromptBox, SEditableTextBox)
                .HintText(FText::FromString(TEXT("e.g. volcanic island obsidian lava crystal spire ash dawn")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
            ]

            // TERRAIN TAGS
            + SScrollBox::Slot().Padding(0, 4, 0, 2)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("TERRAIN")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                .ColorAndOpacity(FLinearColor(0.3f, 0.8f, 0.3f))
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 4)
            [
                BuildTagRow(TerrainKW, FLinearColor(0.3f, 0.8f, 0.3f))
            ]

            // BIOME TAGS
            + SScrollBox::Slot().Padding(0, 4, 0, 2)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("BIOME")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                .ColorAndOpacity(FLinearColor(0.3f, 0.6f, 1.0f))
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 4)
            [
                BuildTagRow(BiomeKW, FLinearColor(0.3f, 0.6f, 1.0f))
            ]

            // STRUCTURE TABS + TAGS
            + SScrollBox::Slot().Padding(0, 4, 0, 2)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("STRUCTURES")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                .ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f))
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 2)
            [
                StructTabRow
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 4)
            [
                SAssignNew(StructTagBox, SWrapBox).UseAllottedSize(true)
            ]

            // WEATHER TAGS
            + SScrollBox::Slot().Padding(0, 4, 0, 2)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("WEATHER")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                .ColorAndOpacity(FLinearColor(0.8f, 0.4f, 0.8f))
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 6)
            [
                BuildTagRow(WeatherKW, FLinearColor(0.8f, 0.4f, 0.8f))
            ]

            // PRESETS
            + SScrollBox::Slot().Padding(0, 0, 0, 2)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("PRESETS")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 6)
            [
                PresetRow
            ]

            // Generate + Clear
            + SScrollBox::Slot().Padding(0, 0, 0, 8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0, 0, 4, 0)
                [
                    SAssignNew(GenerateBtn, SButton)
                    .OnClicked(this, &SBaseXHub::OnGenerateClicked)
                    .HAlign(HAlign_Center)
                    .ContentPadding(FMargin(16, 8))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Generate World")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .OnClicked(this, &SBaseXHub::OnClearClicked)
                    .ContentPadding(FMargin(12, 8))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Clear")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                    ]
                ]
            ]

            // Progress
            + SScrollBox::Slot().Padding(0, 0, 0, 2)
            [
                SNew(SProgressBar)
                .Percent_Lambda([this]() { return TOptional<float>(CurrentProgress); })
            ]
            + SScrollBox::Slot().Padding(0, 0, 0, 6)
            [
                SAssignNew(PhaseText, STextBlock)
                .Text(FText::FromString(TEXT("Ready")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.5f, 0.8f, 0.5f))
            ]

            // Parse results + stats
            + SScrollBox::Slot().Padding(0, 0, 0, 2)
            [
                SAssignNew(ParseText, STextBlock)
                .Text(FText::GetEmpty())
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.7f))
                .AutoWrapText(true)
            ]
            + SScrollBox::Slot()
            [
                SAssignNew(StatsText, STextBlock)
                .Text(FText::GetEmpty())
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.5f))
                .AutoWrapText(true)
            ]
        ];

    RebuildStructTags();

    // Note: we return the SBox above via the return statement at the top
    // The RebuildStructTags call needs to happen after StructTagBox is valid
}

// ======================================================================
// SETTINGS TAB
// ======================================================================

TSharedRef<SWidget> SBaseXHub::BuildSettingsTab()
{
    return SNew(SBox)
        .Padding(12)
        .WidthOverride(700)
        .MaxDesiredHeight(700)
        [
            SNew(SScrollBox)

            + SScrollBox::Slot().Padding(0, 0, 0, 8)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("\u2588 BASE-X SETTINGS")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
            ]

            + SScrollBox::Slot().Padding(0, 0, 0, 4)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Engine Information")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
            ]

            + SScrollBox::Slot().Padding(0, 0, 0, 12)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(
                    TEXT("Base-X World Generation Engine v1.1.0\n")
                    TEXT("(c) 2026 Keenan Dunham & Troll Hunter Gaming\n")
                    TEXT("DRO, LOD, and Base-X Game Engine are patent pending (USPTO)\n\n")
                    TEXT("Presets: %d\n")
                    TEXT("Structure Keywords: %d\n")
                    TEXT("Terrain Keywords: %d\n")
                    TEXT("Biome Keywords: %d\n")
                    TEXT("Weather Keywords: %d"),
                    Owner ? Owner->GetPresetCount() : 0,
                    Builder ? Builder->GetStructureKeywords().Num() : 0,
                    Builder ? Builder->GetTerrainKeywords().Num() : 0,
                    Builder ? Builder->GetBiomeKeywords().Num() : 0,
                    Builder ? Builder->GetWeatherKeywords().Num() : 0)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                .AutoWrapText(true)
            ]
        ];
}

// ======================================================================
// TAG HELPERS (same logic as SBaseXPromptPanel)
// ======================================================================

TSharedRef<SWrapBox> SBaseXHub::BuildTagRow(const TArray<FString>& Tags, const FLinearColor& Color)
{
    TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true);
    for (const FString& Kw : Tags)
    {
        FString KwCopy = Kw;
        Row->AddSlot().Padding(1)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, KwCopy]() { OnTagClicked(KwCopy); return FReply::Handled(); })
            .ContentPadding(FMargin(5, 1))
            .ButtonColorAndOpacity(FLinearColor(Color.R * 0.15f, Color.G * 0.15f, Color.B * 0.15f, 0.8f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(Kw))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FSlateColor(Color))
            ]
        ];
    }
    return Row;
}

void SBaseXHub::OnTagClicked(const FString& Keyword)
{
    if (!PromptBox.IsValid()) return;

    FString Current = PromptBox->GetText().ToString();
    FString Lower = Current.ToLower();
    FString KwLower = Keyword.ToLower();

    int32 Pos = Lower.Find(KwLower);
    if (Pos != INDEX_NONE)
    {
        int32 End = Pos + Keyword.Len();
        if (End < Current.Len() && Current[End] == TEXT(' ')) End++;
        else if (Pos > 0 && Current[Pos - 1] == TEXT(' ')) Pos--;
        Current.RemoveAt(Pos, End - Pos);
    }
    else
    {
        if (!Current.IsEmpty() && !Current.EndsWith(TEXT(" ")))
            Current += TEXT(" ");
        Current += Keyword;
    }
    PromptBox->SetText(FText::FromString(Current));
}

void SBaseXHub::RebuildStructTags()
{
    if (!StructTagBox.IsValid()) return;
    StructTagBox->ClearChildren();

    TArray<FString> Tags;
    if (StructTabIndex < 0)
    {
        if (Builder)
            Tags = Builder->GetStructureKeywords();
    }
    else if (StructTabIndex < StructTabs.Num())
    {
        Tags = StructTabs[StructTabIndex].Keywords;
    }

    FLinearColor Color(0.9f, 0.7f, 0.3f);
    for (const FString& Kw : Tags)
    {
        FString KwCopy = Kw;
        StructTagBox->AddSlot().Padding(1)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, KwCopy]() { OnTagClicked(KwCopy); return FReply::Handled(); })
            .ContentPadding(FMargin(5, 1))
            .ButtonColorAndOpacity(FLinearColor(Color.R * 0.15f, Color.G * 0.15f, Color.B * 0.15f, 0.8f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(KwCopy))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FSlateColor(Color))
            ]
        ];
    }
}

// ======================================================================
// PROMPT CALLBACKS
// ======================================================================

FReply SBaseXHub::OnGenerateClicked()
{
    if (Owner && PromptBox.IsValid())
    {
        Owner->RequestGenerate(PromptBox->GetText().ToString());
    }
    return FReply::Handled();
}

FReply SBaseXHub::OnClearClicked()
{
    if (Owner) Owner->RequestClear();
    SetProgress(0.f, TEXT("Ready"));
    SetStats(FString());
    SetParseInfo(FString());
    return FReply::Handled();
}

FReply SBaseXHub::OnPresetClicked(int32 Index)
{
    if (Owner && PromptBox.IsValid())
    {
        PromptBox->SetText(FText::FromString(Owner->GetPresetPrompt(Index)));
    }
    return FReply::Handled();
}

// ======================================================================
// DELEGATION (called by actor Tick)
// ======================================================================

void SBaseXHub::SetProgress(float Pct, const FString& Phase)
{
    CurrentProgress = Pct / 100.f;
    if (PhaseText.IsValid())
        PhaseText->SetText(FText::FromString(Phase));
}

void SBaseXHub::SetStats(const FString& Text)
{
    if (StatsText.IsValid())
        StatsText->SetText(FText::FromString(Text));
}

void SBaseXHub::SetParseInfo(const FString& Text)
{
    if (ParseText.IsValid())
        ParseText->SetText(FText::FromString(Text));
}

void SBaseXHub::SetGenerating(bool bGen)
{
    if (GenerateBtn.IsValid())
        GenerateBtn->SetEnabled(!bGen);
}
