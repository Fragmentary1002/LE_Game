#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class IDetailsView;
class SLEAbilityPreview;
class ULEAbilityDefinition;

/** 在同一窗口中编辑技能参数、预览 Montage 并定位时间轴事件。 */
class FLEAbilityDefinitionEditor final : public FAssetEditorToolkit
{
public:
    // 打开指定技能资产并初始化预览布局。
    void Open(ULEAbilityDefinition& InDefinition, const TSharedPtr<IToolkitHost>& Host);

    // 注册参数和预览页签。
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

    // 关闭编辑器时移除页签注册。
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

    // 返回用于布局保存的编辑器标识。
    virtual FName GetToolkitFName() const override;

    // 返回编辑器的窗口名称。
    virtual FText GetBaseToolkitName() const override;

    // 为嵌入布局提供页签前缀。
    virtual FString GetWorldCentricTabPrefix() const override;

    // 设置编辑器嵌入页签的主题颜色。
    virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:
    // 创建包含资产字段的详情页签。
    TSharedRef<SDockTab> SpawnDetails(const FSpawnTabArgs& Args);

    // 创建预览视口、时间轴和编辑命令。
    TSharedRef<SDockTab> SpawnPreview(const FSpawnTabArgs& Args);

    // 在播放头时间添加一个点事件。
    FReply AddPoint();

    // 在播放头时间添加一个持续窗口。
    FReply AddWindow();

    // 执行资产数据校验并显示结果。
    FReply Validate();

    TWeakObjectPtr<ULEAbilityDefinition> Definition;
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<SLEAbilityPreview> Preview;
    FText ValidationText;
};
