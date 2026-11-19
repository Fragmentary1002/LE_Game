"""在首次正常构建后，通过 Unreal 编辑器的 Python 执行本脚本生成魂类资产。"""

import unreal


# 加载必要的反射类型，类型缺失时在任何资产写入前停止。
def require_class(path):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError("缺少类型 {}，请先正常构建 LyraEditor 并重启编辑器。".format(path))
    return cls


# 仅创建尚不存在的资产，保留设计师已经调整过的资产。
def create_asset(path, cls):
    existing = unreal.load_asset(path)
    if existing:
        if existing.get_class() != cls:
            raise RuntimeError("资产类型不匹配：{}".format(path))
        return existing, False
    package, name = path.rsplit("/", 1)
    unreal.EditorAssetLibrary.make_directory(package)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, cls, factory)
    if not asset:
        raise RuntimeError("无法创建资产：{}".format(path))
    return asset, True


# 创建一次蓝图资产，重复执行时保留已有配置。
def create_blueprint(path, parent):
    existing = unreal.load_asset(path)
    if existing:
        return existing, False
    package, name = path.rsplit("/", 1)
    unreal.EditorAssetLibrary.make_directory(package)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, package, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError("无法创建蓝图：{}".format(path))
    return blueprint, True


# 保存指定资产并报告写入失败。
def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("无法保存：{}".format(asset.get_path_name()))


# 构造 GameFeatureData 中的主资产扫描规则。
def scan_rule(name, base_class, directory, blueprints=False):
    rule = unreal.PrimaryAssetTypeInfo()
    rule.set_editor_property("primary_asset_type", name)
    rule.set_editor_property("asset_base_class", base_class)
    rule.set_editor_property("has_blueprint_classes", blueprints)
    rule.set_editor_property("is_editor_only", False)
    rule.set_editor_property("directories", [unreal.DirectoryPath(directory)])
    rules = unreal.PrimaryAssetRules()
    rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
    rule.set_editor_property("rules", rules)
    return rule


# 在新关卡中添加具备碰撞的灰盒几何体。
def add_block(actors, label, location, scale):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(label)
    actor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    actor.static_mesh_component.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


# 创建全新的测试关卡，保留当前打开关卡及所有已有地图。
def create_test_map(experience_class, target_class):
    path = "/SoulsCore/Maps/L_Souls_Test"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    current = editor.get_editor_world()
    previous = current.get_path_name().split(".")[0] if current else ""
    if not levels.new_level(path):
        raise RuntimeError("无法创建测试地图；请先保存当前关卡。")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor.get_editor_world()
    settings = world.get_world_settings()
    settings.set_editor_property("default_gameplay_experience", experience_class)
    settings.set_editor_property("default_game_mode", require_class("/Game/B_LyraGameMode.B_LyraGameMode_C"))
    settings.set_editor_property("force_standalone_net_mode", True)
    add_block(actors, "地面", (0, 0, -25), (40, 40, 0.5))
    add_block(actors, "遮挡测试墙", (750, 380, 150), (0.3, 4, 3))
    add_block(actors, "相机碰撞墙", (-600, 0, 150), (0.3, 12, 3))
    start = actors.spawn_actor_from_class(require_class("/Script/LyraGame.LyraPlayerStart"), unreal.Vector(-300, 0, 100))
    start.set_actor_label("Souls 玩家出生点")
    for index, position in enumerate(((500, -260, 90), (600, 220, 90))):
        target = actors.spawn_actor_from_class(target_class, unreal.Vector(*position))
        target.set_actor_label("锁定测试目标 {}".format(index + 1))
    light = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 500))
    light.set_actor_rotation(unreal.Rotator(-45, -30, 0), False)
    actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400))
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    if not levels.save_current_level():
        raise RuntimeError("无法保存 Souls 测试关卡。")
    if previous and unreal.EditorAssetLibrary.does_asset_exist(previous):
        levels.load_level(previous)


# 生成插件入口、Experience、角色、HUD 配置与测试关卡。
def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, "Module Load SoulsCoreRuntime")
    character_class = require_class("/Script/SoulsCoreRuntime.SoulsCharacter")
    require_class("/Script/SoulsCoreRuntime.SoulsSetupLibrary")
    if not unreal.SoulsSetupLibrary.prepare_content_for_setup():
        raise RuntimeError("无法挂载 SoulsCore，请确认插件已启用。")
    pawn_data_class = require_class("/Script/SoulsCoreRuntime.SoulsPawnData")
    input_class = require_class("/Script/SoulsCoreRuntime.SoulsInputConfig")
    hud_class = require_class("/Script/SoulsCoreRuntime.SoulsHUDWidget")
    target_class = require_class("/Script/SoulsCoreRuntime.SoulsTrainingTarget")
    experience_base = require_class("/Script/LyraGame.LyraExperienceDefinition")
    action_set_class = require_class("/Script/LyraGame.LyraExperienceActionSet")
    ability_set_class = require_class("/Script/LyraGame.LyraAbilitySet")
    death_ability_class = require_class("/Game/Characters/Heroes/Abilities/GA_Hero_Death.GA_Hero_Death_C")
    feature_class = require_class("/Script/GameFeatures.GameFeatureData")
    widget_action_class = require_class("/Script/LyraGame.GameFeatureAction_AddWidgets")

    feature, new = create_asset("/SoulsCore/SoulsCore", feature_class)
    if new:
        feature.set_editor_property("primary_asset_types_to_scan", [
            scan_rule("LyraExperienceDefinition", experience_base, "/SoulsCore/Experiences", True),
            scan_rule("LyraExperienceActionSet", action_set_class, "/SoulsCore/Experiences"),
            scan_rule("Map", require_class("/Script/Engine.World"), "/SoulsCore/Maps")])
        save(feature)

    config, new = create_asset("/SoulsCore/Input/InputConfig_Souls", input_class)
    if new:
        save(config)
    hero, new = create_blueprint("/SoulsCore/Characters/B_Hero_Souls", character_class)
    hero_class = unreal.load_class(None, "/SoulsCore/Characters/B_Hero_Souls.B_Hero_Souls_C")
    if new:
        cdo = unreal.get_default_object(hero_class)
        mesh = cdo.get_editor_property("mesh")
        mesh.set_skeletal_mesh_asset(unreal.load_asset("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny"))
        mesh.set_anim_instance_class(require_class("/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base_C"))
        cdo.set_editor_property("unarmed_animation_layer", require_class(
            "/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Unarmed/ABP_UnarmedAnimLayers.ABP_UnarmedAnimLayers_C"))
        save(hero)

    abilities, new = create_asset("/SoulsCore/Characters/AbilitySet_Souls", ability_set_class)
    if new:
        death = unreal.LyraAbilitySet_GameplayAbility()
        death.set_editor_property("ability", death_ability_class)
        abilities.set_editor_property("granted_gameplay_abilities", [death])
        save(abilities)

    pawn_data, new = create_asset("/SoulsCore/Characters/HeroData_Souls", pawn_data_class)
    if new:
        pawn_data.set_editor_property("pawn_class", hero_class)
        pawn_data.set_editor_property("input_config", config)
        pawn_data.set_editor_property("ability_sets", [abilities])
        save(pawn_data)

    hud_set, new = create_asset("/SoulsCore/Experiences/LAS_Souls_HUD", action_set_class)
    if new:
        action = unreal.new_object(widget_action_class, outer=hud_set)
        layout = unreal.LyraHUDLayoutRequest()
        layout.set_editor_property("layout_class", hud_class)
        layer = unreal.GameplayTag()
        if not layer.import_text('(TagName="UI.Layer.Game")'):
            raise RuntimeError("无法创建 HUD Layer 标签。")
        layout.set_editor_property("layer_id", layer)
        action.set_editor_property("layout", [layout])
        hud_set.set_editor_property("actions", [action])
        save(hud_set)

    experience, new = create_blueprint("/SoulsCore/Experiences/B_SoulsExperience", experience_base)
    experience_class = unreal.load_class(None, "/SoulsCore/Experiences/B_SoulsExperience.B_SoulsExperience_C")
    if new:
        cdo = unreal.get_default_object(experience_class)
        cdo.set_editor_property("game_features_to_enable", ["SoulsCore"])
        cdo.set_editor_property("default_pawn_data", pawn_data)
        cdo.set_editor_property("action_sets", [hud_set])
        save(experience)

    create_test_map(experience_class, target_class)
    unreal.log("SoulsCore 资产准备完成。打开 /SoulsCore/Maps/L_Souls_Test 后以 Standalone 运行。")


if __name__ == "__main__":
    main()
