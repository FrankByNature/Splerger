using UnrealBuildTool;

public class Splerger : ModuleRules
{
	public Splerger(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"DeveloperSettings",
			"PhysicsCore",
			// Satisfactory + mod loader
			"FactoryGame",
			"SML",
		});
	}
}
