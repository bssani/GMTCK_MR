"""Run once in Unreal Editor Python after rebuilding the CXMR plugin.

Migrates only the bundled template assets. Custom vehicle Blueprints must be
compiled and migrated separately. A failed dependency check keeps retired assets.
"""

import unreal

RETIRED_ACTIONS = [
    "IA_CXMR_CycleTrim", "IA_CXMR_PanelClick", "IA_CXMR_SpinLeft",
    "IA_CXMR_SpinRight", "IA_CXMR_TurntableAxis",
    "IA_Varjo_GazeVisualizationToggle",
    "IA_Varjo_FoveatedRenderingVisualizationToggle",
    "IA_Varjo_DynamicTrackingToggle",
]
ACTION_FOLDER = "/CXMR/Core/Input/Actions/"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(["/CXMR/Core", "/Game/Vehicle/Example"], True)

context = unreal.EditorAssetLibrary.load_asset("/CXMR/Core/Input/IMC_Varjo")
require(context is not None, "Bundled input context could not be loaded.")
for name in RETIRED_ACTIONS:
    path = ACTION_FOLDER + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        action = unreal.EditorAssetLibrary.load_asset(path)
        require(action is not None, "Retired action could not be loaded: " + path)
        context.unmap_all_keys_from_action(action)
require(unreal.EditorAssetLibrary.save_loaded_asset(context, False), "Input context save failed.")

# 현재 C++에서 없어진 기본값과 컴포넌트 참조를 다시 저장함.
paths = list(unreal.EditorAssetLibrary.list_assets("/CXMR/Core", True, False))
paths += list(unreal.EditorAssetLibrary.list_assets("/Game/Vehicle/Example", True, False))
for path in paths:
    if any(path.startswith(ACTION_FOLDER + name + ".") for name in RETIRED_ACTIONS):
        continue
    asset = unreal.EditorAssetLibrary.load_asset(path)
    require(asset is not None, "Bundled asset load failed: " + path)
    if isinstance(asset, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    if (path.startswith("/CXMR/Core/Pawn/BP_CXMRPawn.")
            or path.startswith("/Game/Vehicle/Example/BP_VehicleRoot_TestCar.")
            or isinstance(asset, unreal.CXMRVehicleProfile)):
        require(unreal.EditorAssetLibrary.save_loaded_asset(asset, False), "Bundled asset save failed: " + path)

# 배치된 맵 인스턴스의 이전 턴테이블 참조도 앵커로 옮김.
registry.scan_paths_synchronous(["/Game/Map"], True)
world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/L_Main")
require(world is not None, "Template map could not be loaded.")
for root in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CXMRVehicleRoot):
    root.get_editor_property("loader").set_editor_property(
        "attach_target", root.get_editor_property("vehicle_anchor"))
require(unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Map/L_Main"), "Template map save failed.")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
for name in RETIRED_ACTIONS:
    path = ACTION_FOLDER + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        continue
    # 레지스트리에 남은 캐시 대신 로드한 실제 참조를 확인함.
    refs = unreal.EditorAssetLibrary.find_package_referencers_for_asset(path, True)
    require(not refs, "Retired action still referenced; kept: " + path + " " + str(refs))
    require(unreal.EditorAssetLibrary.delete_asset(path), "Retired action deletion failed: " + path)
    unreal.log("CXMR retired action removed: " + name)

unreal.log("CXMR design review asset migration completed.")
