using UnrealBuildTool;
using System.IO;

public class BaseXWorldGen : ModuleRules
{
    public BaseXWorldGen(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // ── UE Module Dependencies ──
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Slate",
            "SlateCore",
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore",
            "RHI",
            "ProceduralMeshComponent"
        });

        // Editor-only modules for asset creation
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "AssetTools"
            });
        }

        // ── Base-X Pure C++ Headers ──
        // These are your original engine headers — no UE dependency,
        // copied into ThirdParty/BaseX/ from include/BaseX/
        // Add ThirdParty/ so #include "BaseX/DRO.h" resolves correctly
        string ThirdPartyPath = Path.Combine(ModuleDirectory, "..", "..", "ThirdParty");
        PublicIncludePaths.Add(ThirdPartyPath);

        // Allow standard C++ headers that Base-X uses
        bEnableExceptions = true;
        CppStandard = CppStandardVersion.Cpp20;
    }
}
