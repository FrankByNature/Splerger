"""Rename Build_Splerger's ports and give it its own hologram (mod 1.1.0).

Why: the vanilla conveyor-attachment hologram wires a belt it cuts into the
components named "Input1" / "Output1". In 1.0.0 those were the Splerger's
side ports, so placing it on a belt used up the side ports. Now:

    Input1  (-X)  Output1 (+X)   straight through, what a cut belt connects to
    Input2  (+Y)  Output2 (-Y)   the sides

It also creates Holo_Splerger (a copy of the vanilla Holo_ConveyorAttachment
reparented to the C++ ASplergerHologram, which adds flipping on a belt) and
sets it as Build_Splerger's Hologram Class.

Needs the C++ module built first (SplergerHologram must exist).
Run inside the Unreal editor (Output Log -> switch "Cmd" to "Python"):
    py "<repo>/scripts/setup_ports_and_hologram.py"
Safe to re-run: steps already done are skipped.
"""
import unreal

BUILD = "/Splerger/Buildable/Build_Splerger"
HOLO = "/Splerger/Buildable/Holo_Splerger"
VANILLA_HOLO = "/Game/FactoryGame/Buildable/Factory/-Shared/Holo_ConveyorAttachment"
HOLO_PARENT = "/Script/Splerger.SplergerHologram"

# Order matters: free up a name before reusing it.
RENAMES = [("Input1", "Input2"), ("Output1", "Output2"), ("Input0", "Input1"), ("Output0", "Output1")]

eal = unreal.EditorAssetLibrary
bel = unreal.BlueprintEditorLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def components(bp):
    """{variable name: (handle, template)} for the Blueprint's own components."""
    out = {}
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = sdl.get_data(handle)
        name = str(sdl.get_variable_name(data))
        if name and name != "None":
            out[name] = (handle, sdl.get_object(data))
    return out


def print_ports(bp, label):
    unreal.log(f"[Splerger] ports {label}:")
    for name, (_, obj) in sorted(components(bp).items()):
        if isinstance(obj, unreal.FGFactoryConnectionComponent):
            try:
                direction = obj.get_editor_property("mDirection")
            except Exception:
                direction = "?"
            unreal.log(f"[Splerger]   {name}: location {obj.get_editor_property('relative_location')}, "
                       f"rotation {obj.get_editor_property('relative_rotation')}, direction {direction}")


def rename_ports(bp):
    comps = components(bp)
    if "Input2" in comps and "Output2" in comps and "Input0" not in comps and "Output0" not in comps:
        unreal.log("[Splerger] ports already renamed")
        return
    for old, new in RENAMES:
        comps = components(bp)
        if old not in comps:
            raise RuntimeError(f"Build_Splerger has no component {old}; found {sorted(comps)}")
        if new in comps:
            raise RuntimeError(f"Build_Splerger already has a component {new}; rename by hand")
        ok = sds.rename_subobject_member_variable(bp, comps[old][0], unreal.Name(new))
        unreal.log(f"[Splerger] renamed {old} -> {new}: {ok}")


def make_hologram():
    parent = unreal.load_class(None, HOLO_PARENT)
    if not parent:
        raise RuntimeError(f"{HOLO_PARENT} not found: build the C++ module first")
    if not eal.does_asset_exist(HOLO):
        if not eal.duplicate_asset(VANILLA_HOLO, HOLO):
            raise RuntimeError(f"could not copy {VANILLA_HOLO}")
        unreal.log(f"[Splerger] copied {VANILLA_HOLO} -> {HOLO}")
    holo = eal.load_asset(HOLO)
    bel.reparent_blueprint(holo, parent)
    bel.compile_blueprint(holo)
    eal.save_loaded_asset(holo)
    unreal.log(f"[Splerger] {HOLO} parent = {HOLO_PARENT}")
    return eal.load_blueprint_class(HOLO)


def main():
    bp = eal.load_asset(BUILD)
    print_ports(bp, "before")
    rename_ports(bp)

    holo_class = make_hologram()
    cdo = unreal.get_default_object(eal.load_blueprint_class(BUILD))
    cdo.set_editor_property("mHologramClass", holo_class)

    bel.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    print_ports(bp, "after")
    unreal.log(f"[Splerger] Build_Splerger hologram = {cdo.get_editor_property('mHologramClass')}")


main()
