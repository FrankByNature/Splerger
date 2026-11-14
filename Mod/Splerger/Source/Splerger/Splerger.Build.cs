using UnrealBuildTool;

public class Splerger : ModuleRules
{
	public Splerger(ReadOnlyTargetRules Target) : base(Target)
	{
		// Same settings as SML's own module (Mods/SML/Source/SML/SML.Build.cs).
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			// Satisfactory + mod loader
			"FactoryGame",
			"SML",
		});
	}
}
