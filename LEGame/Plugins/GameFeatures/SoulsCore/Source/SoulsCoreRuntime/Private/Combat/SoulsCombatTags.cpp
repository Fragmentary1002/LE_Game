#include "NativeGameplayTags.h"

// 在玩法模块加载时注册战斗标签，供能力默认对象和输入资产查询。
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Input_Souls_LightAttack, "InputTag.Souls.LightAttack");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Souls_LightAttack, "Ability.Souls.LightAttack");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Souls_HitReact, "Ability.Souls.HitReact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Souls_Death, "Ability.Souls.Death");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Souls_Attacking, "State.Souls.Attacking");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Souls_HitReact, "State.Souls.HitReact");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Event_Souls_HitReact, "Event.Souls.HitReact");
