"""Import the Splerger icon PNGs, set them on Desc_Splerger, and set the
building's display name (the build menu takes the name from Build_Splerger).

Run inside the Unreal editor (Output Log -> switch "Cmd" to "Python"):
    py "<repo>/scripts/import_icon.py"
Re-running it reimports and overwrites.

Source PNGs are read through the C:/Modding/SplergerArt junction so the
user's own path is not stored in the texture's import data.
"""
import unreal

ART = "C:/Modding/SplergerArt/Icon"
DEST = "/Splerger/UI"
DESC = "/Splerger/Buildable/Desc_Splerger"
BUILD = "/Splerger/Buildable/Build_Splerger"
DISPLAY_NAME = "Conveyor Splerger"
VANILLA = "/Game/FactoryGame/Buildable/Factory/CA_Splitter/UI/IconDesc_ConveyorSplitter_{}"

eal = unreal.EditorAssetLibrary


def import_icon(size):
    name = f"IconDesc_Splerger_{size}"
    task = unreal.AssetImportTask()
    task.filename = f"{ART}/Icon_Splerger_outputs_{size}.png"
    task.destination_path = DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    tex = eal.load_asset(f"{DEST}/{name}")
    if tex is None:
        raise RuntimeError(f"Import failed for {task.filename}")

    # Match the vanilla splitter icon's texture settings when the starter
    # project has it; otherwise use the usual UI-icon settings.
    ref = eal.load_asset(VANILLA.format(size)) if eal.does_asset_exist(VANILLA.format(size)) else None
    for prop, fallback in (
        ("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
        ("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),
        ("srgb", True),
        ("never_stream", True),
    ):
        value = ref.get_editor_property(prop) if ref else fallback
        tex.set_editor_property(prop, value)
    eal.save_loaded_asset(tex, only_if_is_dirty=False)
    unreal.log(f"[Splerger] imported {tex.get_path_name()} ({'copied vanilla settings' if ref else 'default UI settings'})")
    return tex


small = import_icon(256)
big = import_icon(512)

bp = eal.load_asset(DESC)
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("mSmallIcon", small)
cdo.set_editor_property("mPersistentBigIcon", big)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_asset(DESC, only_if_is_dirty=False)

cdo = unreal.get_default_object(bp.generated_class())
unreal.log(f"[Splerger] Desc_Splerger small icon = {cdo.get_editor_property('mSmallIcon').get_path_name()}")
unreal.log(f"[Splerger] Desc_Splerger big icon   = {cdo.get_editor_property('mPersistentBigIcon').get_path_name()}")

build_bp = eal.load_asset(BUILD)
build_cdo = unreal.get_default_object(build_bp.generated_class())
build_cdo.set_editor_property("mDisplayName", unreal.Text(DISPLAY_NAME))
unreal.BlueprintEditorLibrary.compile_blueprint(build_bp)
eal.save_asset(BUILD, only_if_is_dirty=False)
build_cdo = unreal.get_default_object(build_bp.generated_class())
unreal.log(f"[Splerger] Build_Splerger display name = {build_cdo.get_editor_property('mDisplayName')}")
