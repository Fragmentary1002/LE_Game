import math
import unreal

BOSS_ROOT = "/SoulsCore/Enemies/Boss"
MAP_PATH = "/SoulsCore/Maps/L_SoulsBossTest"
TEMPLATE_MAP = "/SoulsCore/Maps/EmptySoulsMap"
LYRA_ACTIONS = "/Game/Characters/Heroes/Mannequin/Animations/Actions"
VERSION_TAG = "SoulsCore.BossSetupVersion"
VERSION = "2"
MAP_VERSION = "3"
# 招式、占位动画、原始挥击区间、目标前摇和后摇（秒）。
ATTACKS = (
    ("Sweep", "Rifle", 0.233, 0.350, 0.55, 0.45),
    ("Overhead", "Pistol", 0.307, 0.460, 0.90, 0.75),
    ("Thrust", "Shotgun", 0.117, 0.350, 0.75, 0.60),
)
ACTIVE_SECONDS = 0.20


# 加载必需资产，提前报告缺失引用。
def require_asset(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("缺少资产：" + path)
    return asset


# 首次创建时复制源资产，支持中途失败后的重试。
def duplicate(path, source):
    assets = unreal.EditorAssetLibrary
    asset = (require_asset(path) if assets.does_asset_exist(path)
             else assets.duplicate_asset(source, path))
    if not asset:
        raise RuntimeError("无法复制资产：" + path)
    return asset


# 保存资源并记录生成版本，避免重复执行覆盖后续手工调整。
def save(asset):
    assets = unreal.EditorAssetLibrary
    assets.set_metadata_tag(asset, VERSION_TAG, VERSION)
    if not assets.save_loaded_asset(asset, False):
        raise RuntimeError("无法保存：" + asset.get_path_name())


# 将目标时间映射到原动画的前摇、挥击和回收三段。
def map_time(time, segments):
    for (start, source_start), (end, source_end) in zip(segments, segments[1:]):
        if time <= end:
            return source_start + (source_end - source_start) * (time - start) / (end - start)
    return segments[-1][1]


# 生成独立序列，用分段重采样把唯一命中窗口对齐挥击。
def retime(sequence, source, source_start, source_end, windup, recovery):
    lib = unreal.AnimationLibrary
    original_length = source.get_play_length()
    target_length = windup + ACTIVE_SECONDS + recovery
    frames_per_second = 30
    num_frames = math.floor(target_length * frames_per_second + 0.5001)
    segments = ((0.0, 0.0), (windup, source_start), (windup + ACTIVE_SECONDS, source_end),
                (target_length, original_length))
    sample_times = [map_time(target_length * i / num_frames, segments)
                    for i in range(num_frames + 1)]
    controller = sequence.get_editor_property("controller")
    controller.open_bracket("生成重剑守卫占位动作", False)
    try:
        controller.set_frame_rate(unreal.FrameRate(frames_per_second, 1), False)
        controller.set_number_of_frames(unreal.FrameNumber(num_frames), False)
        bones = lib.get_animation_track_names(source)
        poses = [lib.get_bone_poses_for_time(source, bones, t, False) for t in sample_times]
        if any(len(pose) != len(bones) for pose in poses):
            raise RuntimeError("占位动画缺少骨骼姿势：" + source.get_name())
        for bone, track in zip(bones, zip(*poses)):
            positions = [pose.translation for pose in track]
            rotations = [pose.rotation for pose in track]
            scales = [pose.scale3d for pose in track]
            if not controller.set_bone_track_keys(bone, positions, rotations, scales, False):
                raise RuntimeError("骨骼轨道写入失败：" + str(bone))
    finally:
        controller.close_bracket(False)


# 创建并验证定义、FullBody Montage 和唯一有效窗口。
def create_attack(spec):
    name, style, source_start, source_end, windup, recovery = spec
    source_path = f"{LYRA_ACTIONS}/MM_{style}_Melee"
    sequence = duplicate(f"{BOSS_ROOT}/Animations/MM_SoulsBoss_{name}", source_path)
    definition = duplicate(f"{BOSS_ROOT}/DA_SoulsBoss_{name}", "/SoulsCore/Combat/DA_Souls_LightAttack")
    assets = unreal.EditorAssetLibrary
    lib = unreal.AnimationLibrary
    pending = assets.get_metadata_tag(definition, VERSION_TAG) != VERSION
    if pending:
        retime(sequence, require_asset(source_path), source_start, source_end, windup, recovery)
    montage_path = f"{BOSS_ROOT}/Animations/AM_SoulsBoss_{name}"
    if assets.does_asset_exist(montage_path):
        montage = require_asset(montage_path)
    else:
        folder, asset_name = montage_path.rsplit("/", 1)
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("source_animation", sequence)
        montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, folder, unreal.AnimMontage, factory)
        if not montage:
            raise RuntimeError("无法创建 Montage：" + montage_path)
    if pending:
        if not unreal.SoulsCombatEditorLibrary.configure_attack_montage(montage, sequence):
            raise RuntimeError("无法配置全身动作：" + name)
        lib.remove_animation_notify_events_by_name(montage, "SoulsMeleeWindow")
        track = lib.get_animation_notify_track_names(montage)[0]
        window = lib.add_animation_notify_state_event(montage, track, windup, ACTIVE_SECONDS,
                                                       unreal.AnimNotifyState_SoulsMeleeWindow)
        if not window:
            raise RuntimeError("无法创建命中窗口：" + name)
        definition.set_editor_property("attack_montage", montage)
        definition.set_editor_property("play_rate", 1.0)
        definition.set_editor_property("turn_duration", 0.0)
    windows = lib.get_animation_notify_events(montage)
    if (len(windows) != 1 or not isinstance(windows[0].get_editor_property("notify_state_class"),
                                          unreal.AnimNotifyState_SoulsMeleeWindow)
            or not math.isclose(lib.get_anim_notify_event_trigger_time(windows[0]), windup, abs_tol=0.001)
            or not math.isclose(lib.get_anim_notify_event_duration(windows[0]), ACTIVE_SECONDS, abs_tol=0.001)
            or not math.isclose(montage.get_play_length(), windup + ACTIVE_SECONDS + recovery, abs_tol=0.018)
            or definition.get_editor_property("attack_montage") != montage
            or not definition.get_editor_property("damage_effect_class")
            or lib.get_animation_notify_events(sequence)):
        raise RuntimeError("招式动画、命中窗口或伤害引用不匹配：" + name)
    if pending:
        save(sequence)
        save(montage)
        save(definition)
    unreal.log(f"SOULS_BOSS_ATTACK {name}: {montage.get_play_length():.3f}s, 窗口 {windup:.2f}s + {ACTIVE_SECONDS:.2f}s")
    return definition


# 复用剑兵蓝图基类、行为树和能力集，生成独立 Boss 出生配置。
def create_boss(sweep):
    data = duplicate(BOSS_ROOT + "/DA_SoulsBoss", "/SoulsCore/Enemies/DA_Souls_Swordsman")
    if unreal.EditorAssetLibrary.get_metadata_tag(data, VERSION_TAG) != VERSION:
        data.set_editor_property("max_health", 225.0)
        data.set_editor_property("attack_definition", sweep)
        save(data)
    return data


# 准备单人测试房，保留模板的玩法设置、地面、灯光和导航。
def create_map(data):
    assets = unreal.EditorAssetLibrary
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if assets.does_asset_exist(MAP_PATH):
        if not levels.load_level(MAP_PATH):
            raise RuntimeError("无法打开 Boss 测试关卡")
    elif not levels.new_level_from_template(MAP_PATH, TEMPLATE_MAP):
        raise RuntimeError("无法生成 Boss 测试关卡")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if assets.get_metadata_tag(world, VERSION_TAG) != MAP_VERSION:
        for actor in actors.get_all_level_actors():
            label = actor.get_actor_label()
            if (isinstance(actor, unreal.SoulsEnemy) or label.startswith("Souls_EnemySpawn_")
                    or label.startswith("Souls_TrainingTarget")):
                if not actors.destroy_actor(actor):
                    raise RuntimeError("无法移除模板测试目标：" + label)
        enemy = actors.spawn_actor_from_class(data.get_editor_property("pawn_class"), unreal.Vector(350, 0, 100),
                                              unreal.Rotator(yaw=180.0))
        if not enemy:
            raise RuntimeError("无法放置重剑守卫")
        enemy.set_actor_location(unreal.Vector(350, 0, 92), False, True)
        enemy.set_actor_label("Souls_GreatswordGuardian")
        enemy.set_editor_property("ai_pawn_data", data)
        if not unreal.SoulsEditorSetupLibrary.build_map_navigation(world):
            raise RuntimeError("无法构建测试房导航")
        assets.set_metadata_tag(world, VERSION_TAG, MAP_VERSION)
        if not levels.save_current_level():
            raise RuntimeError("无法保存 Boss 测试房")
    enemies = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.SoulsEnemy)]
    if len(enemies) != 1 or enemies[0].get_editor_property("ai_pawn_data") != data:
        raise RuntimeError("测试房的 Boss 配置引用不匹配")
    capsule = enemies[0].get_component_by_class(unreal.CapsuleComponent)
    feet = enemies[0].get_actor_location() - unreal.Vector(0, 0, capsule.get_scaled_capsule_half_height())
    if unreal.NavigationSystemV1.project_point_to_navigation(world, feet, None, None, unreal.Vector(100, 100, 150)) is None:
        raise RuntimeError("Boss 出生点附近没有导航网格")


# 生成资产后检查关卡引用和导航，输出可用于批处理的完成标记。
def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/SoulsCore"], True)
    unreal.EditorAssetLibrary.make_directory(BOSS_ROOT + "/Animations")
    definitions = [create_attack(attack) for attack in ATTACKS]
    data = create_boss(definitions[0])
    create_map(data)
    unreal.log("SOULS_BOSS_SETUP_COMPLETE: " + MAP_PATH)


if __name__ == "__main__":
    main()
