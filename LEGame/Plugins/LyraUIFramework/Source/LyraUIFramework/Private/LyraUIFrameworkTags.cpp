#include "LyraUIFrameworkTags.h"

namespace LyraUIFrameworkTags
{
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Scene, "UI.Layer.Scene", "World Space 场景界面");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Background, "UI.Layer.Background", "背景与黑边");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Normal, "UI.Layer.Normal", "普通界面");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Info, "UI.Layer.Info", "信息界面");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Top, "UI.Layer.Top", "加载等顶层界面");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tip, "UI.Layer.Tip", "提示界面");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modal, "UI.Layer.Modal", "模态确认框");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Game, "UI.Layer.Game", "Lyra HUD 兼容层");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameMenu, "UI.Layer.GameMenu", "Lyra 游戏菜单兼容层");
    UE_DEFINE_GAMEPLAY_TAG_COMMENT(Menu, "UI.Layer.Menu", "Lyra 前端菜单兼容层");
}
