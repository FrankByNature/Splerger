# Content (made in the Unreal editor)

These are `.uasset` files, so they're created inside the Unreal editor, not by hand.
In the Content Browser they live under **Plugins → Splerger** (next to "Splerger C++ Classes") (if Plugins isn't
listed: Content Browser ⚙ Settings → tick *Show Plugin Content*).

Make them in this order: each one refers to the one before.
Field names are as the Details panel shows them; use the search box at its top.

Vanilla assets to copy from (all in the starter project):

| What | Path |
|---|---|
| Splitter building | `/Game/FactoryGame/Buildable/Factory/CA_Splitter/Build_ConveyorAttachmentSplitter` |
| Splitter descriptor | `/Game/FactoryGame/Buildable/Factory/CA_Splitter/Desc_ConveyorAttachmentSplitter` |
| Splitter mesh | `.../CA_Splitter/Mesh/SM_Splitter_01` (and `ConveyorAttachmentSplitter_static`) |
| Splitter icons | `.../CA_Splitter/UI/IconDesc_ConveyorSplitter_256` / `_512` |
| Build menu category | `/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/BC_Logistics` |
| Build menu sub-category | `.../BuildCategories/Sub_Transport/SC_ConveyorAttachments` |
| Build gun (recipe "produced in") | `/Game/FactoryGame/Equipment/BuildGun/BP_BuildGun` |

The starter project's copies of game assets are placeholders: meshes and icons may
look blank or grey in the editor. References to them still resolve to the real
assets in the game.

## 1. `Buildable/Build_Splerger`: the building

1. Right-click in `Splerger/Buildable` → *Blueprint Class* → *All Classes* →
   search **BuildableSplerger** → name it `Build_Splerger`.
2. Open the vanilla `Build_ConveyorAttachmentSplitter` next to it and copy across,
   one setting at a time:
   - its static mesh component (same mesh)
   - the **clearance** box / clearance data
   - **Hologram Class** → `Holo_Splerger` (see below)
   - **Inventory Size** is set by the C++ (2); leave it.
3. Add four **FGFactoryConnection** components, named exactly
   `Input1`, `Output1`, `Input2`, `Output2`:
   - copy location / rotation / connector clearance from the splitter's connections
     (its straight-through input and output → our Input1 / Output1, one of its side
     outputs → our Output2, the other side → our Input2). The straight-through pair
     **must** be called Input1/Output1: the vanilla hologram connects a cut belt to
     the components with those names.
   - **Direction**: Input for `Input1`/`Input2`, Output for `Output1`/`Output2`
   - **Connector** type: Conveyor
4. **Display Name**: Conveyor Splerger (like "Conveyor Splitter"). **Description**: e.g. "Two inputs, two outputs.
   Merges both inputs and splits them evenly across both outputs."
5. Compile + Save.

`scripts/setup_ports_and_hologram.py` does the port renaming (1.0.0 used
Input0/Input1/Output0/Output1) and the hologram: it copies the vanilla
`Holo_ConveyorAttachment` to `Buildable/Holo_Splerger`, reparents it to the C++
`SplergerHologram` (adds flipping the side ports when placing on a belt), and sets it
as Build_Splerger's Hologram Class.

## 2. `Buildable/Desc_Splerger`: what the build menu shows

1. *Blueprint Class* → parent **FGBuildingDescriptor** → `Desc_Splerger`.
2. Copy settings from `Desc_ConveyorAttachmentSplitter`, then change:
   - **Buildable Class** → `Build_Splerger`
   - **Category** → `BC_Logistics`, **Sub Categories** → `SC_ConveyorAttachments`
   - **Small Icon** / **Big Icon** → the splitter icons for now (own icons later)

## 3. `Recipes/Recipe_Splerger`: the cost

1. *Blueprint Class* → parent **FGRecipe** → `Recipe_Splerger`.
2. **Product**: 1 × `Desc_Splerger`.
3. **Ingredients**: 2 × Iron Plate, 2 × Cable, 2 × Iron Rod
   (`Desc_IronPlate`, `Desc_Cable`, `Desc_IronRod`).
4. **Produced In**: `BP_BuildGun`.

## 4. `Schematics/Schematic_Splerger`: the unlock

1. *Blueprint Class* → parent **FGSchematic** → `Schematic_Splerger`.
2. **Type**: Milestone, **Tech Tier**: 2, **Display Name**: Splerger.
3. **Cost**: something small, e.g. 50 Iron Plate, 50 Cable.
4. **Unlocks** → add *BP Unlock Recipe* → Recipes → `Recipe_Splerger`.
5. Optional: **Schematic Dependencies** on `Schematic_2-1` (Logistics) so it shows
   up after that milestone.

## 5. `RootGameWorld_Splerger`: registers the schematic with SML

1. *Blueprint Class* → parent **GameWorldModule** → `RootGameWorld_Splerger`
   (at the top of the `Splerger` folder).
2. Tick **Root Module**.
3. **Schematics** → add `Schematic_Splerger`.

## 6. `UI/IconDesc_Splerger_256` / `_512`: the icons

Made from `Art/Icon/Icon_Splerger_outputs_256.png` / `_512.png` (the outputs side,
cut out of an in-game screenshot). `scripts/import_icon.py` imports them, copies the
vanilla splitter icons' texture settings, and sets them on `Desc_Splerger` as
**Small Icon** (256) and **Big Icon** (512). The PNGs fill the full width with no
side padding, like the vanilla splitter/merger icons. The script also sets
`Build_Splerger`'s **Display Name** to "Conveyor Splerger". Run it in the editor's Output Log
(Cmd → Python): `py "<repo>/scripts/import_icon.py"`.
