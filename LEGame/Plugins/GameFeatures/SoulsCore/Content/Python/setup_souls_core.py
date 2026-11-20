"""通过 UE5.4 编辑器生成 SoulsCore 蓝图、数据配置和 EmptySoulsMap。"""

import unreal

ROOT = "/SoulsCore"
MAP_PATH = ROOT + "/Maps/EmptySoulsMap"
MAP_TEMPLATE = "/Game/StarterContent/Maps/Minimal_Default"
SETUP_VERSION = "2"
SETUP_TAG = "SoulsCore.SetupVersion"
COMBAT_VERSION = "1"
COMBAT_TAG = "SoulsCore.CombatVersion"


# 加载必要的反射类型，类型缺失时立即停止。
def require_class(path):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError("缺少类型：{}".format(path))
    return cls


# 加载引用资产，避免把空引用写入新配置。
def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("缺少资产：{}".format(path))
    return asset


# 创建数据资产，只有未完成初始化的资产才需要配置。
def create_asset(path, cls):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = require_asset(path)
        if asset.get_class() != cls:
            raise RuntimeError("资产类型不匹配：{}".format(path))
    else:
        package, name = path.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(package)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, cls, factory)
        if not asset:
            raise RuntimeError("无法创建资产：{}".format(path))
    pending = unreal.EditorAssetLibrary.get_metadata_tag(asset, SETUP_TAG) != SETUP_VERSION
    return asset, pending


# 创建角色、相机或 Widget 蓝图，重复执行时保留已完成资产的调整。
def create_blueprint(path, parent, widget=False):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        blueprint = require_asset(path)
        if not isinstance(unreal.get_default_object(blueprint_class(blueprint)), unreal.get_type_from_class(parent)):
            raise RuntimeError("蓝图父类不匹配：{}".format(path))
    else:
        package, name = path.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(package)
        factory = unreal.WidgetBlueprintFactory() if widget else unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        cls = unreal.WidgetBlueprint if widget else unreal.Blueprint
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, cls, factory)
        if not blueprint:
            raise RuntimeError("无法创建蓝图：{}".format(path))
    pending = unreal.EditorAssetLibrary.get_metadata_tag(blueprint, SETUP_TAG) != SETUP_VERSION
    return blueprint, pending


# 取得蓝图生成类，统一检查编译后的类是否存在。
def blueprint_class(blueprint):
    path = blueprint.get_path_name() + "_C"
    return require_class(path)


# 保存并标记已完成配置的资产，失败后允许重试初始化。
def save(asset, blueprint=False):
    if blueprint:
        if not unreal.SoulsEditorSetupLibrary.compile_blueprint_for_setup(asset):
            raise RuntimeError("蓝图编译失败：{}".format(asset.get_path_name()))
    unreal.EditorAssetLibrary.set_metadata_tag(asset, SETUP_TAG, SETUP_VERSION)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, False):
        raise RuntimeError("无法保存：{}".format(asset.get_path_name()))


# 构造 GameFeatureData 的主资产扫描规则。
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


# 将默认内置动作和映射拆成可在内容浏览器中独立编辑的输入资产。
def create_input_config():
    config, pending = create_asset(ROOT + "/Input/InputConfig_Souls", require_class("/Script/SoulsCoreRuntime.SoulsInputConfig"))
    if not pending:
        return config
    source = unreal.get_default_object(config.get_class())
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    actions = []
    names = {}
    for entry in source.get_editor_property("native_input_actions"):
        original = entry.get_editor_property("input_action")
        name = original.get_name() + "_Souls"
        path = ROOT + "/Input/Actions/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            action = require_asset(path)
        else:
            unreal.EditorAssetLibrary.make_directory(ROOT + "/Input/Actions")
            action = tools.duplicate_asset(name, ROOT + "/Input/Actions", original)
            if not action:
                raise RuntimeError("无法复制输入动作：{}".format(name))
            save(action)
        names[original.get_path_name()] = action
        configured = unreal.LyraInputAction()
        if not configured.import_text('(InputAction="{}",InputTag={})'.format(
                action.get_path_name(), entry.get_editor_property("input_tag").export_text())):
            raise RuntimeError("无法配置输入动作：{}".format(name))
        actions.append(configured)
    path = ROOT + "/Input/IMC_Souls"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mapping = require_asset(path)
    else:
        mapping = tools.duplicate_asset("IMC_Souls", ROOT + "/Input", source.get_editor_property("mapping_context"))
        if not mapping:
            raise RuntimeError("无法创建 IMC_Souls")
        entries = list(mapping.get_editor_property("mappings"))
        for entry in entries:
            original = entry.get_editor_property("action")
            entry.set_editor_property("action", names[original.get_path_name()])
        mapping.set_editor_property("mappings", entries)
        save(mapping)
    config.set_editor_property("native_input_actions", actions)
    config.set_editor_property("mapping_context", mapping)
    save(config)
    return config


# 在 Widget 蓝图中放置带名称绑定的原生控件，供设计器后续调整。
def create_hud():
    hud, pending = create_blueprint(ROOT + "/UI/W_SoulsHUD", require_class("/Script/SoulsCoreRuntime.SoulsHUDWidget"), True)
    if pending:
        if not unreal.SoulsEditorSetupLibrary.build_hud_widget_tree(hud):
            raise RuntimeError("无法创建 HUD 蓝图控件树")
        save(hud, True)
    return blueprint_class(hud)


# 复制轻量模板并清理桌椅，建立用于出生、锁定和相机遮挡验证的关卡。
def create_test_map(experience_class, target_class):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        existing = require_asset(MAP_PATH)
        if unreal.EditorAssetLibrary.get_metadata_tag(existing, SETUP_TAG) == SETUP_VERSION:
            return
        if not levels.load_level(MAP_PATH):
            raise RuntimeError("无法加载未完成配置的 EmptySoulsMap")
    elif not levels.new_level_from_template(MAP_PATH, MAP_TEMPLATE):
        raise RuntimeError("无法从 Minimal_Default 复制 EmptySoulsMap")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not unreal.SoulsEditorSetupLibrary.configure_map(world, experience_class):
        raise RuntimeError("无法配置 EmptySoulsMap 的玩法入口")
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.PlayerStart) or actor.get_actor_label().startswith("Souls_"):
            actors.destroy_actor(actor)
        elif isinstance(actor, unreal.StaticMeshActor):
            mesh = actor.static_mesh_component.get_editor_property("static_mesh")
            if mesh and any(word in mesh.get_name().lower() for word in ("chair", "table", "statue")):
                actors.destroy_actor(actor)
    floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -25))
    floor.set_actor_label("Souls_TestFloor")
    floor.static_mesh_component.set_static_mesh(require_asset("/Engine/BasicShapes/Cube"))
    floor.static_mesh_component.set_collision_profile_name("BlockAll")
    floor.set_actor_scale3d(unreal.Vector(40, 40, 0.5))
    start = actors.spawn_actor_from_class(require_class("/Script/LyraGame.LyraPlayerStart"), unreal.Vector(-300, 0, 100))
    start.set_actor_label("Souls_PlayerStart")
    for index, position in enumerate(((500, -260, 90), (600, 220, 90))):
        target = actors.spawn_actor_from_class(target_class, unreal.Vector(*position))
        target.set_actor_label("Souls_TrainingTarget_{}".format(index + 1))
    wall = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(-650, 0, 150))
    wall.set_actor_label("Souls_CameraCollisionWall")
    wall.static_mesh_component.set_static_mesh(require_asset("/Engine/BasicShapes/Cube"))
    wall.static_mesh_component.set_collision_profile_name("BlockAll")
    wall.set_actor_scale3d(unreal.Vector(0.3, 12, 3))
    unreal.EditorAssetLibrary.set_metadata_tag(world, SETUP_TAG, SETUP_VERSION)
    if not levels.save_current_level():
        raise RuntimeError("无法保存 EmptySoulsMap")


# 复制可编辑的战斗资源，完成迁移后保留设计师后续调整。
def duplicate_combat_asset(path, source_path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = require_asset(path)
    else:
        package, name = path.rsplit("/", 1)
        unreal.EditorAssetLibrary.make_directory(package)
        asset = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, package, require_asset(source_path))
        if not asset:
            raise RuntimeError("无法复制战斗资产：" + path)
    return asset, unreal.EditorAssetLibrary.get_metadata_tag(asset, COMBAT_TAG) != COMBAT_VERSION


# 单独记录战斗迁移版本，避免重跑基础生成时覆盖已有角色和地图。
def save_combat(asset, blueprint=False):
    unreal.EditorAssetLibrary.set_metadata_tag(asset, COMBAT_TAG, COMBAT_VERSION)
    save(asset, blueprint)


# 按反射结构解析能力输入和能力集条目，避免依赖只读 Python 属性。
def import_struct(cls, text):
    result = cls()
    if not result.import_text(text):
        raise RuntimeError("无法解析战斗配置：" + text)
    return result


# 在原输入配置上增量添加轻攻击，保留所有既有原生动作和用户映射。
def migrate_combat_input(config):
    action, pending = duplicate_combat_asset(ROOT + "/Input/Actions/IA_Souls_LightAttack", "/Game/Input/Actions/IA_Weapon_Fire")
    if pending:
        action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
        action.set_editor_property("triggers", [])
        action.set_editor_property("modifiers", [])
        save_combat(action)
    if unreal.EditorAssetLibrary.get_metadata_tag(config, COMBAT_TAG) == COMBAT_VERSION:
        return
    entries = list(config.get_editor_property("ability_input_actions"))
    tag = '(TagName="InputTag.Souls.LightAttack")'
    if not any("InputTag.Souls.LightAttack" in entry.get_editor_property("input_tag").export_text() for entry in entries):
        entries.append(import_struct(unreal.LyraInputAction,
            '(InputAction="{}",InputTag={})'.format(action.get_path_name(), tag)))
        config.set_editor_property("ability_input_actions", entries)
    mapping = config.get_editor_property("mapping_context")
    changed = False
    for name in ("LeftMouseButton", "Gamepad_RightShoulder"):
        if not any(entry.get_editor_property("action") == action and
                   entry.get_editor_property("key").export_text() == name
                   for entry in mapping.get_editor_property("mappings")):
            key = import_struct(unreal.Key, name)
            mapping.map_key(action, key)
            changed = True
    if changed:
        save_combat(mapping)
    save_combat(config)


# 配置新的战斗能力蓝图，只在首次迁移写入动作引用。
def create_combat_ability(name, parent, properties):
    blueprint, _ = create_blueprint(ROOT + "/Combat/Abilities/" + name,
        require_class("/Script/SoulsCoreRuntime." + parent))
    if unreal.EditorAssetLibrary.get_metadata_tag(blueprint, COMBAT_TAG) != COMBAT_VERSION:
        cdo = unreal.get_default_object(blueprint_class(blueprint))
        for key, value in properties.items():
            cdo.set_editor_property(key, value)
        save_combat(blueprint, True)
    return blueprint_class(blueprint)


# 合并能力和属性初始化，不清除用户追加的能力、效果或属性集。
def migrate_combat_ability_set(abilities, attack_class, hit_class, death_class, init_class):
    if unreal.EditorAssetLibrary.get_metadata_tag(abilities, COMBAT_TAG) == COMBAT_VERSION:
        return
    entries = []
    old_death = "/Game/Characters/Heroes/Abilities/GA_Hero_Death.GA_Hero_Death_C"
    for entry in abilities.get_editor_property("granted_gameplay_abilities"):
        ability = entry.get_editor_property("ability")
        if not ability or ability.get_path_name() != old_death:
            entries.append(entry)
    for ability, input_tag in ((attack_class, "InputTag.Souls.LightAttack"), (hit_class, ""), (death_class, "")):
        if not any(entry.get_editor_property("ability") == ability for entry in entries):
            entries.append(import_struct(unreal.LyraAbilitySet_GameplayAbility,
                '(Ability="{}",AbilityLevel=1,InputTag=(TagName="{}"))'.format(ability.get_path_name(), input_tag)))
    abilities.set_editor_property("granted_gameplay_abilities", entries)
    attributes = list(abilities.get_editor_property("granted_attributes"))
    combat_set = require_class("/Script/LyraGame.LyraCombatSet")
    if not any(entry.get_editor_property("attribute_set") == combat_set for entry in attributes):
        attributes.append(import_struct(unreal.LyraAbilitySet_AttributeSet,
            '(AttributeSet="{}")'.format(combat_set.get_path_name())))
        abilities.set_editor_property("granted_attributes", attributes)
    effects = list(abilities.get_editor_property("granted_gameplay_effects"))
    if not any(entry.get_editor_property("gameplay_effect") == init_class for entry in effects):
        effects.append(import_struct(unreal.LyraAbilitySet_GameplayEffect,
            '(GameplayEffect="{}",EffectLevel=1)'.format(init_class.get_path_name())))
        abilities.set_editor_property("granted_gameplay_effects", effects)
    save_combat(abilities)


# 在原关卡中增量布置骨骼敌人和遮挡验证区，不清理任何已有场景对象。
def migrate_combat_map(dummy_class):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.load_level(MAP_PATH):
        raise RuntimeError("无法加载战斗测试关卡")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if unreal.EditorAssetLibrary.get_metadata_tag(world, COMBAT_TAG) == COMBAT_VERSION:
        return
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    labels = {actor.get_actor_label() for actor in actors.get_all_level_actors()}
    for name, position in (("Souls_CombatDummy_1", (150, 1000, 100)),
                           ("Souls_CombatDummy_2", (350, 1000, 100)),
                           ("Souls_CombatDummy_BehindWall", (350, 1360, 100))):
        if name not in labels:
            dummy = actors.spawn_actor_from_class(dummy_class, unreal.Vector(*position), unreal.Rotator(0, 180, 0))
            dummy.set_actor_label(name)
    if "Souls_MeleeOcclusionWall" not in labels:
        wall = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(350, 1280, 100))
        wall.set_actor_label("Souls_MeleeOcclusionWall")
        wall.static_mesh_component.set_static_mesh(require_asset("/Engine/BasicShapes/Cube"))
        wall.static_mesh_component.set_collision_profile_name("BlockAll")
        wall.set_actor_scale3d(unreal.Vector(4, 0.25, 2))
    unreal.EditorAssetLibrary.set_metadata_tag(world, COMBAT_TAG, COMBAT_VERSION)
    if not levels.save_current_level():
        raise RuntimeError("无法保存战斗地图迁移")


# 创建近战资产并显式迁移已有角色、输入和能力集，重复执行时保留用户改动。
def create_combat_content(config, hero, abilities):
    sequence, sequence_pending = duplicate_combat_asset(ROOT + "/Combat/Animations/AS_Souls_LightAttack",
        "/Game/Characters/Heroes/Mannequin/Animations/Actions/MM_Rifle_Melee")
    montage, montage_pending = duplicate_combat_asset(ROOT + "/Combat/Animations/AM_Souls_LightAttack",
        "/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_Rifle_Melee")
    if sequence_pending or montage_pending:
        if not unreal.SoulsCombatEditorLibrary.configure_attack_montage(montage, sequence):
            raise RuntimeError("无法配置近战动画窗口")
        save_combat(sequence)
        save_combat(montage)
    damage, _ = create_blueprint(ROOT + "/Combat/Effects/GE_Souls_MeleeDamage", require_class("/Script/GameplayAbilities.GameplayEffect"))
    init, _ = create_blueprint(ROOT + "/Combat/Effects/GE_Souls_CombatInit", require_class("/Script/GameplayAbilities.GameplayEffect"))
    for blueprint, is_damage in ((damage, True), (init, False)):
        if unreal.EditorAssetLibrary.get_metadata_tag(blueprint, COMBAT_TAG) != COMBAT_VERSION:
            if not unreal.SoulsCombatEditorLibrary.configure_combat_effect(blueprint, is_damage, 25.0):
                raise RuntimeError("无法配置战斗效果：" + blueprint.get_path_name())
            save_combat(blueprint, True)
    definition, _ = create_asset(ROOT + "/Combat/DA_Souls_LightAttack", require_class("/Script/SoulsCoreRuntime.SoulsAttackDefinition"))
    if unreal.EditorAssetLibrary.get_metadata_tag(definition, COMBAT_TAG) != COMBAT_VERSION:
        definition.set_editor_property("attack_montage", montage)
        definition.set_editor_property("damage_effect_class", blueprint_class(damage))
        save_combat(definition)
    weapon, _ = create_blueprint(ROOT + "/Combat/Weapons/B_SoulsWeapon_TestSword", require_class("/Script/SoulsCoreRuntime.SoulsMeleeWeapon"))
    if unreal.EditorAssetLibrary.get_metadata_tag(weapon, COMBAT_TAG) != COMBAT_VERSION:
        save_combat(weapon, True)
    attack_class = create_combat_ability("GA_Souls_LightAttack", "SoulsGameplayAbility_LightAttack", {})
    hit_class = create_combat_ability("GA_Souls_HitReact", "SoulsGameplayAbility_HitReact", {
        "front_montage": require_asset("/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_HitReact_Front_Lgt_01"),
        "back_montage": require_asset("/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_HitReact_Back_Lgt_01"),
        "left_montage": require_asset("/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_HitReact_Left_Lgt_01"),
        "right_montage": require_asset("/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_HitReact_Right_Lgt_01")})
    death_class = create_combat_ability("GA_Souls_Death", "SoulsGameplayAbility_Death", {
        "death_montage": require_asset("/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_Death_Front_01")})
    migrate_combat_input(config)
    migrate_combat_ability_set(abilities, attack_class, hit_class, death_class, blueprint_class(init))
    if unreal.EditorAssetLibrary.get_metadata_tag(hero, COMBAT_TAG) != COMBAT_VERSION:
        cdo = unreal.get_default_object(blueprint_class(hero))
        trace = cdo.get_component_by_class(unreal.SoulsMeleeTraceComponent)
        if not trace:
            raise RuntimeError("Souls 角色缺少武器轨迹组件")
        if not trace.get_editor_property("default_attack_definition"):
            trace.set_editor_property("default_attack_definition", definition)
        trace.set_editor_property("weapon_class", blueprint_class(weapon))
        save_combat(hero, True)
    dummy, _ = create_blueprint(ROOT + "/Development/B_SoulsCombatDummy", require_class("/Script/SoulsCoreRuntime.SoulsCombatDummy"))
    if unreal.EditorAssetLibrary.get_metadata_tag(dummy, COMBAT_TAG) != COMBAT_VERSION:
        cdo = unreal.get_default_object(blueprint_class(dummy))
        cdo.set_editor_property("combat_abilities", [hit_class, death_class])
        cdo.set_editor_property("team_id", 2)
        cdo.set_editor_property("initial_health", 100.0)
        mesh = cdo.get_editor_property("mesh")
        mesh.set_skeletal_mesh_asset(require_asset("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny"))
        mesh.set_relative_location(unreal.Vector(0, 0, -90), False, False)
        mesh.set_relative_rotation(unreal.Rotator(0, -90, 0), False, False)
        mesh.set_anim_instance_class(require_class("/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base_C"))
        cdo.set_editor_property("unarmed_animation_layer", require_class(
            "/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Unarmed/ABP_UnarmedAnimLayers.ABP_UnarmedAnimLayers_C"))
        save_combat(dummy, True)
    migrate_combat_map(blueprint_class(dummy))
    unreal.log("SOULS_COMBAT_SETUP_COMPLETE: " + MAP_PATH)


# 按依赖顺序创建并连接插件入口、角色、相机、HUD、Experience 和地图。
def main():
    unreal.load_module("SoulsCoreRuntime")
    unreal.load_module("SoulsCoreEditor")
    require_class("/Script/SoulsCoreRuntime.SoulsSetupLibrary")
    if not unreal.SoulsSetupLibrary.prepare_content_for_setup():
        raise RuntimeError("无法挂载 SoulsCore 内容目录")
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT], True)
    experience_base = require_class("/Script/LyraGame.LyraExperienceDefinition")
    action_set_class = require_class("/Script/LyraGame.LyraExperienceActionSet")
    feature, pending = create_asset(ROOT + "/SoulsCore", require_class("/Script/GameFeatures.GameFeatureData"))
    if pending:
        feature.set_editor_property("primary_asset_types_to_scan", [
            scan_rule("LyraExperienceDefinition", experience_base, ROOT + "/Experiences", True),
            scan_rule("LyraExperienceActionSet", action_set_class, ROOT + "/Experiences"),
            scan_rule("Map", require_class("/Script/Engine.World"), ROOT + "/Maps")])
        save(feature)
    config = create_input_config()
    cameras = []
    for name, parent in (("CM_Souls_Free", "SoulsCameraMode"), ("CM_Souls_LockOn", "SoulsCameraMode_LockOn")):
        camera, pending = create_blueprint(ROOT + "/Camera/" + name, require_class("/Script/SoulsCoreRuntime." + parent))
        if pending:
            save(camera, True)
        cameras.append(blueprint_class(camera))
    hero, pending = create_blueprint(ROOT + "/Characters/B_Hero_Souls", require_class("/Script/SoulsCoreRuntime.SoulsCharacter"))
    if pending:
        cdo = unreal.get_default_object(blueprint_class(hero))
        mesh = cdo.get_editor_property("mesh")
        mesh.set_skeletal_mesh_asset(require_asset("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny"))
        mesh.set_relative_location(unreal.Vector(0, 0, -90), False, False)
        mesh.set_anim_instance_class(require_class("/Game/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.ABP_Mannequin_Base_C"))
        cdo.set_editor_property("unarmed_animation_layer", require_class(
            "/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Unarmed/ABP_UnarmedAnimLayers.ABP_UnarmedAnimLayers_C"))
        save(hero, True)
    abilities, pending = create_asset(ROOT + "/Characters/AbilitySet_Souls", require_class("/Script/LyraGame.LyraAbilitySet"))
    if pending:
        death = unreal.LyraAbilitySet_GameplayAbility()
        death_class = require_class("/Game/Characters/Heroes/Abilities/GA_Hero_Death.GA_Hero_Death_C")
        if not death.import_text('(Ability="{}",AbilityLevel=1)'.format(death_class.get_path_name())):
            raise RuntimeError("无法配置死亡能力")
        abilities.set_editor_property("granted_gameplay_abilities", [death])
        save(abilities)
    pawn_data, pending = create_asset(ROOT + "/Characters/HeroData_Souls", require_class("/Script/SoulsCoreRuntime.SoulsPawnData"))
    if pending:
        pawn_data.set_editor_property("pawn_class", blueprint_class(hero))
        pawn_data.set_editor_property("input_config", config)
        pawn_data.set_editor_property("ability_sets", [abilities])
        pawn_data.set_editor_property("default_camera_mode", cameras[0])
        pawn_data.set_editor_property("lock_on_camera_mode", cameras[1])
        save(pawn_data)
    hud_class = create_hud()
    hud_set, pending = create_asset(ROOT + "/Experiences/LAS_Souls_HUD", action_set_class)
    if pending:
        action = unreal.SoulsEditorSetupLibrary.create_hud_action(hud_set, hud_class)
        if not action:
            raise RuntimeError("无法创建 HUD Action")
        hud_set.set_editor_property("actions", [action])
        save(hud_set)
    experience, pending = create_blueprint(ROOT + "/Experiences/B_SoulsExperience", experience_base)
    if pending:
        cdo = unreal.get_default_object(blueprint_class(experience))
        cdo.set_editor_property("game_features_to_enable", ["SoulsCore"])
        cdo.set_editor_property("default_pawn_data", pawn_data)
        cdo.set_editor_property("action_sets", [hud_set])
        save(experience, True)
    target, pending = create_blueprint(ROOT + "/Development/B_SoulsTrainingTarget", require_class("/Script/SoulsCoreRuntime.SoulsTrainingTarget"))
    if pending:
        save(target, True)
    create_test_map(blueprint_class(experience), blueprint_class(target))
    create_combat_content(config, hero, abilities)
    unreal.log("SOULS_SETUP_COMPLETE: " + MAP_PATH)


if __name__ == "__main__":
    main()
