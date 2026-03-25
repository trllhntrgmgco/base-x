// BaseXMeshFactory.cpp — Procedural mesh generation for Base-X structure keywords.
// Generates UStaticMesh .uassets (editor) or runtime meshes (game) for each
// structure type so the DataTable can reference proper 3D geometry.

#include "BaseXMeshFactory.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#endif

// ══════════════════════════════════════════════════════════════════════
// MESH GEOMETRY HELPERS
// ══════════════════════════════════════════════════════════════════════

#if WITH_EDITOR

// Build a cylinder / trunk mesh
static void BuildCylinder(FMeshDescriptionBuilder& B, FPolygonGroupID PG,
                           float Radius, float Height, int32 Segments = 12)
{
    TArray<FVertexInstanceID> Verts;
    float HalfH = Height * 0.5f;

    // Bottom + top center
    FVertexID BotCenter = B.AppendVertex(FVector(0, 0, 0));
    FVertexID TopCenter = B.AppendVertex(FVector(0, 0, Height));

    TArray<FVertexID> BotRing, TopRing;
    for (int32 i = 0; i < Segments; i++)
    {
        float A = (float)i / Segments * 2.f * PI;
        float X = FMath::Cos(A) * Radius;
        float Y = FMath::Sin(A) * Radius;
        BotRing.Add(B.AppendVertex(FVector(X, Y, 0)));
        TopRing.Add(B.AppendVertex(FVector(X, Y, Height)));
    }

    // Helper: create a triangle from 3 vertices
    auto MakeTri = [&](FVertexID V0, FVertexID V1, FVertexID V2, FVector Normal)
    {
        FVertexInstanceID I0 = B.AppendInstance(V0);
        B.SetInstanceNormal(I0, Normal);
        B.SetInstanceUV(I0, FVector2D(0, 0));
        B.SetInstanceColor(I0, FVector4f(1, 1, 1, 1));
        FVertexInstanceID I1 = B.AppendInstance(V1);
        B.SetInstanceNormal(I1, Normal);
        B.SetInstanceUV(I1, FVector2D(1, 0));
        B.SetInstanceColor(I1, FVector4f(1, 1, 1, 1));
        FVertexInstanceID I2 = B.AppendInstance(V2);
        B.SetInstanceNormal(I2, Normal);
        B.SetInstanceUV(I2, FVector2D(0.5f, 1));
        B.SetInstanceColor(I2, FVector4f(1, 1, 1, 1));
        TArray<FVertexInstanceID> Tri = { I0, I1, I2 };
        B.AppendTriangle(Tri[0], Tri[1], Tri[2], PG);
    };

    // Side faces
    for (int32 i = 0; i < Segments; i++)
    {
        int32 Next = (i + 1) % Segments;
        float A0 = (float)i / Segments * 2.f * PI;
        float A1 = (float)Next / Segments * 2.f * PI;
        FVector N0(FMath::Cos(A0), FMath::Sin(A0), 0);
        FVector N1(FMath::Cos(A1), FMath::Sin(A1), 0);
        FVector Avg = ((N0 + N1) * 0.5f).GetSafeNormal();
        MakeTri(BotRing[i], TopRing[i], TopRing[Next], Avg);
        MakeTri(BotRing[i], TopRing[Next], BotRing[Next], Avg);
    }

    // Bottom cap
    for (int32 i = 0; i < Segments; i++)
        MakeTri(BotCenter, BotRing[(i + 1) % Segments], BotRing[i], FVector(0, 0, -1));

    // Top cap
    for (int32 i = 0; i < Segments; i++)
        MakeTri(TopCenter, TopRing[i], TopRing[(i + 1) % Segments], FVector(0, 0, 1));
}

// Build a sphere / canopy
static void BuildSphere(FMeshDescriptionBuilder& B, FPolygonGroupID PG,
                         float Radius, FVector Center, int32 Rings = 8, int32 Segments = 12)
{
    auto MakeTri = [&](FVector P0, FVector P1, FVector P2)
    {
        FVector N = FVector::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
        FVertexID V0 = B.AppendVertex(P0);
        FVertexID V1 = B.AppendVertex(P1);
        FVertexID V2 = B.AppendVertex(P2);
        FVertexInstanceID I0 = B.AppendInstance(V0);
        B.SetInstanceNormal(I0, N); B.SetInstanceUV(I0, FVector2D(0, 0)); B.SetInstanceColor(I0, FVector4f(1,1,1,1));
        FVertexInstanceID I1 = B.AppendInstance(V1);
        B.SetInstanceNormal(I1, N); B.SetInstanceUV(I1, FVector2D(1, 0)); B.SetInstanceColor(I1, FVector4f(1,1,1,1));
        FVertexInstanceID I2 = B.AppendInstance(V2);
        B.SetInstanceNormal(I2, N); B.SetInstanceUV(I2, FVector2D(0.5f, 1)); B.SetInstanceColor(I2, FVector4f(1,1,1,1));
        B.AppendTriangle(I0, I1, I2, PG);
    };

    for (int32 r = 0; r < Rings; r++)
    {
        float Theta0 = (float)r / Rings * PI;
        float Theta1 = (float)(r + 1) / Rings * PI;
        for (int32 s = 0; s < Segments; s++)
        {
            float Phi0 = (float)s / Segments * 2.f * PI;
            float Phi1 = (float)(s + 1) / Segments * 2.f * PI;

            FVector P00 = Center + Radius * FVector(FMath::Sin(Theta0)*FMath::Cos(Phi0), FMath::Sin(Theta0)*FMath::Sin(Phi0), FMath::Cos(Theta0));
            FVector P01 = Center + Radius * FVector(FMath::Sin(Theta0)*FMath::Cos(Phi1), FMath::Sin(Theta0)*FMath::Sin(Phi1), FMath::Cos(Theta0));
            FVector P10 = Center + Radius * FVector(FMath::Sin(Theta1)*FMath::Cos(Phi0), FMath::Sin(Theta1)*FMath::Sin(Phi0), FMath::Cos(Theta1));
            FVector P11 = Center + Radius * FVector(FMath::Sin(Theta1)*FMath::Cos(Phi1), FMath::Sin(Theta1)*FMath::Sin(Phi1), FMath::Cos(Theta1));

            MakeTri(P00, P10, P11);
            MakeTri(P00, P11, P01);
        }
    }
}

// Build a box
static void BuildBox(FMeshDescriptionBuilder& B, FPolygonGroupID PG,
                      FVector Size, FVector Offset = FVector::ZeroVector)
{
    FVector H = Size * 0.5f;
    FVector Lo = Offset - H;
    FVector Hi = Offset + H;

    auto MakeQuad = [&](FVector P0, FVector P1, FVector P2, FVector P3, FVector N)
    {
        auto MakeInst = [&](FVector P) -> FVertexInstanceID
        {
            FVertexID V = B.AppendVertex(P);
            FVertexInstanceID I = B.AppendInstance(V);
            B.SetInstanceNormal(I, N);
            B.SetInstanceUV(I, FVector2D(0, 0));
            B.SetInstanceColor(I, FVector4f(1, 1, 1, 1));
            return I;
        };
        FVertexInstanceID I0 = MakeInst(P0), I1 = MakeInst(P1), I2 = MakeInst(P2), I3 = MakeInst(P3);
        B.AppendTriangle(I0, I1, I2, PG);
        B.AppendTriangle(I0, I2, I3, PG);
    };

    // 6 faces
    MakeQuad(FVector(Lo.X,Lo.Y,Hi.Z), FVector(Hi.X,Lo.Y,Hi.Z), FVector(Hi.X,Hi.Y,Hi.Z), FVector(Lo.X,Hi.Y,Hi.Z), FVector(0,0,1));   // top
    MakeQuad(FVector(Lo.X,Hi.Y,Lo.Z), FVector(Hi.X,Hi.Y,Lo.Z), FVector(Hi.X,Lo.Y,Lo.Z), FVector(Lo.X,Lo.Y,Lo.Z), FVector(0,0,-1));  // bottom
    MakeQuad(FVector(Lo.X,Lo.Y,Lo.Z), FVector(Hi.X,Lo.Y,Lo.Z), FVector(Hi.X,Lo.Y,Hi.Z), FVector(Lo.X,Lo.Y,Hi.Z), FVector(0,-1,0));  // front
    MakeQuad(FVector(Hi.X,Hi.Y,Lo.Z), FVector(Lo.X,Hi.Y,Lo.Z), FVector(Lo.X,Hi.Y,Hi.Z), FVector(Hi.X,Hi.Y,Hi.Z), FVector(0,1,0));   // back
    MakeQuad(FVector(Lo.X,Hi.Y,Lo.Z), FVector(Lo.X,Lo.Y,Lo.Z), FVector(Lo.X,Lo.Y,Hi.Z), FVector(Lo.X,Hi.Y,Hi.Z), FVector(-1,0,0));  // left
    MakeQuad(FVector(Hi.X,Lo.Y,Lo.Z), FVector(Hi.X,Hi.Y,Lo.Z), FVector(Hi.X,Hi.Y,Hi.Z), FVector(Hi.X,Lo.Y,Hi.Z), FVector(1,0,0));   // right
}

// Build a cone / pyramid
static void BuildCone(FMeshDescriptionBuilder& B, FPolygonGroupID PG,
                       float Radius, float Height, int32 Segments = 8)
{
    FVertexID Apex = B.AppendVertex(FVector(0, 0, Height));
    FVertexID Center = B.AppendVertex(FVector(0, 0, 0));

    TArray<FVertexID> Ring;
    for (int32 i = 0; i < Segments; i++)
    {
        float A = (float)i / Segments * 2.f * PI;
        Ring.Add(B.AppendVertex(FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 0)));
    }

    auto MakeTri = [&](FVertexID V0, FVertexID V1, FVertexID V2, FVector Normal)
    {
        FVertexInstanceID I0 = B.AppendInstance(V0);
        B.SetInstanceNormal(I0, Normal); B.SetInstanceUV(I0, FVector2D(0,0)); B.SetInstanceColor(I0, FVector4f(1,1,1,1));
        FVertexInstanceID I1 = B.AppendInstance(V1);
        B.SetInstanceNormal(I1, Normal); B.SetInstanceUV(I1, FVector2D(1,0)); B.SetInstanceColor(I1, FVector4f(1,1,1,1));
        FVertexInstanceID I2 = B.AppendInstance(V2);
        B.SetInstanceNormal(I2, Normal); B.SetInstanceUV(I2, FVector2D(0.5f,1)); B.SetInstanceColor(I2, FVector4f(1,1,1,1));
        B.AppendTriangle(I0, I1, I2, PG);
    };

    for (int32 i = 0; i < Segments; i++)
    {
        int32 Next = (i + 1) % Segments;
        FVector N = FVector::CrossProduct(
            FVector(0, 0, Height) - B.GetPosition(Ring[i]),
            B.GetPosition(Ring[Next]) - B.GetPosition(Ring[i])).GetSafeNormal();
        MakeTri(Ring[i], Apex, Ring[Next], N);
        MakeTri(Center, Ring[Next], Ring[i], FVector(0, 0, -1));
    }
}

// ══════════════════════════════════════════════════════════════════════
// COMPOSITE STRUCTURES — multi-part meshes for realistic visuals
// ══════════════════════════════════════════════════════════════════════

struct FMeshRecipe
{
    FString Keyword;
    TFunction<void(FMeshDescriptionBuilder&, FPolygonGroupID)> BuildFunc;
};

static TArray<FMeshRecipe> GetAllRecipes()
{
    TArray<FMeshRecipe> R;

    // ── Conifer tree (trunk cylinder + cone canopy) ──
    auto ConiferBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 8.f, 120.f, 8);
        BuildCone(B, PG, 50.f, 160.f, 10);
    };
    for (auto& K : { TEXT("pine"), TEXT("spruce"), TEXT("cedar"), TEXT("fir"), TEXT("redwood"), TEXT("cypress") })
        R.Add({K, ConiferBuild});

    // ── Broadleaf tree (trunk + sphere canopy) ──
    auto BroadleafBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 10.f, 100.f, 8);
        BuildSphere(B, PG, 60.f, FVector(0, 0, 140));
    };
    for (auto& K : { TEXT("oak"), TEXT("willow"), TEXT("birch"), TEXT("maple"),
                     TEXT("elm"), TEXT("banyan"), TEXT("ash_tree"), TEXT("magnolia"), TEXT("tree") })
        R.Add({K, BroadleafBuild});

    // ── Tropical tree (thin trunk + elongated top) ──
    auto TropicalBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 6.f, 150.f, 6);
        BuildSphere(B, PG, 40.f, FVector(0, 0, 170));
    };
    for (auto& K : { TEXT("palm"), TEXT("acacia"), TEXT("baobab"), TEXT("mangrove") })
        R.Add({K, TropicalBuild});

    // ── Stump / log ──
    R.Add({TEXT("stump"), [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 15.f, 20.f, 8);
    }});
    R.Add({TEXT("log"), [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 10.f, 60.f, 8);
    }});

    // ── Small bush/shrub (small sphere on ground) ──
    auto SmallVegBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildSphere(B, PG, 25.f, FVector(0, 0, 20));
    };
    for (auto& K : { TEXT("shrub"), TEXT("bush"), TEXT("hedge"), TEXT("fern"), TEXT("flower"),
                     TEXT("garden"), TEXT("topiary"), TEXT("grass_tuft"), TEXT("vine"),
                     TEXT("moss_patch"), TEXT("fairy_ring"), TEXT("flower_bed"), TEXT("root") })
        R.Add({K, SmallVegBuild});

    // ── Mushroom (short cylinder + sphere cap) ──
    R.Add({TEXT("mushroom_cluster"), [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 6.f, 20.f, 6);
        BuildSphere(B, PG, 18.f, FVector(0, 0, 25));
    }});

    // ── Rock / boulder (squashed sphere) ──
    auto RockBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildSphere(B, PG, 40.f, FVector(0, 0, 25), 6, 8);
    };
    for (auto& K : { TEXT("rock"), TEXT("boulder"), TEXT("outcrop"), TEXT("cliff_face"),
                     TEXT("sea_stack"), TEXT("rock_arch") })
        R.Add({K, RockBuild});

    // ── Crystal spire (thin tall cone) ──
    auto CrystalBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCone(B, PG, 12.f, 100.f, 6);
    };
    for (auto& K : { TEXT("stalagmite"), TEXT("stalactite"), TEXT("crystal_cluster"),
                     TEXT("crystal_spire"), TEXT("geode") })
        R.Add({K, CrystalBuild});

    // ── Tower / column (tall cylinder + box cap) ──
    auto TowerBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 20.f, 200.f, 8);
        BuildBox(B, PG, FVector(50, 50, 15), FVector(0, 0, 207));
    };
    for (auto& K : { TEXT("tower"), TEXT("spire"), TEXT("obelisk"), TEXT("minaret"),
                     TEXT("lighthouse"), TEXT("pagoda"), TEXT("bell_tower"), TEXT("collapsed_tower") })
        R.Add({K, TowerBuild});

    // ── Pillar / column (thin cylinder) ──
    auto PillarBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 10.f, 150.f, 8);
        BuildBox(B, PG, FVector(30, 30, 8), FVector(0, 0, 154));
    };
    for (auto& K : { TEXT("pillar"), TEXT("column"), TEXT("monolith"), TEXT("torii") })
        R.Add({K, PillarBuild});

    // ── Large building (box with pitched roof cone) ──
    auto LargeBuildingBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(120, 100, 80), FVector(0, 0, 40));
        BuildCone(B, PG, 70.f, 40.f, 4);
    };
    for (auto& K : { TEXT("castle"), TEXT("fortress"), TEXT("cathedral"), TEXT("palace"),
                     TEXT("citadel"), TEXT("temple"), TEXT("keep"), TEXT("colosseum"),
                     TEXT("amphitheater"), TEXT("arena"), TEXT("monastery"), TEXT("mansion"),
                     TEXT("villa"), TEXT("warehouse"), TEXT("mausoleum"), TEXT("barn"),
                     TEXT("stable"), TEXT("granary"), TEXT("barracks") })
        R.Add({K, LargeBuildingBuild});

    // ── Small building (box with small roof) ──
    auto SmallBuildingBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(60, 50, 50), FVector(0, 0, 25));
        BuildCone(B, PG, 40.f, 25.f, 4);
    };
    for (auto& K : { TEXT("hut"), TEXT("cabin"), TEXT("cottage"), TEXT("shack"), TEXT("shed"),
                     TEXT("farmhouse"), TEXT("chapel"), TEXT("forge"), TEXT("workshop"),
                     TEXT("smithy"), TEXT("tavern"), TEXT("inn"), TEXT("bakery"),
                     TEXT("apothecary"), TEXT("guardhouse"), TEXT("outhouse"), TEXT("boathouse"),
                     TEXT("market_stall"), TEXT("lean_to"), TEXT("windmill"), TEXT("tomb"),
                     TEXT("shrine"), TEXT("marketplace") })
        R.Add({K, SmallBuildingBuild});

    // ── Bridge / platform (flat wide box) ──
    auto PlatformBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(200, 60, 10), FVector(0, 0, 5));
    };
    for (auto& K : { TEXT("bridge"), TEXT("dock"), TEXT("pier"), TEXT("jetty"), TEXT("wharf"),
                     TEXT("platform"), TEXT("terrace"), TEXT("stairway"), TEXT("aqueduct"),
                     TEXT("colonnade"), TEXT("drawbridge") })
        R.Add({K, PlatformBuild});

    // ── Wall / fence (tall thin box) ──
    auto WallBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(100, 10, 80), FVector(0, 0, 40));
    };
    for (auto& K : { TEXT("wall"), TEXT("fence"), TEXT("rampart"), TEXT("palisade"),
                     TEXT("parapet"), TEXT("battlement"), TEXT("gatehouse"), TEXT("gate"),
                     TEXT("arch"), TEXT("archway"), TEXT("portcullis"), TEXT("ruin_arch") })
        R.Add({K, WallBuild});

    // ── Dome / gazebo (hemisphere) ──
    auto DomeBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildSphere(B, PG, 35.f, FVector(0, 0, 10), 6, 10);
    };
    for (auto& K : { TEXT("dome"), TEXT("cupola"), TEXT("altar"), TEXT("dolmen"), TEXT("barrow"),
                     TEXT("well"), TEXT("fountain"), TEXT("bird_bath"), TEXT("sundial"),
                     TEXT("gazebo"), TEXT("pergola"), TEXT("summoning_circle") })
        R.Add({K, DomeBuild});

    // ── Pyramid (wide cone, few segments) ──
    auto PyramidBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCone(B, PG, 80.f, 100.f, 4);
    };
    for (auto& K : { TEXT("pyramid"), TEXT("ziggurat") })
        R.Add({K, PyramidBuild});

    // ── Tent ──
    R.Add({TEXT("tent"), [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCone(B, PG, 35.f, 40.f, 6);
    }});

    // ── Props: crate/barrel ──
    auto CrateBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(20, 20, 20), FVector(0, 0, 10));
    };
    for (auto& K : { TEXT("crate"), TEXT("chest"), TEXT("barrel"), TEXT("cart"),
                     TEXT("wagon"), TEXT("anchor"), TEXT("sarcophagus") })
        R.Add({K, CrateBuild});

    // ── Props: light sources (small sphere on stick) ──
    auto TorchBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 3.f, 50.f, 6);
        BuildSphere(B, PG, 8.f, FVector(0, 0, 55));
    };
    for (auto& K : { TEXT("torch"), TEXT("lantern"), TEXT("brazier"), TEXT("campfire"),
                     TEXT("bonfire"), TEXT("soul_lantern"), TEXT("crystal_orb"), TEXT("lamp_post") })
        R.Add({K, TorchBuild});

    // ── Signs / markers ──
    auto SignBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 3.f, 60.f, 4);
        BuildBox(B, PG, FVector(30, 3, 20), FVector(0, 0, 60));
    };
    for (auto& K : { TEXT("signpost"), TEXT("banner"), TEXT("flag"), TEXT("gravestone"),
                     TEXT("headstone"), TEXT("cross"), TEXT("memorial"), TEXT("cenotaph"),
                     TEXT("runestone"), TEXT("ward_stone") })
        R.Add({K, SignBuild});

    // ── Portal (tall ring = two concentric cylinders) ──
    auto PortalBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 40.f, 5.f, 16);
        BuildCylinder(B, PG, 38.f, 100.f, 16);
    };
    for (auto& K : { TEXT("portal"), TEXT("rift") })
        R.Add({K, PortalBuild});

    R.Add({TEXT("enchanted_well"), [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 25.f, 20.f, 10);
        BuildSphere(B, PG, 12.f, FVector(0, 0, 25));
    }});

    // ── Siege (box-ish) ──
    auto SiegeBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(40, 20, 15), FVector(0, 0, 8));
        BuildCylinder(B, PG, 4.f, 35.f, 6);
    };
    for (auto& K : { TEXT("cannon"), TEXT("catapult"), TEXT("ballista") })
        R.Add({K, SiegeBuild});

    // ── Ruins (low broken box) ──
    auto RuinBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(60, 40, 25), FVector(0, 0, 12));
        BuildBox(B, PG, FVector(15, 15, 35), FVector(20, 10, 17));
    };
    for (auto& K : { TEXT("ruins"), TEXT("rubble"), TEXT("crumbled_wall"),
                     TEXT("broken_pillar"), TEXT("shipwreck"), TEXT("sunken_ship") })
        R.Add({K, RuinBuild});

    // ── Statue (cylinder base + sphere head) ──
    R.Add({TEXT("statue"), [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 12.f, 80.f, 8);
        BuildSphere(B, PG, 15.f, FVector(0, 0, 90));
    }});

    R.Add({TEXT("throne"), [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildBox(B, PG, FVector(30, 30, 25), FVector(0, 0, 12));
        BuildBox(B, PG, FVector(30, 5, 45), FVector(0, -12, 22));
    }});

    // ── Thermal / water features ──
    auto ThermalBuild = [](FMeshDescriptionBuilder& B, FPolygonGroupID PG) {
        BuildCylinder(B, PG, 30.f, 5.f, 10);
    };
    for (auto& K : { TEXT("hot_spring"), TEXT("geyser"), TEXT("lava_pool"),
                     TEXT("volcanic_vent"), TEXT("cave_mouth"),
                     TEXT("cattail"), TEXT("lily_pad"), TEXT("tidepool"),
                     TEXT("coral_formation") })
        R.Add({K, ThermalBuild});

    return R;
}

// ══════════════════════════════════════════════════════════════════════
// ASSET CREATION
// ══════════════════════════════════════════════════════════════════════

static UStaticMesh* CreateMeshAsset(const FString& Keyword,
    TFunction<void(FMeshDescriptionBuilder&, FPolygonGroupID)> BuildFunc)
{
    FString AssetPath = FString::Printf(TEXT("/Game/BaseX/Meshes/SM_BX_%s"), *Keyword);
    FString AssetName = FString::Printf(TEXT("SM_BX_%s"), *Keyword);
    FString FullPath = AssetPath + TEXT(".") + AssetName;

    // Already exists?
    if (UStaticMesh* Existing = LoadObject<UStaticMesh>(nullptr, *FullPath))
        return Existing;

    if (!IsInGameThread())
    {
        UE_LOG(LogTemp, Warning, TEXT("BaseXMeshFactory: Skipping %s — not on game thread"), *Keyword);
        return nullptr;
    }

    UPackage* Pkg = CreatePackage(*AssetPath);
    if (!Pkg)
    {
        UE_LOG(LogTemp, Error, TEXT("BaseXMeshFactory: Failed to create package for %s"), *Keyword);
        return nullptr;
    }
    Pkg->FullyLoad();

    UStaticMesh* SM = NewObject<UStaticMesh>(Pkg, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

    // Build geometry
    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attrs(MeshDesc);
    Attrs.Register();

    FMeshDescriptionBuilder Builder;
    Builder.SetMeshDescription(&MeshDesc);
    Builder.EnablePolyGroups();
    Builder.SetNumUVLayers(1);

    FPolygonGroupID PG = Builder.AppendPolygonGroup();
    BuildFunc(Builder, PG);

    if (MeshDesc.Vertices().Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("BaseXMeshFactory: %s — geometry produced 0 vertices"), *Keyword);
        return nullptr;
    }

    // Build the static mesh from description
    TArray<const FMeshDescription*> MeshDescs;
    MeshDescs.Add(&MeshDesc);

    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bMarkPackageDirty = false;
    Params.bBuildSimpleCollision = true;
    Params.bFastBuild = true;

    SM->GetStaticMaterials().Add(FStaticMaterial());
    SM->BuildFromMeshDescriptions(MeshDescs, Params);

    // Save
    FAssetRegistryModule::AssetCreated(SM);
    Pkg->MarkPackageDirty();
    FString FilePath = FPackageName::LongPackageNameToFilename(
        Pkg->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    bool bSaved = UPackage::SavePackage(Pkg, SM, *FilePath, SaveArgs);

    if (!bSaved)
        UE_LOG(LogTemp, Error, TEXT("BaseXMeshFactory: Failed to save %s"), *AssetPath);

    return SM;
}

void BaseXMeshFactory::EnsureStructureMeshesExist()
{
    TArray<FMeshRecipe> Recipes = GetAllRecipes();
    int32 Created = 0, Existed = 0;

    for (const FMeshRecipe& Recipe : Recipes)
    {
        FString FullPath = FString::Printf(TEXT("/Game/BaseX/Meshes/SM_BX_%s.SM_BX_%s"), *Recipe.Keyword, *Recipe.Keyword);
        if (LoadObject<UStaticMesh>(nullptr, *FullPath))
        {
            Existed++;
            continue;
        }

        UStaticMesh* SM = CreateMeshAsset(Recipe.Keyword, Recipe.BuildFunc);
        if (SM) Created++;
    }

    UE_LOG(LogTemp, Warning, TEXT("BaseXMeshFactory: %d meshes created, %d already existed (%d total keywords)"),
        Created, Existed, Recipes.Num());
}

#else // !WITH_EDITOR

void BaseXMeshFactory::EnsureStructureMeshesExist() {}

#endif

UStaticMesh* BaseXMeshFactory::LoadMesh(const FString& Keyword)
{
    FString FullPath = FString::Printf(TEXT("/Game/BaseX/Meshes/SM_BX_%s.SM_BX_%s"), *Keyword, *Keyword);
    return LoadObject<UStaticMesh>(nullptr, *FullPath);
}
