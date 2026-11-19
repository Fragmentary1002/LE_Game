"""在新的编辑器进程中验证 SoulsCore 的资产引用和蓝图编译。"""

import json
from pathlib import Path
import runpy
import unreal


# 读取并验证所有入口配置，不修改或保存设计师资产。
def main():
    unreal.load_module("SoulsCoreRuntime")
    unreal.load_module("SoulsCoreEditor")
    if not unreal.SoulsSetupLibrary.prepare_content_for_setup():
        raise RuntimeError("无法挂载 SoulsCore")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/SoulsCore"], True)
    setup = runpy.run_path(str(Path(__file__).with_name("setup_souls_core.py")), run_name="souls_setup_helpers")
    load = setup["require_asset"]
    cls = setup["require_class"]
    prefix = "/SoulsCore"
    blueprints = ["Characters/B_Hero_Souls", "Camera/CM_Souls_Free", "Camera/CM_Souls_LockOn",
                  "UI/W_SoulsHUD", "Experiences/B_SoulsExperience", "Development/B_SoulsTrainingTarget"]
    for path in blueprints:
        if not unreal.SoulsEditorSetupLibrary.compile_blueprint_for_setup(load(prefix + "/" + path)):
            raise RuntimeError("蓝图编译失败：" + path)
    experience = unreal.get_default_object(cls(prefix + "/Experiences/B_SoulsExperience.B_SoulsExperience_C"))
    data = load(prefix + "/Characters/HeroData_Souls")
    config = load(prefix + "/Input/InputConfig_Souls")
    hud_set = load(prefix + "/Experiences/LAS_Souls_HUD")
    assert list(experience.get_editor_property("game_features_to_enable")) == ["SoulsCore"]
    assert experience.get_editor_property("default_pawn_data") == data
    assert list(experience.get_editor_property("action_sets")) == [hud_set]
    assert len(hud_set.get_editor_property("actions")) == 1
    assert data.get_editor_property("pawn_class") == cls(prefix + "/Characters/B_Hero_Souls.B_Hero_Souls_C")
    assert data.get_editor_property("input_config") == config
    assert data.get_editor_property("default_camera_mode") == cls(prefix + "/Camera/CM_Souls_Free.CM_Souls_Free_C")
    assert data.get_editor_property("lock_on_camera_mode") == cls(prefix + "/Camera/CM_Souls_LockOn.CM_Souls_LockOn_C")
    abilities = load(prefix + "/Characters/AbilitySet_Souls")
    assert list(data.get_editor_property("ability_sets")) == [abilities]
    assert len(abilities.get_editor_property("granted_gameplay_abilities")) == 1
    hero = unreal.get_default_object(data.get_editor_property("pawn_class"))
    mesh = hero.get_editor_property("mesh")
    assert mesh.get_skeletal_mesh_asset() == load("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny")
    assert mesh.get_editor_property("relative_location").z == -90
    assert hero.get_editor_property("unarmed_animation_layer")
    actions = [entry.get_editor_property("input_action") for entry in config.get_editor_property("native_input_actions")]
    assert len(actions) == 5
    assert all(action.get_path_name().startswith(prefix + "/Input/Actions/") for action in actions)
    mapping = config.get_editor_property("mapping_context")
    assert mapping == load(prefix + "/Input/IMC_Souls")
    mappings = mapping.get_editor_property("mappings")
    assert len(mappings) == 12
    assert all(entry.get_editor_property("action") in actions for entry in mappings)
    feature = load(prefix + "/SoulsCore")
    assert len(feature.get_editor_property("primary_asset_types_to_scan")) == 3
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level(prefix + "/Maps/EmptySoulsMap")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    assert settings.get_editor_property("default_gameplay_experience") == cls(prefix + "/Experiences/B_SoulsExperience.B_SoulsExperience_C")
    assert settings.get_editor_property("force_standalone_net_mode")
    assert settings.get_editor_property("default_game_mode") == cls("/Game/B_LyraGameMode.B_LyraGameMode_C")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    targets = [actor for actor in actors if isinstance(actor, unreal.SoulsTrainingTarget)]
    starts = [actor for actor in actors if isinstance(actor, unreal.LyraPlayerStart)]
    assert len(targets) == 2
    assert len(starts) == 1
    report = {"result": "passed", "blueprints": blueprints, "input_actions": len(actions),
              "key_mappings": len(mappings), "training_targets": len(targets),
              "map": prefix + "/Maps/EmptySoulsMap",
              "actors": [actor.get_actor_label() for actor in actors]}
    destination = Path(unreal.Paths.project_saved_dir()) / "SoulsCore" / "ContentValidation.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SOULS_CONTENT_VALIDATION_PASSED")


if __name__ == "__main__":
    main()
