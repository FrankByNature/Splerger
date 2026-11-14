# Content (made in the Unreal editor)

These are `.uasset` files, so they're created inside the Unreal editor, not by hand.
Make them under `Mods/Splerger/Content/` in the Content Browser. The folder layout
below is the suggested one; the asset names are what the code and docs assume.

| Asset | Type (parent class) | Key settings |
|---|---|---|
| `Buildable/Build_Splerger` | Blueprint Class, parent `BuildableSplerger` | Static mesh (vanilla splitter mesh to start), 4 × `FGFactoryConnectionComponent` named **Input0, Input1, Output0, Output1** with Direction set (see `docs/DESIGN.md`), `mHologramClass` = `FGConveyorAttachmentHologram`, display name / description, clearance box |
| `Buildable/Desc_Splerger` | Blueprint Class, parent `FGBuildingDescriptor` | `mBuildableClass` = `Build_Splerger`, icons, build-menu category (same as the vanilla splitter) |
| `Recipes/Recipe_Splerger` | Blueprint Class, parent `FGRecipe` | Product: 1 × `Desc_Splerger`; Ingredients: 2 Iron Plate, 2 Cable, 2 Iron Rod; Produced In: `BP_BuildGun` |
| `Schematics/Schematic_Splerger` | Blueprint Class, parent `FGSchematic` | Type Milestone, Tier 2 (or Custom for testing), Unlocks → Recipe_Splerger |
| `RootGameWorld_Splerger` | Blueprint Class, parent `GameWorldModule` | **Root Module** ticked, `mSchematics` += `Schematic_Splerger` |
| `Icons/IconDesc_Splerger_256`, `_64` | Texture2D | Import from `../../../Art/` |
