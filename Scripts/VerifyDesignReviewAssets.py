"""Read-only verification of the bundled template after migration."""
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(["/CXMR/Core", "/Game/Vehicle/Example", "/Game/Map"], True)
paths = list(unreal.EditorAssetLibrary.list_assets("/CXMR/Core", True, False))
paths += list(unreal.EditorAssetLibrary.list_assets("/Game/Vehicle/Example", True, False))
for path in paths:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Template asset could not be loaded: " + path)
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/L_Main")
if world is None:
    raise RuntimeError("Template map could not be loaded.")
roots = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CXMRVehicleRoot))
if not roots:
    raise RuntimeError("Template map has no vehicle root.")
for root in roots:
    names = [component.get_class().get_name() for component in root.get_components_by_class(unreal.ActorComponent)]
    if "CXMRTurntableComponent" in names or "CXMRErgonomicsComponent" in names:
        raise RuntimeError("Template root retains a retired default component: " + str(names))
    loader = root.get_editor_property("loader")
    anchor = root.get_editor_property("vehicle_anchor")
    if loader.get_editor_property("attach_target") != anchor:
        raise RuntimeError("Template loader is not attached to the calibrated anchor.")
unreal.log("CXMR design review template asset verification completed.")
