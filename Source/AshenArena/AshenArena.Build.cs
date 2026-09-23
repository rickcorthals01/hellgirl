using UnrealBuildTool;
public class AshenArena : ModuleRules
{
    public AshenArena(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent", "DeveloperSettings", "Slate", "SlateCore", "ImageCore" });
    }
}

