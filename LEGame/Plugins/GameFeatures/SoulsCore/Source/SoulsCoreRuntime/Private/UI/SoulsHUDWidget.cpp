#include "UI/SoulsHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Character/LyraHealthComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Input/CommonUIInputTypes.h"
#include "Input/UIActionBindingHandle.h"
#include "Rendering/DrawElements.h"
#include "Targeting/SoulsLockOnComponent.h"
#include "Targeting/SoulsTargetComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsHUDWidget)

// HUD 不参与鼠标命中或焦点导航。
USoulsHUDWidget::USoulsHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bIsFocusable = false;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    ForceVolatile(true);
}

// 激活 HUD 时保持游戏控制和鼠标捕获。
TOptional<FUIInputConfig> USoulsHUDWidget::GetDesiredInputConfig() const
{
    return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
}

// 原生构建血条、目标信息及输入提示，无需额外 Widget 蓝图。
TSharedRef<SWidget> USoulsHUDWidget::RebuildWidget()
{
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
        WidgetTree->RootWidget = Root;
        PlayerHealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayerHealthText"));
        UCanvasPanelSlot* TextSlot = Root->AddChildToCanvas(PlayerHealthText);
        TextSlot->SetPosition(FVector2D(48.0f, 32.0f));
        TextSlot->SetSize(FVector2D(340.0f, 32.0f));
        PlayerHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PlayerHealth"));
        PlayerHealthBar->SetFillColorAndOpacity(FLinearColor(0.6f, 0.03f, 0.03f));
        UCanvasPanelSlot* HealthSlot = Root->AddChildToCanvas(PlayerHealthBar);
        HealthSlot->SetPosition(FVector2D(48.0f, 72.0f));
        HealthSlot->SetSize(FVector2D(340.0f, 18.0f));

        TargetText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TargetText"));
        UCanvasPanelSlot* TargetTextSlot = Root->AddChildToCanvas(TargetText);
        TargetTextSlot->SetAnchors(FAnchors(0.5f, 0.85f));
        TargetTextSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        TargetTextSlot->SetPosition(FVector2D(0.0f, -36.0f));
        TargetTextSlot->SetSize(FVector2D(400.0f, 32.0f));
        TargetText->SetJustification(ETextJustify::Center);
        TargetHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("TargetHealth"));
        TargetHealthBar->SetFillColorAndOpacity(FLinearColor(0.7f, 0.15f, 0.04f));
        UCanvasPanelSlot* TargetSlot = Root->AddChildToCanvas(TargetHealthBar);
        TargetSlot->SetAnchors(FAnchors(0.5f, 0.85f));
        TargetSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        TargetSlot->SetPosition(FVector2D::ZeroVector);
        TargetSlot->SetSize(FVector2D(400.0f, 12.0f));

        UTextBlock* Hints = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InputHints"));
        Hints->SetText(NSLOCTEXT("Souls", "InputHints", "WASD 移动 | 鼠标 观察 | 中键 锁定 | Q / E 切换目标\n手柄：左摇杆移动，右摇杆观察/切换，按下右摇杆锁定"));
        UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(Hints);
        HintSlot->SetAnchors(FAnchors(0.0f, 1.0f));
        HintSlot->SetAlignment(FVector2D(0.0f, 1.0f));
        HintSlot->SetPosition(FVector2D(48.0f, -32.0f));
        HintSlot->SetSize(FVector2D(900.0f, 64.0f));
    }
    return Super::RebuildWidget();
}

// 订阅当前控制器并立即绑定已生成的角色。
void USoulsHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
        HandlePawnChanged(nullptr, PC->GetPawn());
    }
}

// 角色更换时释放旧绑定，并订阅新角色的生命及锁定事件。
void USoulsHUDWidget::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
    UnbindPawn();
    PlayerHealth = ULyraHealthComponent::FindHealthComponent(NewPawn);
    LockOn = NewPawn ? NewPawn->FindComponentByClass<USoulsLockOnComponent>() : nullptr;
    if (PlayerHealth.IsValid())
    {
        PlayerHealth->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerHealth);
        PlayerHealth->OnMaxHealthChanged.AddUniqueDynamic(this, &ThisClass::HandlePlayerHealth);
    }
    if (LockOn.IsValid())
    {
        LockOn->OnTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetChanged);
    }
    RefreshPlayerHealth();
    HandleTargetChanged(LockOn.IsValid() ? LockOn->GetTargetActor() : nullptr);
}

// 生命变化时更新玩家血条。
void USoulsHUDWidget::HandlePlayerHealth(ULyraHealthComponent* Component, float OldValue, float NewValue, AActor* Instigator)
{
    RefreshPlayerHealth();
}

// 显示真实血量，初始化尚未完成时隐藏血条。
void USoulsHUDWidget::RefreshPlayerHealth()
{
    if (!PlayerHealthBar || !PlayerHealthText)
    {
        return;
    }
    const bool bReady = PlayerHealth.IsValid() && PlayerHealth->GetMaxHealth() > 0.0f;
    PlayerHealthBar->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    PlayerHealthText->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bReady)
    {
        PlayerHealthBar->SetPercent(PlayerHealth->GetHealthNormalized());
        PlayerHealthText->SetText(FText::Format(NSLOCTEXT("Souls", "Health", "生命 {0} / {1}"),
            FText::AsNumber(FMath::CeilToInt(PlayerHealth->GetHealth())),
            FText::AsNumber(FMath::CeilToInt(PlayerHealth->GetMaxHealth()))));
    }
}

// 切换目标时重新订阅其血量。
void USoulsHUDWidget::HandleTargetChanged(AActor* Target)
{
    UnbindTarget();
    TargetHealth = ULyraHealthComponent::FindHealthComponent(Target);
    if (TargetHealth.IsValid())
    {
        TargetHealth->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetHealth);
        TargetHealth->OnMaxHealthChanged.AddUniqueDynamic(this, &ThisClass::HandleTargetHealth);
    }
    if (TargetText)
    {
        TargetText->SetText(Target ? NSLOCTEXT("Souls", "LockedTarget", "锁定目标") : FText::GetEmpty());
        TargetText->SetVisibility(Target ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    RefreshTargetHealth();
}

// 目标生命值变化时刷新目标血条。
void USoulsHUDWidget::HandleTargetHealth(ULyraHealthComponent* Component, float OldValue, float NewValue, AActor* Instigator)
{
    RefreshTargetHealth();
}

// 只有具备有效生命数据的目标才显示血条。
void USoulsHUDWidget::RefreshTargetHealth()
{
    if (TargetHealthBar)
    {
        const bool bReady = TargetHealth.IsValid() && TargetHealth->GetMaxHealth() > 0.0f;
        TargetHealthBar->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        TargetHealthBar->SetPercent(bReady ? TargetHealth->GetHealthNormalized() : 0.0f);
    }
}

// 清理旧目标的生命订阅。
void USoulsHUDWidget::UnbindTarget()
{
    if (TargetHealth.IsValid())
    {
        TargetHealth->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleTargetHealth);
        TargetHealth->OnMaxHealthChanged.RemoveDynamic(this, &ThisClass::HandleTargetHealth);
    }
    TargetHealth.Reset();
}

// 清理旧角色的生命、锁定及目标订阅。
void USoulsHUDWidget::UnbindPawn()
{
    UnbindTarget();
    if (PlayerHealth.IsValid())
    {
        PlayerHealth->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandlePlayerHealth);
        PlayerHealth->OnMaxHealthChanged.RemoveDynamic(this, &ThisClass::HandlePlayerHealth);
    }
    if (LockOn.IsValid())
    {
        LockOn->OnTargetChanged.RemoveDynamic(this, &ThisClass::HandleTargetChanged);
    }
    PlayerHealth.Reset();
    LockOn.Reset();
}

// 绘制随世界锁定点移动的菱形标记。
int32 USoulsHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);
    const USoulsTargetComponent* Target = LockOn.IsValid() ? LockOn->GetTarget() : nullptr;
    FVector2D Screen;
    if (IsActivated() && Target && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        GetOwningPlayer(), Target->GetComponentLocation(), Screen, false))
    {
        const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(GetOwningPlayer());
        const FVector2D Local = AllottedGeometry.AbsoluteToLocal(Viewport.LocalToAbsolute(Screen));
        const TArray<FVector2D> Points = {
            Local + FVector2D(0, -9), Local + FVector2D(9, 0), Local + FVector2D(0, 9),
            Local + FVector2D(-9, 0), Local + FVector2D(0, -9)
        };
        FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(),
            Points, ESlateDrawEffect::None, FLinearColor(1.0f, 0.85f, 0.4f), true, 2.0f);
        return BaseLayer + 1;
    }
    return BaseLayer;
}

// 控件移除时撤销控制器和角色订阅。
void USoulsHUDWidget::NativeDestruct()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
    }
    UnbindPawn();
    Super::NativeDestruct();
}
