using UnrealBuildTool;
public class Hellgirl : ModuleRules
{
    public Hellgirl(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Source is organized in subfolders (Fighter/, Enemies/, Levels/...); includes are relative to this folder.
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent", "Niagara", "DeveloperSettings", "Slate", "SlateCore", "ImageCore" });
    }
}

