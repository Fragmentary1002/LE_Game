#include "LEAbilityDefinitionEditor.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "IDetailsView.h"
#include "LEAbilityDefinition.h"
#include "Misc/DataValidation.h"
#include "PreviewScene.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SEditorViewport.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LEAbilityDefinitionEditor"

namespace
{
    const FName DetailsTab(TEXT("LEAbilityDetails"));
    const FName PreviewTab(TEXT("LEAbilityPreview"));

    /** 在相同时间比例下绘制窗口、点事件和当前播放头。 */
    class SLETimelineTrack final : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SLETimelineTrack) {}
            SLATE_ATTRIBUTE(float, Start)
            SLATE_ATTRIBUTE(float, End)
            SLATE_ATTRIBUTE(float, Length)
            SLATE_ATTRIBUTE(float, Position)
            SLATE_ARGUMENT(FLinearColor, Color)
        SLATE_END_ARGS()

        // 保存动态时序值，用于资产编辑后的实时重绘。
        void Construct(const FArguments& Args)
        {
            Start = Args._Start;
            End = Args._End;
            Length = Args._Length;
            Position = Args._Position;
            Color = Args._Color;
        }

        // 为每条轨道保留固定高度。
        virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
        {
            return FVector2D(160.0, 22.0);
        }

        // 使用同一尺度绘制持续时间和单点标记。
        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
            FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override
        {
            const float Duration = FMath::Max(Length.Get(0.0f), KINDA_SMALL_NUMBER);
            const float Width = Geometry.GetLocalSize().X;
            const float Left = FMath::Clamp(Start.Get() / Duration, 0.0f, 1.0f) * Width;
            const float Right = FMath::Clamp(End.Get() / Duration, 0.0f, 1.0f) * Width;
            const float PlayheadX = FMath::Clamp(Position.Get() / Duration, 0.0f, 1.0f) * Width;
            const FSlateBrush* Brush = FAppStyle::GetBrush("WhiteBrush");
            FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(),
                Brush, ESlateDrawEffect::None, FLinearColor(0.04f, 0.055f, 0.07f));
            FSlateDrawElement::MakeBox(Elements, Layer + 1,
                Geometry.ToPaintGeometry(FVector2f(FMath::Max(3.0f, Right - Left), 12.0f), FSlateLayoutTransform(FVector2f(Left, 5.0f))),
                Brush, ESlateDrawEffect::None, Color);
            FSlateDrawElement::MakeBox(Elements, Layer + 2,
                Geometry.ToPaintGeometry(FVector2f(1.0f, 22.0f), FSlateLayoutTransform(FVector2f(PlayheadX, 0.0f))),
                Brush, ESlateDrawEffect::None, FLinearColor::White);
            return Layer + 2;
        }

    private:
        TAttribute<float> Start;
        TAttribute<float> End;
        TAttribute<float> Length;
        TAttribute<float> Position;
        FLinearColor Color;
    };
}

/** 只在预览世界中采样 Montage 姿势，跳转时关闭动画通知。 */
class SLEAbilityPreview final : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SLEAbilityPreview) {}
    SLATE_END_ARGS()

    // 初始化隔离场景和骨骼网格，使用当前资产的预览配置。
    void Construct(const FArguments& Args, ULEAbilityDefinition* InDefinition)
    {
        Definition = InDefinition;
        Mesh = NewObject<USkeletalMeshComponent>(GetTransientPackage());
        Scene.AddComponent(Mesh, FTransform::Identity);
        SEditorViewport::Construct(SEditorViewport::FArguments());
        RefreshPreview();
    }

    // 从预览场景中注销骨骼组件。
    virtual ~SLEAbilityPreview() override
    {
        if (TSharedPtr<FEditorViewportClient> ViewClient = GetViewportClient())
        {
            ViewClient->Viewport = nullptr;
        }
        Scene.RemoveComponent(Mesh);
    }

    // 切换编辑器预览的播放状态。
    FReply TogglePlaying()
    {
        bPlaying = !bPlaying && GetLength() > 0.0f;
        if (bPlaying && Position >= GetLength())
        {
            Seek(0.0f);
        }
        return FReply::Handled();
    }

    // 暂停并向前移动一个素材采样步长。
    FReply Step()
    {
        bPlaying = false;
        Seek(Position + 1.0f / 30.0f);
        return FReply::Handled();
    }

    // 停止预览并回到动作起点。
    FReply Reset()
    {
        bPlaying = false;
        Seek(0.0f);
        return FReply::Handled();
    }

    // 根据滑杆位置跳转到指定素材时间。
    void Scrub(float Normalized)
    {
        bPlaying = false;
        Seek(Normalized * GetLength());
    }

    // 返回当前播放头指向的素材秒数。
    float GetPosition() const { return Position; }

    // 返回 Montage 的素材长度。
    float GetLength() const
    {
        const ULEAbilityDefinition* Asset = Definition.Get();
        return Asset && Asset->Execution.Montage ? Asset->Execution.Montage->GetPlayLength() : 0.0f;
    }

    // 根据预览播放状态显示对应操作。
    FText GetPlaybackLabel() const { return bPlaying ? LOCTEXT("Pause", "暂停") : LOCTEXT("Play", "播放"); }

    // 在资产属性修改后更新模型和动画。
    void RefreshPreview()
    {
        const ULEAbilityDefinition* Asset = Definition.Get();
        UAnimMontage* Montage = Asset ? Asset->Execution.Montage.Get() : nullptr;
        USkeletalMesh* PreviewMesh = Asset ? Asset->PreviewMesh.Get() : nullptr;
        if (!PreviewMesh && Montage)
        {
            PreviewMesh = Montage->GetPreviewMesh();
        }
        if (Mesh->GetSkeletalMeshAsset() != PreviewMesh || CurrentMontage.Get() != Montage)
        {
            CurrentMontage = Montage;
            Mesh->SetSkeletalMeshAsset(PreviewMesh);
            Mesh->PlayAnimation(PreviewMesh ? Montage : nullptr, false);
        }
        Seek(Position);
    }

protected:
    // 创建只渲染技能预览场景的编辑器视口。
    virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
    {
        TSharedRef<FEditorViewportClient> ViewClient = MakeShared<FEditorViewportClient>(nullptr, &Scene, SharedThis(this));
        ViewClient->SetViewLocation(FVector(280.0f, 360.0f, 170.0f));
        ViewClient->SetViewRotation((FVector(0.0f, 0.0f, 90.0f) - ViewClient->GetViewLocation()).Rotation());
        ViewClient->SetViewMode(VMI_Lit);
        ViewClient->SetRealtime(true);
        ViewClient->EngineShowFlags.SetGrid(false);
        return ViewClient;
    }

    // 按与运行时一致的速率推进预览。
    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override
    {
        SEditorViewport::Tick(Geometry, CurrentTime, DeltaTime);
        const ULEAbilityDefinition* Asset = Definition.Get();
        if (bPlaying && Asset && Asset->Execution.Montage)
        {
            Seek(Position + DeltaTime * Asset->Execution.PlayRate * Asset->Execution.Montage->RateScale);
            bPlaying = Position < GetLength();
        }
    }

private:
    // 在单节点预览实例中采样姿态并更新视口。
    void Seek(float Time)
    {
        Position = FMath::Clamp(Time, 0.0f, GetLength());
        if (UAnimSingleNodeInstance* Animation = Mesh->GetSingleNodeInstance())
        {
            Animation->SetPlaying(false);
            Animation->SetPosition(Position, false);
            Mesh->RefreshBoneTransforms();
        }
        Invalidate();
    }

    FPreviewScene Scene;
    USkeletalMeshComponent* Mesh = nullptr;
    TWeakObjectPtr<ULEAbilityDefinition> Definition;
    TWeakObjectPtr<UAnimMontage> CurrentMontage;
    float Position = 0.0f;
    bool bPlaying = false;
};

// 创建参数和动画时间轴并排的技能编辑器。
void FLEAbilityDefinitionEditor::Open(ULEAbilityDefinition& InDefinition, const TSharedPtr<IToolkitHost>& Host)
{
    Definition = &InDefinition;
    FDetailsViewArgs DetailsArgs;
    DetailsArgs.bHideSelectionTip = true;
    Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsArgs);
    Details->SetObject(&InDefinition);

    const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("LEAbilityDefinitionEditor_v1")
        ->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
            ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.38f)->AddTab(DetailsTab, ETabState::OpenedTab))
            ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.62f)->AddTab(PreviewTab, ETabState::OpenedTab)));
    InitAssetEditor(Host ? EToolkitMode::WorldCentric : EToolkitMode::Standalone, Host,
        TEXT("LEAbilityDefinitionEditor"), Layout, true, true, &InDefinition);
}

// 把技能编辑器的页签与构建回调绑定。
void FLEAbilityDefinitionEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
    InTabManager->RegisterTabSpawner(DetailsTab, FOnSpawnTab::CreateSP(this, &FLEAbilityDefinitionEditor::SpawnDetails))
        .SetDisplayName(LOCTEXT("Details", "技能配置"));
    InTabManager->RegisterTabSpawner(PreviewTab, FOnSpawnTab::CreateSP(this, &FLEAbilityDefinitionEditor::SpawnPreview))
        .SetDisplayName(LOCTEXT("Preview", "动作与时间轴"));
}

// 清理页签注册，避免关闭窗口后持有编辑器回调。
void FLEAbilityDefinitionEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
    InTabManager->UnregisterTabSpawner(DetailsTab);
    InTabManager->UnregisterTabSpawner(PreviewTab);
    FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

// 提供编辑器持久化标识。
FName FLEAbilityDefinitionEditor::GetToolkitFName() const { return TEXT("LEAbilityDefinition"); }

// 显示技能编辑器名称。
FText FLEAbilityDefinitionEditor::GetBaseToolkitName() const { return LOCTEXT("Toolkit", "技能定义"); }

// 显示嵌入模式的页签前缀。
FString FLEAbilityDefinitionEditor::GetWorldCentricTabPrefix() const { return LOCTEXT("TabPrefix", "技能").ToString(); }

// 设置嵌入模式的页签色彩。
FLinearColor FLEAbilityDefinitionEditor::GetWorldCentricTabColorScale() const { return FLinearColor(0.1f, 0.4f, 0.45f); }

// 显示资产的可编辑属性。
TSharedRef<SDockTab> FLEAbilityDefinitionEditor::SpawnDetails(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)[Details.ToSharedRef()];
}

// 显示时间轴、播放头、命令按钮和预览视口。
TSharedRef<SDockTab> FLEAbilityDefinitionEditor::SpawnPreview(const FSpawnTabArgs& Args)
{
    SAssignNew(Preview, SLEAbilityPreview, Definition.Get());
    const TSharedRef<SVerticalBox> Tracks = SNew(SVerticalBox);
    const TWeakPtr<SLEAbilityPreview> WeakPreview = Preview;
    const TWeakObjectPtr<ULEAbilityDefinition> WeakDefinition = Definition;
    const TWeakPtr<SVerticalBox> WeakTracks = Tracks;

    // 属性编辑后重建轨道，窗口条和事件点使用同一个播放头。
    const auto Rebuild = [WeakPreview, WeakDefinition, WeakTracks]()
    {
        const TSharedPtr<SLEAbilityPreview> PreviewWidget = WeakPreview.Pin();
        const TSharedPtr<SVerticalBox> TrackList = WeakTracks.Pin();
        const ULEAbilityDefinition* Asset = WeakDefinition.Get();
        if (!PreviewWidget || !TrackList || !Asset)
        {
            return;
        }

        PreviewWidget->RefreshPreview();
        TrackList->ClearChildren();
        // 为一个窗口或点事件创建预览行。
        const auto AddTrack = [&TrackList, &PreviewWidget](const FText& Name, float Start, float End, FLinearColor Color)
        {
            const FText TrackLabel = FMath::IsNearlyEqual(Start, End)
                ? FText::Format(LOCTEXT("PointTrackLabel", "{0}  {1}s"), Name, FText::AsNumber(Start))
                : FText::Format(LOCTEXT("WindowTrackLabel", "{0}  {1}–{2}s"), Name, FText::AsNumber(Start), FText::AsNumber(End));
            TrackList->AddSlot().AutoHeight().Padding(0, 1)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(0.43f).VAlign(VAlign_Center).Padding(4, 0)
                [SNew(STextBlock).Text(TrackLabel).ToolTipText(TrackLabel)]
                + SHorizontalBox::Slot().FillWidth(0.57f)
                [SNew(SLETimelineTrack).Start(Start).End(End).Color(Color)
                    .Length(TAttribute<float>::CreateSP(PreviewWidget.Get(), &SLEAbilityPreview::GetLength))
                    .Position(TAttribute<float>::CreateSP(PreviewWidget.Get(), &SLEAbilityPreview::GetPosition))]
            ];
        };
        for (const FLECombatTimelineWindow& Window : Asset->TimelineWindows)
        {
            AddTrack(Window.DisplayName.IsEmpty() ? FText::FromName(Window.Tag.GetTagName()) : Window.DisplayName,
                Window.StartTime, Window.EndTime, FLinearColor(0.16f, 0.63f, 0.54f));
        }
        for (const FLECombatTimelinePoint& Point : Asset->TimelinePoints)
        {
            AddTrack(Point.DisplayName.IsEmpty() ? FText::FromName(Point.Tag.GetTagName()) : Point.DisplayName,
                Point.Time, Point.Time, FLinearColor(1.0f, 0.66f, 0.25f));
        }
    };
    Details->OnFinishedChangingProperties().AddLambda([Rebuild](const FPropertyChangedEvent&) { Rebuild(); });
    Rebuild();

    return SNew(SDockTab)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(0.66f)[Preview.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight().Padding(4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(Preview.ToSharedRef(), &SLEAbilityPreview::GetPlaybackLabel).OnClicked(Preview.ToSharedRef(), &SLEAbilityPreview::TogglePlaying)]
            + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("Step", "步进")).OnClicked(Preview.ToSharedRef(), &SLEAbilityPreview::Step)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("Reset", "重置")).OnClicked(Preview.ToSharedRef(), &SLEAbilityPreview::Reset)]
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 4, 0)[SNew(SButton).Text(LOCTEXT("AddPoint", "添加点")).OnClicked(this, &FLEAbilityDefinitionEditor::AddPoint)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("AddWindow", "添加窗口")).OnClicked(this, &FLEAbilityDefinitionEditor::AddWindow)]
            + SHorizontalBox::Slot().AutoWidth().Padding(4, 0)[SNew(SButton).Text(LOCTEXT("Validate", "校验")).OnClicked(this, &FLEAbilityDefinitionEditor::Validate)]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(6, 1)
        [
            SNew(SSlider).Value_Lambda([WeakPreview]()
            {
                const TSharedPtr<SLEAbilityPreview> Widget = WeakPreview.Pin();
                return Widget && Widget->GetLength() > 0.0f ? Widget->GetPosition() / Widget->GetLength() : 0.0f;
            }).OnValueChanged(Preview.ToSharedRef(), &SLEAbilityPreview::Scrub)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(6, 1)
        [
            SNew(STextBlock).Text_Lambda([WeakPreview]()
            {
                const TSharedPtr<SLEAbilityPreview> Widget = WeakPreview.Pin();
                return Widget ? FText::Format(LOCTEXT("CurrentTime", "{0}s / {1}s"),
                    FText::AsNumber(Widget->GetPosition()), FText::AsNumber(Widget->GetLength())) : FText();
            })
        ]
        + SVerticalBox::Slot().FillHeight(0.34f)[SNew(SScrollBox) + SScrollBox::Slot()[Tracks]]
        + SVerticalBox::Slot().AutoHeight().Padding(6)
        [
            SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakEditor = TWeakPtr<FAssetEditorToolkit>(SharedThis(this))]()
            {
                const TSharedPtr<FAssetEditorToolkit> Editor = WeakEditor.Pin();
                return Editor ? static_cast<FLEAbilityDefinitionEditor&>(*Editor).ValidationText : FText();
            })
        ]
    ];
}

// 插入点事件并通过属性通知同步轨道和编辑器。
FReply FLEAbilityDefinitionEditor::AddPoint()
{
    if (ULEAbilityDefinition* Asset = Definition.Get())
    {
        const FScopedTransaction Transaction(LOCTEXT("AddPointTransaction", "添加技能点事件"));
        Asset->Modify();
        FLECombatTimelinePoint& Point = Asset->TimelinePoints.AddDefaulted_GetRef();
        Point.Time = Preview->GetPosition();
        Point.DisplayName = LOCTEXT("NewPoint", "新点事件");
        FPropertyChangedEvent Event(FindFProperty<FProperty>(ULEAbilityDefinition::StaticClass(),
            GET_MEMBER_NAME_CHECKED(ULEAbilityDefinition, TimelinePoints)), EPropertyChangeType::ArrayAdd);
        Asset->PostEditChangeProperty(Event);
        Details->ForceRefresh();
        Details->OnFinishedChangingProperties().Broadcast(Event);
    }
    return FReply::Handled();
}

// 插入窗口并将默认结束时间限制在 Montage 范围内。
FReply FLEAbilityDefinitionEditor::AddWindow()
{
    if (ULEAbilityDefinition* Asset = Definition.Get())
    {
        const FScopedTransaction Transaction(LOCTEXT("AddWindowTransaction", "添加技能窗口"));
        Asset->Modify();
        FLECombatTimelineWindow& Window = Asset->TimelineWindows.AddDefaulted_GetRef();
        const float Length = Preview->GetLength();
        Window.StartTime = FMath::Min(Preview->GetPosition(), FMath::Max(0.0f, Length - 0.01f));
        Window.EndTime = Length > 0.0f ? FMath::Min(Window.StartTime + 0.2f, Length) : 0.2f;
        Window.DisplayName = LOCTEXT("NewWindow", "新窗口");
        FPropertyChangedEvent Event(FindFProperty<FProperty>(ULEAbilityDefinition::StaticClass(),
            GET_MEMBER_NAME_CHECKED(ULEAbilityDefinition, TimelineWindows)), EPropertyChangeType::ArrayAdd);
        Asset->PostEditChangeProperty(Event);
        Details->ForceRefresh();
        Details->OnFinishedChangingProperties().Broadcast(Event);
    }
    return FReply::Handled();
}

// 显示当前资产的校验结果和具体错误。
FReply FLEAbilityDefinitionEditor::Validate()
{
    if (const ULEAbilityDefinition* Asset = Definition.Get())
    {
        FDataValidationContext Context;
        const EDataValidationResult Result = Asset->IsDataValid(Context);
        TArray<FText> Warnings;
        TArray<FText> Errors;
        Context.SplitIssues(Warnings, Errors);
        ValidationText = Result == EDataValidationResult::Valid ? LOCTEXT("Valid", "配置校验通过")
            : FText::Join(FText::FromString(TEXT("\n")), Errors);
    }
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
