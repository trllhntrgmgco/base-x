// SBaseXAssetMapper.cpp - "Bring Your Own Assets" UI + Terrain Size selector
// (c) 2026 Keenan Dunham and Troll Hunter Gaming. All Rights Reserved.
// DRO, LOD, and Base-X Game Engine are patent pending (USPTO).

#include "SBaseXAssetMapper.h"
#include "UBaseXWorldBuilder.h"
#include "ABaseXStructureSpawner.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

// ======================================================================
// TERRAIN SIZE PRESETS
// ======================================================================

void SBaseXAssetMapper::BuildTerrainSizes()
{
    TerrainSizes.Empty();
    TerrainSizes.Add({TEXT("Tiny (16x16)"),     16,  0.5f});
    TerrainSizes.Add({TEXT("Small (32x32)"),     32,  0.75f});
    TerrainSizes.Add({TEXT("Medium (64x64)"),    64,  1.0f});
    TerrainSizes.Add({TEXT("Large (128x128)"),  128,  1.5f});
    TerrainSizes.Add({TEXT("Huge (256x256)"),   256,  2.0f});
    TerrainSizes.Add({TEXT("Epic (512x512)"),   512,  3.0f});
}

int32 SBaseXAssetMapper::GetSelectedGridSize() const
{
    if (TerrainSizes.IsValidIndex(SelectedTerrainIndex))
        return TerrainSizes[SelectedTerrainIndex].GridSize;
    return 64;
}

float SBaseXAssetMapper::GetSelectedWorldScale() const
{
    if (TerrainSizes.IsValidIndex(SelectedTerrainIndex))
        return TerrainSizes[SelectedTerrainIndex].WorldScale;
    return 1.0f;
}

// ======================================================================
// CATEGORY DATA (mirrors SBaseXPromptPanel 11 tabs)
// ======================================================================

void SBaseXAssetMapper::BuildCategoryData()
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
    Categories.Empty();
    for (auto& Cat : Cats)
    {
        FCategoryDef Def;
        Def.Label = Cat.L;
        for (auto* K : Cat.KW) Def.Keywords.Add(K);
        Categories.Add(MoveTemp(Def));
    }

    // Build initial mappings from categories
    Mappings.Empty();
    for (const FCategoryDef& Cat : Categories)
    {
        for (const FString& Kw : Cat.Keywords)
        {
            FBaseXAssetMapping M;
            M.Keyword = Kw;
            M.Category = Cat.Label;
            Mappings.Add(MoveTemp(M));
        }
    }
}

// ======================================================================
// CONSTRUCT - Build the full Slate UI
// ======================================================================

void SBaseXAssetMapper::Construct(const FArguments& InArgs)
{
    Builder = InArgs._Builder;
    MeshTable = InArgs._MeshTable;

    BuildCategoryData();
    BuildTerrainSizes();
    LoadMappings();

    // --- Terrain size buttons ---
    TSharedRef<SWrapBox> TerrainRow = SNew(SWrapBox).UseAllottedSize(true);
    for (int32 i = 0; i < TerrainSizes.Num(); i++)
    {
        bool bSelected = (i == SelectedTerrainIndex);
        TerrainRow->AddSlot().Padding(2)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, i]() { return OnTerrainSizeSelected(i); })
            .ContentPadding(FMargin(8, 4))
            .ButtonColorAndOpacity(bSelected
                ? FLinearColor(0.1f, 0.3f, 0.1f)
                : FLinearColor(0.08f, 0.08f, 0.08f))
            [
                SNew(STextBlock)
                .Text(FText::FromString(TerrainSizes[i].Label))
                .Font(FCoreStyle::GetDefaultFontStyle(bSelected ? "Bold" : "Regular", 10))
                .ColorAndOpacity(bSelected
                    ? FLinearColor(0.3f, 1.0f, 0.3f)
                    : FLinearColor(0.6f, 0.6f, 0.6f))
            ]
        ];
    }

    // --- Category filter tabs ---
    TSharedRef<SWrapBox> CatRow = SNew(SWrapBox).UseAllottedSize(true);
    CatRow->AddSlot().Padding(1)
    [
        SNew(SButton)
        .OnClicked_Lambda([this]() { ActiveCategoryIndex = -1; RebuildKeywordList(); return FReply::Handled(); })
        .ContentPadding(FMargin(6, 2))
        .ButtonColorAndOpacity(FLinearColor(0.15f, 0.12f, 0.06f))
        [
            SNew(STextBlock).Text(FText::FromString(TEXT("All (186)")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
            .ColorAndOpacity(FLinearColor(1.f, 0.9f, 0.4f))
        ]
    ];
    for (int32 i = 0; i < Categories.Num(); i++)
    {
        FString TabLabel = FString::Printf(TEXT("%s (%d)"), *Categories[i].Label, Categories[i].Keywords.Num());
        CatRow->AddSlot().Padding(1)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, i]() { ActiveCategoryIndex = i; RebuildKeywordList(); return FReply::Handled(); })
            .ContentPadding(FMargin(6, 2))
            [
                SNew(STextBlock).Text(FText::FromString(TabLabel))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FLinearColor(0.8f, 0.7f, 0.5f))
            ]
        ];
    }

    // --- Main layout ---
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.08f, 0.96f))
        .Padding(12)
        [
            SNew(SBox)
            .WidthOverride(760)
            .MaxDesiredHeight(800)
            [
                SNew(SScrollBox)

                // Title
                + SScrollBox::Slot().Padding(0, 0, 0, 4)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("\u2588 BASE-X ASSET MAPPER v1.1")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                    .ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.2f))
                ]
                + SScrollBox::Slot().Padding(0, 0, 0, 8)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Assign your own meshes to any keyword. Click [Scan Project] to discover assets.")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
                    .AutoWrapText(true)
                ]

                // ====== TERRAIN SIZE ======
                + SScrollBox::Slot().Padding(0, 4, 0, 2)
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("TERRAIN SIZE (tiles)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                    .ColorAndOpacity(FLinearColor(0.3f, 0.8f, 0.3f))
                ]
                + SScrollBox::Slot().Padding(0, 0, 0, 2)
                [
                    TerrainRow
                ]
                + SScrollBox::Slot().Padding(0, 0, 0, 8)
                [
                    SAssignNew(TerrainInfoText, STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("Grid: %dx%d tiles  |  Scale: %.1fx  |  Total cells: %s"),
                        GetSelectedGridSize(), GetSelectedGridSize(), GetSelectedWorldScale(),
                        *FString::FromInt(GetSelectedGridSize() * GetSelectedGridSize()))))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FLinearColor(0.5f, 0.7f, 0.5f))
                ]

                // ====== ACTION BUTTONS ======
                + SScrollBox::Slot().Padding(0, 0, 0, 8)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .OnClicked(this, &SBaseXAssetMapper::OnScanClicked)
                        .ContentPadding(FMargin(10, 6))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Scan Project")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .OnClicked(this, &SBaseXAssetMapper::OnAutoMatchClicked)
                        .ContentPadding(FMargin(10, 6))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Auto-Match")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .OnClicked(this, &SBaseXAssetMapper::OnApplyClicked)
                        .ContentPadding(FMargin(10, 6))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Apply to World")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                            .ColorAndOpacity(FLinearColor(0.3f, 1.0f, 0.3f))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .OnClicked(this, &SBaseXAssetMapper::OnSaveClicked)
                        .ContentPadding(FMargin(8, 6))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Save")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .OnClicked(this, &SBaseXAssetMapper::OnLoadClicked)
                        .ContentPadding(FMargin(8, 6))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Load")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton)
                        .OnClicked(this, &SBaseXAssetMapper::OnClearAllClicked)
                        .ContentPadding(FMargin(8, 6))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Clear All")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                            .ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.4f))
                        ]
                    ]
                ]

                // ====== SEARCH ======
                + SScrollBox::Slot().Padding(0, 0, 0, 4)
                [
                    SAssignNew(SearchBox, SEditableTextBox)
                    .HintText(FText::FromString(TEXT("Search keywords or assets...")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .OnTextChanged_Lambda([this](const FText& T) { OnSearchChanged(T); })
                ]

                // ====== CATEGORY TABS ======
                + SScrollBox::Slot().Padding(0, 4, 0, 2)
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("KEYWORDS")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f))
                ]
                + SScrollBox::Slot().Padding(0, 0, 0, 4)
                [
                    CatRow
                ]

                // ====== KEYWORD BUTTONS (left) + ASSET BUTTONS (right) ======
                + SScrollBox::Slot().Padding(0, 0, 0, 4)
                [
                    SNew(SHorizontalBox)

                    // Left: keyword list
                    + SHorizontalBox::Slot().FillWidth(0.45f).Padding(0, 0, 4, 0)
                    [
                        SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(0.04f, 0.04f, 0.06f))
                        .Padding(4)
                        [
                            SNew(SBox).MaxDesiredHeight(400)
                            [
                                SAssignNew(KeywordListBox, SScrollBox)
                            ]
                        ]
                    ]

                    // Right: discovered asset list
                    + SHorizontalBox::Slot().FillWidth(0.55f)
                    [
                        SNew(SBorder)
                        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(0.04f, 0.06f, 0.04f))
                        .Padding(4)
                        [
                            SNew(SBox).MaxDesiredHeight(400)
                            [
                                SAssignNew(AssetListBox, SScrollBox)
                            ]
                        ]
                    ]
                ]

                // ====== STATUS ======
                + SScrollBox::Slot().Padding(0, 4, 0, 0)
                [
                    SAssignNew(StatusText, STextBlock)
                    .Text(FText::FromString(TEXT("Click [Scan Project] to discover your assets.")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.6f))
                    .AutoWrapText(true)
                ]
            ]
        ]
    ];

    RebuildKeywordList();
}

// ======================================================================
// SCAN - Find all StaticMesh assets in user's project
// ======================================================================

void SBaseXAssetMapper::ScanProjectAssets()
{
    DiscoveredAssets.Empty();

    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AR = ARM.Get();

    FARFilter Filter;
    Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetList;
    AR.GetAssets(Filter, AssetList);

    for (const FAssetData& AD : AssetList)
    {
        // Skip our own built-in BX_ meshes
        FString Name = AD.AssetName.ToString();
        if (Name.StartsWith(TEXT("SM_BX_"))) continue;

        FBaseXDiscoveredAsset DA;
        DA.AssetName = Name;
        DA.AssetPath = AD.GetSoftObjectPath().ToString();
        DA.Mesh = TSoftObjectPtr<UStaticMesh>(AD.GetSoftObjectPath());

        // Pre-compute keyword matches
        FString NameLower = Name.ToLower();
        for (const FBaseXAssetMapping& M : Mappings)
        {
            float Score = CalculateMatchScore(NameLower, M.Keyword);
            if (Score > 0.5f)
            {
                DA.MatchedKeywords.Add(M.Keyword);
            }
        }

        DiscoveredAssets.Add(MoveTemp(DA));
    }

    // Sort: assets with matches first, then alphabetical
    DiscoveredAssets.Sort([](const FBaseXDiscoveredAsset& A, const FBaseXDiscoveredAsset& B)
    {
        if (A.MatchedKeywords.Num() != B.MatchedKeywords.Num())
            return A.MatchedKeywords.Num() > B.MatchedKeywords.Num();
        return A.AssetName < B.AssetName;
    });

    UpdateStatus();
    RebuildAssetList();
}

// ======================================================================
// AUTO-MATCH - Assign best-matching assets to unassigned keywords
// ======================================================================

void SBaseXAssetMapper::AutoMatchAssets()
{
    if (DiscoveredAssets.Num() == 0)
    {
        ScanProjectAssets();
    }

    int32 Matched = 0;
    for (FBaseXAssetMapping& M : Mappings)
    {
        if (M.bIsUserAssigned) continue; // don't overwrite user choices

        float BestScore = 0.f;
        int32 BestIdx = INDEX_NONE;

        for (int32 i = 0; i < DiscoveredAssets.Num(); i++)
        {
            float Score = CalculateMatchScore(
                DiscoveredAssets[i].AssetName.ToLower(), M.Keyword);
            if (Score > BestScore && Score > 0.6f)
            {
                BestScore = Score;
                BestIdx = i;
            }
        }

        if (BestIdx != INDEX_NONE)
        {
            M.UserMesh = DiscoveredAssets[BestIdx].Mesh;
            M.bIsUserAssigned = true;
            Matched++;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXAssetMapper: Auto-matched %d keywords"), Matched);
    UpdateStatus();
    RebuildKeywordList();
}

// ======================================================================
// APPLY - Write mappings to the DataTable for spawning
// ======================================================================

void SBaseXAssetMapper::ApplyMappingsToDataTable()
{
    if (!MeshTable) return;

    for (const FBaseXAssetMapping& M : Mappings)
    {
        if (!M.bIsUserAssigned) continue;

        FName RowName(*M.Keyword);
        FBaseXStructureMeshRow* Row = MeshTable->FindRow<FBaseXStructureMeshRow>(
            RowName, TEXT("AssetMapper"));

        if (Row)
        {
            Row->Mesh = M.UserMesh;
            Row->ScaleMultiplier = M.ScaleMultiplier;
            Row->PivotOffset = M.PivotOffset;
        }
        else
        {
            // Add new row
            FBaseXStructureMeshRow NewRow;
            NewRow.Mesh = M.UserMesh;
            NewRow.ScaleMultiplier = M.ScaleMultiplier;
            NewRow.PivotOffset = M.PivotOffset;
            MeshTable->AddRow(RowName, NewRow);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXAssetMapper: Applied %d mappings to DataTable"),
        GetUserMappedCount());
    UpdateStatus();
}

// ======================================================================
// PERSISTENCE
// ======================================================================

void SBaseXAssetMapper::SaveMappings()
{
    FString SavePath = FPaths::ProjectSavedDir() / TEXT("BaseX") / TEXT("AssetMappings.ini");
    FConfigFile Cfg;

    FString Section = TEXT("AssetMappings");
    for (const FBaseXAssetMapping& M : Mappings)
    {
        if (!M.bIsUserAssigned) continue;
        FString Value = FString::Printf(TEXT("%s|%.2f|%.1f,%.1f,%.1f"),
            *M.UserMesh.ToString(), M.ScaleMultiplier,
            M.PivotOffset.X, M.PivotOffset.Y, M.PivotOffset.Z);
        Cfg.SetString(*Section, *M.Keyword, *Value);
    }

    // Save terrain size
    Cfg.SetString(TEXT("Terrain"), TEXT("SizeIndex"), *FString::FromInt(SelectedTerrainIndex));

    Cfg.Write(SavePath);
    UE_LOG(LogTemp, Log, TEXT("BaseXAssetMapper: Saved mappings to %s"), *SavePath);
    UpdateStatus();
}

void SBaseXAssetMapper::LoadMappings()
{
    FString SavePath = FPaths::ProjectSavedDir() / TEXT("BaseX") / TEXT("AssetMappings.ini");
    if (!FPaths::FileExists(SavePath)) return;

    FConfigFile Cfg;
    Cfg.Read(SavePath);

    FString Section = TEXT("AssetMappings");
    for (FBaseXAssetMapping& M : Mappings)
    {
        FString Value;
        if (Cfg.GetString(*Section, *M.Keyword, Value))
        {
            TArray<FString> Parts;
            Value.ParseIntoArray(Parts, TEXT("|"));
            if (Parts.Num() >= 1)
            {
                M.UserMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Parts[0]));
                M.bIsUserAssigned = true;
            }
            if (Parts.Num() >= 2)
            {
                M.ScaleMultiplier = FCString::Atof(*Parts[1]);
            }
            if (Parts.Num() >= 3)
            {
                TArray<FString> Offs;
                Parts[2].ParseIntoArray(Offs, TEXT(","));
                if (Offs.Num() == 3)
                {
                    M.PivotOffset = FVector(
                        FCString::Atof(*Offs[0]),
                        FCString::Atof(*Offs[1]),
                        FCString::Atof(*Offs[2]));
                }
            }
        }
    }

    // Load terrain size
    FString SizeIdx;
    if (Cfg.GetString(TEXT("Terrain"), TEXT("SizeIndex"), SizeIdx))
    {
        SelectedTerrainIndex = FCString::Atoi(*SizeIdx);
        SelectedTerrainIndex = FMath::Clamp(SelectedTerrainIndex, 0, TerrainSizes.Num() - 1);
    }

    UE_LOG(LogTemp, Log, TEXT("BaseXAssetMapper: Loaded %d mappings"), GetUserMappedCount());
}

// ======================================================================
// STATS
// ======================================================================

int32 SBaseXAssetMapper::GetUserMappedCount() const
{
    int32 Count = 0;
    for (const FBaseXAssetMapping& M : Mappings)
        if (M.bIsUserAssigned) Count++;
    return Count;
}

int32 SBaseXAssetMapper::GetAutoMatchedCount() const
{
    return GetUserMappedCount(); // same for now
}

int32 SBaseXAssetMapper::GetTotalKeywordCount() const
{
    return Mappings.Num();
}

// ======================================================================
// REBUILD KEYWORD LIST (left panel)
// ======================================================================

void SBaseXAssetMapper::RebuildKeywordList()
{
    if (!KeywordListBox.IsValid()) return;
    KeywordListBox->ClearChildren();

    for (int32 i = 0; i < Mappings.Num(); i++)
    {
        const FBaseXAssetMapping& M = Mappings[i];

        // Category filter
        if (ActiveCategoryIndex >= 0 && ActiveCategoryIndex < Categories.Num())
        {
            if (M.Category != Categories[ActiveCategoryIndex].Label) continue;
        }

        // Search filter
        if (!SearchFilter.IsEmpty())
        {
            if (!M.Keyword.Contains(SearchFilter) && !M.Category.Contains(SearchFilter))
                continue;
        }

        bool bSelected = (i == SelectedKeywordIndex);
        bool bHasUserMesh = M.bIsUserAssigned;

        // Color: green = assigned, orange = selected, dim = default
        FLinearColor BtnColor = bSelected
            ? FLinearColor(0.2f, 0.15f, 0.05f)
            : (bHasUserMesh ? FLinearColor(0.05f, 0.15f, 0.05f) : FLinearColor(0.06f, 0.06f, 0.06f));
        FLinearColor TxtColor = bSelected
            ? FLinearColor(1.0f, 0.8f, 0.3f)
            : (bHasUserMesh ? FLinearColor(0.3f, 1.0f, 0.3f) : FLinearColor(0.5f, 0.5f, 0.5f));

        // Button text: "keyword" or "keyword -> MeshName"
        FString Label = M.Keyword;
        if (bHasUserMesh)
        {
            FString MeshName = FPaths::GetBaseFilename(M.UserMesh.ToString());
            Label = FString::Printf(TEXT("%s -> %s"), *M.Keyword, *MeshName);
        }

        int32 Idx = i;
        KeywordListBox->AddSlot().Padding(1)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.f)
            [
                SNew(SButton)
                .OnClicked_Lambda([this, Idx]() { OnKeywordSelected(Idx); return FReply::Handled(); })
                .ContentPadding(FMargin(6, 3))
                .ButtonColorAndOpacity(BtnColor)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(TxtColor)
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                // Clear button (only if assigned)
                SNew(SButton)
                .Visibility(bHasUserMesh ? EVisibility::Visible : EVisibility::Collapsed)
                .OnClicked_Lambda([this, Idx]() { OnClearKeywordAssignment(Idx); return FReply::Handled(); })
                .ContentPadding(FMargin(4, 3))
                .ButtonColorAndOpacity(FLinearColor(0.15f, 0.05f, 0.05f))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("X")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FLinearColor(1.f, 0.3f, 0.3f))
                ]
            ]
        ];
    }
}

// ======================================================================
// REBUILD ASSET LIST (right panel)
// ======================================================================

void SBaseXAssetMapper::RebuildAssetList()
{
    if (!AssetListBox.IsValid()) return;
    AssetListBox->ClearChildren();

    if (DiscoveredAssets.Num() == 0)
    {
        AssetListBox->AddSlot().Padding(8)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("No assets scanned yet.\nClick [Scan Project] to discover StaticMesh assets.")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
            .ColorAndOpacity(FLinearColor(0.4f, 0.4f, 0.4f))
            .AutoWrapText(true)
        ];
        return;
    }

    // Header with count
    AssetListBox->AddSlot().Padding(2, 2, 2, 4)
    [
        SNew(STextBlock)
        .Text(FText::FromString(FString::Printf(TEXT("Found %d assets in project:"), DiscoveredAssets.Num())))
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
        .ColorAndOpacity(FLinearColor(0.6f, 0.8f, 0.6f))
    ];

    for (int32 i = 0; i < DiscoveredAssets.Num(); i++)
    {
        const FBaseXDiscoveredAsset& DA = DiscoveredAssets[i];

        // Search filter
        if (!SearchFilter.IsEmpty())
        {
            if (!DA.AssetName.Contains(SearchFilter)) continue;
        }

        bool bHasMatches = DA.MatchedKeywords.Num() > 0;
        FLinearColor TxtColor = bHasMatches
            ? FLinearColor(0.8f, 0.9f, 0.5f)
            : FLinearColor(0.6f, 0.6f, 0.6f);

        // Show matches hint
        FString Label = DA.AssetName;
        if (bHasMatches)
        {
            Label += TEXT("  [");
            for (int32 m = 0; m < FMath::Min(3, DA.MatchedKeywords.Num()); m++)
            {
                if (m > 0) Label += TEXT(", ");
                Label += DA.MatchedKeywords[m];
            }
            if (DA.MatchedKeywords.Num() > 3)
                Label += FString::Printf(TEXT(" +%d"), DA.MatchedKeywords.Num() - 3);
            Label += TEXT("]");
        }

        int32 AssetIdx = i;
        AssetListBox->AddSlot().Padding(1)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, AssetIdx]() { OnAssetAssigned(AssetIdx); return FReply::Handled(); })
            .ContentPadding(FMargin(6, 3))
            .ButtonColorAndOpacity(FLinearColor(0.05f, 0.06f, 0.04f))
            .IsEnabled(SelectedKeywordIndex != INDEX_NONE)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Label))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(TxtColor)
            ]
        ];
    }
}

// ======================================================================
// UI CALLBACKS
// ======================================================================

FReply SBaseXAssetMapper::OnScanClicked()
{
    ScanProjectAssets();
    return FReply::Handled();
}

FReply SBaseXAssetMapper::OnAutoMatchClicked()
{
    AutoMatchAssets();
    return FReply::Handled();
}

FReply SBaseXAssetMapper::OnApplyClicked()
{
    ApplyMappingsToDataTable();
    return FReply::Handled();
}

FReply SBaseXAssetMapper::OnSaveClicked()
{
    SaveMappings();
    return FReply::Handled();
}

FReply SBaseXAssetMapper::OnLoadClicked()
{
    LoadMappings();
    RebuildKeywordList();
    return FReply::Handled();
}

FReply SBaseXAssetMapper::OnClearAllClicked()
{
    for (FBaseXAssetMapping& M : Mappings)
    {
        M.UserMesh.Reset();
        M.bIsUserAssigned = false;
        M.ScaleMultiplier = 1.f;
        M.PivotOffset = FVector::ZeroVector;
    }
    SelectedKeywordIndex = INDEX_NONE;
    RebuildKeywordList();
    UpdateStatus();
    return FReply::Handled();
}

FReply SBaseXAssetMapper::OnTerrainSizeSelected(int32 Index)
{
    SelectedTerrainIndex = FMath::Clamp(Index, 0, TerrainSizes.Num() - 1);

    if (TerrainInfoText.IsValid())
    {
        TerrainInfoText->SetText(FText::FromString(FString::Printf(
            TEXT("Grid: %dx%d tiles  |  Scale: %.1fx  |  Total cells: %s"),
            GetSelectedGridSize(), GetSelectedGridSize(), GetSelectedWorldScale(),
            *FString::FromInt(GetSelectedGridSize() * GetSelectedGridSize()))));
    }

    // Rebuild terrain row to update highlight (full reconstruct would be heavy,
    // so just log for now - a future version can invalidate just the row)
    UE_LOG(LogTemp, Log, TEXT("BaseXAssetMapper: Terrain size set to %s (%dx%d)"),
        *TerrainSizes[SelectedTerrainIndex].Label,
        GetSelectedGridSize(), GetSelectedGridSize());

    return FReply::Handled();
}

void SBaseXAssetMapper::OnKeywordSelected(int32 Index)
{
    SelectedKeywordIndex = Index;
    RebuildKeywordList();
    RebuildAssetList();
}

void SBaseXAssetMapper::OnAssetAssigned(int32 AssetIndex)
{
    if (SelectedKeywordIndex == INDEX_NONE) return;
    if (!DiscoveredAssets.IsValidIndex(AssetIndex)) return;
    if (!Mappings.IsValidIndex(SelectedKeywordIndex)) return;

    FBaseXAssetMapping& M = Mappings[SelectedKeywordIndex];
    M.UserMesh = DiscoveredAssets[AssetIndex].Mesh;
    M.bIsUserAssigned = true;

    UE_LOG(LogTemp, Log, TEXT("BaseXAssetMapper: Assigned '%s' -> %s"),
        *M.Keyword, *DiscoveredAssets[AssetIndex].AssetName);

    // Auto-advance to next unassigned keyword
    for (int32 i = SelectedKeywordIndex + 1; i < Mappings.Num(); i++)
    {
        if (!Mappings[i].bIsUserAssigned)
        {
            SelectedKeywordIndex = i;
            break;
        }
    }

    RebuildKeywordList();
    UpdateStatus();
}

void SBaseXAssetMapper::OnClearKeywordAssignment(int32 KeywordIndex)
{
    if (Mappings.IsValidIndex(KeywordIndex))
    {
        Mappings[KeywordIndex].UserMesh.Reset();
        Mappings[KeywordIndex].bIsUserAssigned = false;
        Mappings[KeywordIndex].ScaleMultiplier = 1.f;
        Mappings[KeywordIndex].PivotOffset = FVector::ZeroVector;
    }
    RebuildKeywordList();
    UpdateStatus();
}

void SBaseXAssetMapper::OnSearchChanged(const FText& NewText)
{
    SearchFilter = NewText.ToString();
    RebuildKeywordList();
    RebuildAssetList();
}

// ======================================================================
// STATUS BAR
// ======================================================================

void SBaseXAssetMapper::UpdateStatus()
{
    if (!StatusText.IsValid()) return;

    int32 Mapped = GetUserMappedCount();
    int32 Total = GetTotalKeywordCount();
    int32 Assets = DiscoveredAssets.Num();

    StatusText->SetText(FText::FromString(FString::Printf(
        TEXT("Mapped: %d/%d keywords  |  Project assets: %d  |  Terrain: %s"),
        Mapped, Total, Assets, *TerrainSizes[SelectedTerrainIndex].Label)));
}

// ======================================================================
// NAME MATCHING - fuzzy keyword match scoring
// ======================================================================

float SBaseXAssetMapper::CalculateMatchScore(const FString& AssetName, const FString& Keyword) const
{
    FString KwLower = Keyword.ToLower();
    FString NameLower = AssetName.ToLower();

    // Exact substring match (highest score)
    if (NameLower.Contains(KwLower))
        return 1.0f;

    // Replace underscores with spaces for matching
    FString KwNoUnderscore = KwLower.Replace(TEXT("_"), TEXT(" "));
    FString NameNoUnderscore = NameLower.Replace(TEXT("_"), TEXT(" "));

    if (NameNoUnderscore.Contains(KwNoUnderscore))
        return 0.95f;

    // Check each word in the keyword against the asset name
    TArray<FString> KwParts;
    KwNoUnderscore.ParseIntoArray(KwParts, TEXT(" "));
    int32 PartsMatched = 0;
    for (const FString& Part : KwParts)
    {
        if (Part.Len() >= 3 && NameNoUnderscore.Contains(Part))
            PartsMatched++;
    }
    if (KwParts.Num() > 0 && PartsMatched > 0)
        return 0.5f + 0.4f * ((float)PartsMatched / KwParts.Num());

    return 0.f;
}
