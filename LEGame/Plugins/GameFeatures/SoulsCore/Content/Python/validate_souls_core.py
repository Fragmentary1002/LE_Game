"""在新的编辑器进程中验证 SoulsCore 的资产引用和蓝图编译。"""

import json
from pathlib import Path
import runpy
import unreal


# 检查实际保存的鼠标映射，保证额外修饰器不会反转 UE 已处理的鼠标轴。
def validate_mouse_direction(mapping):
    mouse = [entry for entry in mapping.get_editor_property("mappings")
             if entry.get_editor_property("key").export_text() == "Mouse2D"]
    assert len(mouse) == 1, "IMC_Souls 应当只有一条 Mouse2D 映射"
    entry = mouse[0]
    action = entry.get_editor_property("action")
    assert action.get_editor_property("value_type") == unreal.InputActionValueType.AXIS2D
    modifiers = list(entry.get_editor_property("modifiers")) + list(action.get_editor_property("modifiers"))
    for modifier in modifiers:
        if isinstance(modifier, unreal.InputModifierNegate):
            assert not modifier.get_editor_property("y"), "鼠标 Y 轴被额外取反，上移会变成向下看"
            assert not modifier.get_editor_property("x"), "鼠标 X 轴被额外取反，右移会变成向左看"


# 验证单次攻击的输入、伤害、动画窗口和骨骼目标均已保存为真实资产。
def validate_combat_content(load, cls, config, abilities, hero, actors):
    root = "/SoulsCore"
    blueprints = ["Combat/Weapons/B_SoulsWeapon_TestSword", "Combat/Abilities/GA_Souls_LightAttack",
                  "Combat/Abilities/GA_Souls_HitReact", "Combat/Abilities/GA_Souls_Death",
                  "Combat/Effects/GE_Souls_MeleeDamage", "Combat/Effects/GE_Souls_CombatInit",
                  "Development/B_SoulsCombatDummy"]
    for path in blueprints:
        assert unreal.SoulsEditorSetupLibrary.compile_blueprint_for_setup(load(root + "/" + path)), path
    attack = cls(root + "/Combat/Abilities/GA_Souls_LightAttack.GA_Souls_LightAttack_C")
    hit = cls(root + "/Combat/Abilities/GA_Souls_HitReact.GA_Souls_HitReact_C")
    death = cls(root + "/Combat/Abilities/GA_Souls_Death.GA_Souls_Death_C")
    granted = [entry.get_editor_property("ability") for entry in abilities.get_editor_property("granted_gameplay_abilities")]
    assert all(ability in granted for ability in (attack, hit, death)), "角色能力集缺少战斗能力"
    assert cls("/Game/Characters/Heroes/Abilities/GA_Hero_Death.GA_Hero_Death_C") not in granted, "旧死亡能力没有替换"
    attributes = [entry.get_editor_property("attribute_set") for entry in abilities.get_editor_property("granted_attributes")]
    assert cls("/Script/LyraGame.LyraCombatSet") in attributes, "未授予来源攻击属性"
    init_class = cls(root + "/Combat/Effects/GE_Souls_CombatInit.GE_Souls_CombatInit_C")
    assert init_class in [entry.get_editor_property("gameplay_effect") for entry in abilities.get_editor_property("granted_gameplay_effects")]
    definition = load(root + "/Combat/DA_Souls_LightAttack")
    montage = load(root + "/Combat/Animations/AM_Souls_LightAttack")
    assert definition.get_editor_property("attack_montage") == montage
    assert unreal.SoulsCombatEditorLibrary.validate_attack_montage(montage), "攻击动画窗口或片段配置错误"
    damage_class = cls(root + "/Combat/Effects/GE_Souls_MeleeDamage.GE_Souls_MeleeDamage_C")
    assert definition.get_editor_property("damage_effect_class") == damage_class
    damage = unreal.get_default_object(damage_class)
    execution = damage.get_editor_property("executions")
    assert len(execution) == 1
    assert execution[0].get_editor_property("calculation_class") == cls("/Script/LyraGame.LyraDamageExecution")
    trace = hero.get_component_by_class(unreal.SoulsMeleeTraceComponent)
    assert trace and trace.get_editor_property("default_attack_definition") == definition
    assert trace.get_editor_property("weapon_class") == cls(root + "/Combat/Weapons/B_SoulsWeapon_TestSword.B_SoulsWeapon_TestSword_C")
    action = load(root + "/Input/Actions/IA_Souls_LightAttack")
    inputs = [entry for entry in config.get_editor_property("ability_input_actions")
              if entry.get_editor_property("input_action") == action]
    assert len(inputs) == 1 and "InputTag.Souls.LightAttack" in inputs[0].get_editor_property("input_tag").export_text()
    keys = [entry.get_editor_property("key").export_text()
            for entry in config.get_editor_property("mapping_context").get_editor_property("mappings")
            if entry.get_editor_property("action") == action]
    assert "LeftMouseButton" in keys and "Gamepad_RightShoulder" in keys
    dummies = [actor for actor in actors if isinstance(actor, unreal.SoulsCombatDummy)]
    assert len(dummies) >= 3, "骨骼战斗训练目标没有加入地图"
    for dummy in dummies:
        assert dummy.get_editor_property("team_id") == 2
        assert hit in dummy.get_editor_property("combat_abilities") and death in dummy.get_editor_property("combat_abilities")
        assert dummy.get_editor_property("mesh").get_skeletal_mesh_asset()
    return {"blueprints": blueprints, "combat_dummies": len(dummies), "attack_keys": keys,
            "attack_montage": montage.get_path_name(), "damage_execution": "LyraDamageExecution"}


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
    assert len(abilities.get_editor_property("granted_gameplay_abilities")) >= 3
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
    assert len(mappings) >= 14
    ability_actions = [entry.get_editor_property("input_action") for entry in config.get_editor_property("ability_input_actions")]
    assert all(entry.get_editor_property("action") in actions + ability_actions for entry in mappings)
    validate_mouse_direction(mapping)
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
    combat = validate_combat_content(load, cls, config, abilities, hero, actors)
    report = {"result": "passed", "blueprints": blueprints, "input_actions": len(actions),
              "key_mappings": len(mappings), "training_targets": len(targets),
              "map": prefix + "/Maps/EmptySoulsMap",
              "actors": [actor.get_actor_label() for actor in actors], "combat": combat}
    destination = Path(unreal.Paths.project_saved_dir()) / "SoulsCore" / "ContentValidation.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SOULS_CONTENT_VALIDATION_PASSED")


if __name__ == "__main__":
    main()
