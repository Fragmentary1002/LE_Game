#include "SoulsEditorSetupLibrary.h"

#include "AI/SoulsEnemyBehavior.h"
#include "GameModes/SoulsGameMode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeGraph.h"
#include "Blueprint/WidgetTree.h"
#include "EdGraph/EdGraphSchema.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "UI/SoulsHUDWidget.h"
#include "WidgetBlueprint.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsEditorSetupLibrary)

// 创建名称与原生 BindWidgetOptional 字段一致的血条、文字和操作提示。
bool USoulsEditorSetupLibrary::BuildHUDWidgetTree(UWidgetBlueprint* Blueprint)
{
    if (!Blueprint || !Blueprint->ParentClass || !Blueprint->ParentClass->IsChildOf(USoulsHUDWidget::StaticClass()))
    {
        return false;
    }
    Blueprint->Modify();
    UWidgetTree* Tree = Blueprint->WidgetTree;
    if (!Tree)
    {
        return false;
    }
    Tree->Modify();
    UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
    Tree->RootWidget = Root;

    // 统一设置画布位置，确保设计器与运行时使用同一套布局。
    const auto Place = [Root](UWidget* Widget, FVector2D Position, FVector2D Size, FVector2D Anchor)
    {
        UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
        Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
        Slot->SetAlignment(FVector2D(Anchor.X == 0.5f ? 0.5f : 0.0f, 0.0f));
    };
    UTextBlock* PlayerText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayerHealthText"));
    PlayerText->SetText(NSLOCTEXT("Souls", "HealthPreview", "生命 100 / 100"));
    Place(PlayerText, FVector2D(48, 32), FVector2D(340, 32), FVector2D::ZeroVector);
    UProgressBar* PlayerBar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerHealthBar"));
    PlayerBar->SetPercent(1.0f);
    PlayerBar->SetFillColorAndOpacity(FLinearColor(0.6f, 0.03f, 0.03f));
    Place(PlayerBar, FVector2D(48, 72), FVector2D(340, 18), FVector2D::ZeroVector);
    UTextBlock* TargetText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TargetText"));
    TargetText->SetJustification(ETextJustify::Center);
    TargetText->SetVisibility(ESlateVisibility::Collapsed);
    Place(TargetText, FVector2D(0, -36), FVector2D(400, 32), FVector2D(0.5f, 0.85f));
    UProgressBar* TargetBar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TargetHealthBar"));
    TargetBar->SetFillColorAndOpacity(FLinearColor(0.7f, 0.15f, 0.04f));
    TargetBar->SetVisibility(ESlateVisibility::Collapsed);
    Place(TargetBar, FVector2D::ZeroVector, FVector2D(400, 12), FVector2D(0.5f, 0.85f));
    UTextBlock* Hints = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InputHints"));
    Hints->SetText(NSLOCTEXT("Souls", "InputHints", "WASD 移动 | 鼠标 观察 | 中键 锁定 | Q / E 切换目标\n手柄：左摇杆移动，右摇杆观察/切换，按下右摇杆锁定"));
    Place(Hints, FVector2D(48, -96), FVector2D(1000, 64), FVector2D(0, 1));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return true;
}

// 使用编辑器标准编译入口并检查最终状态。
bool USoulsEditorSetupLibrary::CompileBlueprintForSetup(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return Blueprint->GeneratedClass && (Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
}

// 通过反射填写未导出给 Python 的 FLyraHUDLayoutRequest，不修改 Lyra 的原生接口。
UObject* USoulsEditorSetupLibrary::CreateHUDAction(UObject* ActionSet, TSubclassOf<UUserWidget> LayoutClass)
{
    UClass* ActionClass = LoadClass<UObject>(nullptr, TEXT("/Script/LyraGame.GameFeatureAction_AddWidgets"));
    if (!ActionSet || !LayoutClass || !ActionClass || !LayoutClass->IsChildOf(USoulsHUDWidget::StaticClass()))
    {
        return nullptr;
    }
    UObject* Action = NewObject<UObject>(ActionSet, ActionClass, NAME_None, RF_Transactional);
    FArrayProperty* Layout = FindFProperty<FArrayProperty>(ActionClass, TEXT("Layout"));
    const FString Value = FString::Printf(TEXT("((LayoutClass=\"%s\",LayerID=(TagName=\"UI.Layer.Game\")))"), *LayoutClass->GetPathName());
    if (!Layout || !Layout->ImportText_Direct(*Value, Layout->ContainerPtrToValuePtr<void>(Action), Action, PPF_None))
    {
        return nullptr;
    }
    return Action;
}

// 绕过 Python 对 EditDefaultsOnly 实例的写入限制，设置地图专属的玩法入口。
bool USoulsEditorSetupLibrary::ConfigureMap(UWorld* World, TSubclassOf<UObject> ExperienceClass)
{
    AWorldSettings* Settings = World ? World->GetWorldSettings() : nullptr;
    UClass* ExperienceBase = LoadClass<UObject>(nullptr, TEXT("/Script/LyraGame.LyraExperienceDefinition"));
    UClass* GameMode = ASoulsGameMode::StaticClass();
    if (!Settings || !ExperienceClass || !ExperienceBase || !ExperienceClass->IsChildOf(ExperienceBase) || !GameMode)
    {
        return false;
    }
    FSoftClassProperty* Experience = FindFProperty<FSoftClassProperty>(Settings->GetClass(), TEXT("DefaultGameplayExperience"));
    FBoolProperty* Standalone = FindFProperty<FBoolProperty>(Settings->GetClass(), TEXT("ForceStandaloneNetMode"));
    if (!Experience || !Standalone)
    {
        return false;
    }
    Settings->Modify();
    const FString Value = FString::Printf(TEXT("\"%s\""), *ExperienceClass->GetPathName());
    if (!Experience->ImportText_Direct(*Value, Experience->ContainerPtrToValuePtr<void>(Settings), Settings, PPF_None))
    {
        return false;
    }
    Settings->DefaultGameMode = GameMode;
    Standalone->SetPropertyValue_InContainer(Settings, true);
    Settings->MarkPackageDirty();
    return true;
}

// 为脚本创建可编辑的树图，保留已有的行为树与黑板配置。
bool USoulsEditorSetupLibrary::BuildEnemyBehaviorAssets(UBehaviorTree* Tree, UBlackboardData* Blackboard)
{
    if (!Tree || !Blackboard)
    {
        return false;
    }
    if (Tree->RootNode && Tree->BTGraph)
    {
        return Tree->BlackboardAsset == Blackboard && SoulsEnemyBlackboard::HasRequiredKeys(*Blackboard);
    }
    if (Tree->BTGraph || (!Tree->RootNode && !SoulsEnemyBlackboard::BuildDefaultTree(*Tree, *Blackboard)) ||
        Tree->BlackboardAsset != Blackboard)
    {
        return false;
    }

    UBehaviorTreeGraph* Graph = CastChecked<UBehaviorTreeGraph>(FBlueprintEditorUtils::CreateNewGraph(
        Tree, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), GetDefault<UBehaviorTreeGraph>()->Schema));
    Tree->BTGraph = Graph;
    Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
    // 导入完成前暂停图表更新，避免自动清理尚未挂到图上的分支条件。
    Graph->LockUpdates();
    Graph->OnCreated();
    Graph->Initialize();
    Graph->UnlockUpdates();
    Graph->UpdateAsset();
    Blackboard->MarkPackageDirty();
    Tree->MarkPackageDirty();
    return Tree->RootNode != nullptr;
}

// 直接构建导航，兼容交互编辑器和无窗口资产生成命令。
bool USoulsEditorSetupLibrary::BuildMapNavigation(UWorld* World)
{
    UNavigationSystemV1* Navigation = World ? UNavigationSystemV1::GetCurrent(World) : nullptr;
    if (!Navigation)
    {
        return false;
    }
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
    {
        Navigation->OnNavigationBoundsUpdated(*It);
    }
    // 命令行资产生成没有常规编辑器帧，先处理新体积和场景物体的导航更新。
    Navigation->Tick(0.0f);
    Navigation->Build();
    World->MarkPackageDirty();
    return Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
}
