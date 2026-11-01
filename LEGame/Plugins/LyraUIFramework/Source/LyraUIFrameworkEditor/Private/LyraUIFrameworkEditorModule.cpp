#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetData.h"
#include "Animation/WidgetAnimation.h"
#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "DataTableEditorUtils.h"
#include "Engine/DataTable.h"
#include "Framework/Commands/UIAction.h"
#include "LyraUIFrameworkSettings.h"
#include "LyraUIFrameworkTypes.h"
#include "LyraUIFrameworkEditorTools.h"
#include "LyraUIView.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "UObject/SoftObjectPath.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "LyraUIFrameworkEditor"

/** 为 Widget 模板生成框架 View、UIName 枚举和配置行。 */
class FLyraUIFrameworkEditorModule : public IModuleInterface
{
public:
    /** 注册 Content Browser 的 Create UI 右键菜单。 */
    virtual void StartupModule() override
    {
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLyraUIFrameworkEditorModule::RegisterMenus));
    }

    /** 注销本模块注册的菜单扩展。 */
    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }

private:
    /** 在 Widget Blueprint 资产菜单中添加 Create UI 命令。 */
    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu"));
        if (!Menu)
        {
            return;
        }

        FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("LyraUIFramework"));
        Section.AddDynamicEntry(TEXT("LyraUIFrameworkCreateUI"), FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
        {
            const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
            if (!Context || !Context->SelectedAssets.ContainsByPredicate([](const FAssetData& Asset) { return Asset.IsInstanceOf(UWidgetBlueprint::StaticClass()); }))
            {
                return;
            }
            InSection.AddMenuEntry(
                TEXT("LyraUIFrameworkCreateUI"),
                LOCTEXT("CreateUI", "Create UI"),
                LOCTEXT("CreateUITooltip", "Generate a framework View, UIName enum and DataTable row from this Widget Blueprint."),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateStatic(&FLyraUIFrameworkEditorModule::CreateUIForAssets, Context->SelectedAssets)));
        }));
    }

    /** 生成 View 和标识枚举后写入配置，保留已有行的业务设置。 */
    static void CreateUIForAssets(TArray<FAssetData> SelectedAssets)
    {
        if (!GEditor || GEditor->PlayWorld)
        {
            UE_LOG(LogTemp, Error, TEXT("Create UI 不能在 PIE 或模拟运行时生成资产。"));
            return;
        }
        const ULyraUIFrameworkSettings* Settings = GetDefault<ULyraUIFrameworkSettings>();
        UDataTable* DataTable = Settings ? Settings->UIConfigTable.LoadSynchronous() : nullptr;
        if (!DataTable || DataTable->GetRowStruct() != FLyraUIConfigRow::StaticStruct())
        {
            UE_LOG(LogTemp, Error, TEXT("Create UI 失败：请在 Lyra UI Framework 项目设置中指定 FLyraUIConfigRow DataTable。"));
            return;
        }

        FScopedTransaction Transaction(LOCTEXT("CreateUITransaction", "Create UI Configuration"));
        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
        UUserDefinedEnum* NameEnum = LyraUIFrameworkEditorTools::ResolveOrCreateNameEnum(DataTable, AssetTools);
        if (!NameEnum)
        {
            Transaction.Cancel();
            return;
        }
        int32 CreatedCount = 0;
        for (const FAssetData& AssetData : SelectedAssets)
        {
            if (!AssetData.IsInstanceOf(UWidgetBlueprint::StaticClass()))
            {
                continue;
            }
            UWidgetBlueprint* WidgetBlueprint = LyraUIFrameworkEditorTools::ResolveOrCreateView(Cast<UWidgetBlueprint>(AssetData.GetAsset()), AssetTools);
            UClass* GeneratedClass = WidgetBlueprint ? WidgetBlueprint->GeneratedClass : nullptr;
            if (!WidgetBlueprint || !GeneratedClass || !GeneratedClass->IsChildOf(ULyraUIView::StaticClass()))
            {
                continue;
            }

            const FName RowName = WidgetBlueprint->GetFName();
            const FSoftClassPath GeneratedClassPath(GeneratedClass->GetPathName());
            const FLyraUIConfigRow* ExistingRow = DataTable->FindRow<FLyraUIConfigRow>(RowName, TEXT("LyraUIFrameworkEditor::CreateUI"), false);
            if (ExistingRow && ExistingRow->WBPPath.ToSoftObjectPath() != GeneratedClassPath)
            {
                UE_LOG(LogTemp, Error, TEXT("Create UI 冲突：DataTable 行 %s 已指向其他 Widget Blueprint。"), *RowName.ToString());
                continue;
            }

            FLyraUIConfigRow Row = ExistingRow ? *ExistingRow : FLyraUIConfigRow();
            Row.WBPPath = TSoftClassPtr<ULyraUIView>(GeneratedClassPath);
            if (!ExistingRow)
            {
                Row.UIName = RowName;
                Row.DisplayName = FText::FromName(RowName);
                Row.UILayer = Settings->NormalLayer;
            }
            if ((!Row.UIName.IsNone() && Row.UIName != RowName) ||
                !LyraUIFrameworkEditorTools::EnsureNameEnumEntry(NameEnum, RowName))
            {
                UE_LOG(LogTemp, Error, TEXT("Create UI 跳过：%s 的 UIName 与行名不一致，或枚举同步失败。"), *RowName.ToString());
                continue;
            }
            Row.UIName = RowName;

            DataTable->Modify();
            const FDataTableEditorUtils::EDataTableChangeInfo ChangeInfo = ExistingRow
                ? FDataTableEditorUtils::EDataTableChangeInfo::RowData
                : FDataTableEditorUtils::EDataTableChangeInfo::RowList;
            FDataTableEditorUtils::BroadcastPreChange(DataTable, ChangeInfo);
            DataTable->AddRow(RowName, Row);
            FDataTableEditorUtils::BroadcastPostChange(DataTable, ChangeInfo);
            DataTable->MarkPackageDirty();
            ++CreatedCount;

            if (!WidgetBlueprint->Animations.ContainsByPredicate([](const TObjectPtr<UWidgetAnimation>& Animation) { return Animation && Animation->GetFName() == TEXT("OpenAnimation"); }))
            {
                UE_LOG(LogTemp, Warning, TEXT("框架 View %s 未定义可选 OpenAnimation；包装模板中的同名动画不会自动代理到 View。"), *RowName.ToString());
            }
            if (!WidgetBlueprint->Animations.ContainsByPredicate([](const TObjectPtr<UWidgetAnimation>& Animation) { return Animation && Animation->GetFName() == TEXT("CloseAnimation"); }))
            {
                UE_LOG(LogTemp, Warning, TEXT("框架 View %s 未定义可选 CloseAnimation；包装模板中的同名动画不会自动代理到 View。"), *RowName.ToString());
            }
        }
        UE_LOG(LogTemp, Display, TEXT("Create UI 完成：创建或更新 %d 条配置，请保存 View、枚举和 DataTable 资产。"), CreatedCount);
    }
};

IMPLEMENT_MODULE(FLyraUIFrameworkEditorModule, LyraUIFrameworkEditor)

#undef LOCTEXT_NAMESPACE
