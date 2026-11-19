#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetData.h"
#include "Animation/WidgetAnimation.h"
#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "DataTableEditorUtils.h"
#include "Engine/DataTable.h"
#include "Framework/Commands/UIAction.h"
#include "LEUIFrameWorkSettings.h"
#include "LEUIFrameWorkTypes.h"
#include "LEUIFrameWorkEditorTools.h"
#include "LEUIView.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "UObject/SoftObjectPath.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "LEUIFrameWorkEditor"

/** 为 Widget 模板生成框架 View、UIName 枚举和配置行。 */
class FLEUIFrameWorkEditorModule : public IModuleInterface
{
public:
    /** 注册 Content Browser 的 Create UI 右键菜单。 */
    virtual void StartupModule() override
    {
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLEUIFrameWorkEditorModule::RegisterMenus));
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

        FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("LEUIFrameWork"));
        Section.AddDynamicEntry(TEXT("LEUIFrameWorkCreateUI"), FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
        {
            const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
            if (!Context || !Context->SelectedAssets.ContainsByPredicate([](const FAssetData& Asset) { return Asset.IsInstanceOf(UWidgetBlueprint::StaticClass()); }))
            {
                return;
            }
            InSection.AddMenuEntry(
                TEXT("LEUIFrameWorkCreateUI"),
                LOCTEXT("CreateUI", "Create UI"),
                LOCTEXT("CreateUITooltip", "Generate a framework View, UIName enum and DataTable row from this Widget Blueprint."),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateStatic(&FLEUIFrameWorkEditorModule::CreateUIForAssets, Context->SelectedAssets)));
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
        const ULEUIFrameWorkSettings* Settings = GetDefault<ULEUIFrameWorkSettings>();
        UDataTable* DataTable = Settings ? Settings->UIConfigTable.LoadSynchronous() : nullptr;
        if (!DataTable || DataTable->GetRowStruct() != FLEUIConfigRow::StaticStruct())
        {
            UE_LOG(LogTemp, Error, TEXT("Create UI 失败：请在 LEUIFrameWork 项目设置中指定 FLEUIConfigRow DataTable。"));
            return;
        }

        FScopedTransaction Transaction(LOCTEXT("CreateUITransaction", "Create UI Configuration"));
        IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
        UUserDefinedEnum* NameEnum = LEUIFrameWorkEditorTools::ResolveOrCreateNameEnum(DataTable, AssetTools);
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
            UWidgetBlueprint* WidgetBlueprint = LEUIFrameWorkEditorTools::ResolveOrCreateView(Cast<UWidgetBlueprint>(AssetData.GetAsset()), AssetTools);
            UClass* GeneratedClass = WidgetBlueprint ? WidgetBlueprint->GeneratedClass : nullptr;
            if (!WidgetBlueprint || !GeneratedClass || !GeneratedClass->IsChildOf(ULEUIView::StaticClass()))
            {
                continue;
            }

            const FName RowName = WidgetBlueprint->GetFName();
            const FSoftClassPath GeneratedClassPath(GeneratedClass->GetPathName());
            const FLEUIConfigRow* ExistingRow = DataTable->FindRow<FLEUIConfigRow>(RowName, TEXT("LEUIFrameWorkEditor::CreateUI"), false);
            if (ExistingRow && ExistingRow->WBPPath.ToSoftObjectPath() != GeneratedClassPath)
            {
                UE_LOG(LogTemp, Error, TEXT("Create UI 冲突：DataTable 行 %s 已指向其他 Widget Blueprint。"), *RowName.ToString());
                continue;
            }

            FLEUIConfigRow Row = ExistingRow ? *ExistingRow : FLEUIConfigRow();
            Row.WBPPath = TSoftClassPtr<ULEUIView>(GeneratedClassPath);
            if (!ExistingRow)
            {
                Row.UIName = RowName;
                Row.DisplayName = FText::FromName(RowName);
                Row.UILayer = Settings->NormalLayer;
            }
            if ((!Row.UIName.IsNone() && Row.UIName != RowName) ||
                !LEUIFrameWorkEditorTools::EnsureNameEnumEntry(NameEnum, RowName))
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

IMPLEMENT_MODULE(FLEUIFrameWorkEditorModule, LEUIFrameWorkEditor)

#undef LOCTEXT_NAMESPACE
