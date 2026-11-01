#include "LyraUIFrameworkEditorTools.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/DataTable.h"
#include "Engine/UserDefinedEnum.h"
#include "Factories/EnumFactory.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LyraUIView.h"
#include "Misc/PackageName.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace
{
    const FName SourceTemplateKey(TEXT("LyraUIFramework.SourceTemplate"));
    const FName ConfigTableKey(TEXT("LyraUIFramework.ConfigTable"));

    /** 优先解析内存资产，再检查磁盘，避免覆盖尚未保存的同名资产。 */
    UObject* FindExistingAsset(const FString& PackageName, const FString& AssetName, bool& bExists)
    {
        const FSoftObjectPath ObjectPath(PackageName + TEXT(".") + AssetName);
        if (UObject* LoadedAsset = ObjectPath.ResolveObject())
        {
            bExists = true;
            return LoadedAsset;
        }
        const FAssetData AssetData = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"))
            .Get().GetAssetByObjectPath(ObjectPath);
        bExists = AssetData.IsValid() || FPackageName::DoesPackageExist(PackageName);
        return bExists ? ObjectPath.TryLoad() : nullptr;
    }
}

/** 不修改模板父类和内容，通过组合保留模板的控件树及蓝图行为。 */
UWidgetBlueprint* LyraUIFrameworkEditorTools::ResolveOrCreateView(UWidgetBlueprint* Template, IAssetTools& AssetTools)
{
    if (!Template || !Template->IsUpToDate() || !Template->GeneratedClass ||
        Template->GeneratedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    {
        UE_LOG(LogTemp, Error, TEXT("Create UI 失败：模板必须是已编译且可实例化的 Widget Blueprint。"));
        return nullptr;
    }
    if (Template->GeneratedClass->IsChildOf(ULyraUIView::StaticClass()))
    {
        return Template;
    }
    if (Template->GeneratedClass->IsChildOf(UCommonActivatableWidget::StaticClass()))
    {
        UE_LOG(LogTemp, Error, TEXT("Create UI 失败：非框架的 ActivatableWidget 不能嵌套为模板，请创建继承 ULyraUIView 的 View。"));
        return nullptr;
    }

    const FString AssetName = Template->GetName() + TEXT("_View");
    const FString Folder = FPackageName::GetLongPackagePath(Template->GetOutermost()->GetName());
    const FString PackageName = Folder / AssetName;
    bool bExists = false;
    UObject* ExistingAsset = FindExistingAsset(PackageName, AssetName, bExists);
    if (bExists)
    {
        UWidgetBlueprint* ExistingView = Cast<UWidgetBlueprint>(ExistingAsset);
        if (ExistingView && ExistingView->IsUpToDate() && ExistingView->GeneratedClass &&
            ExistingView->GeneratedClass->IsChildOf(ULyraUIView::StaticClass()) &&
            !ExistingView->GeneratedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) &&
            ExistingView->GetOutermost()->GetMetaData()->GetValue(ExistingView, SourceTemplateKey) == Template->GetPathName())
        {
            return ExistingView;
        }
        UE_LOG(LogTemp, Error, TEXT("Create UI 冲突：%s 已被其他资产占用或生成蓝图需要修复。"), *PackageName);
        return nullptr;
    }

    UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
    Factory->ParentClass = ULyraUIView::StaticClass();
    UWidgetBlueprint* View = Cast<UWidgetBlueprint>(AssetTools.CreateAsset(AssetName, Folder, UWidgetBlueprint::StaticClass(), Factory));
    if (!View || !View->WidgetTree)
    {
        return nullptr;
    }
    View->Modify();
    View->WidgetTree->Modify();
    View->WidgetTree->RootWidget = View->WidgetTree->ConstructWidget<UUserWidget>(Template->GeneratedClass, TEXT("TemplateContent"));
    View->GetOutermost()->GetMetaData()->SetValue(View, SourceTemplateKey, *Template->GetPathName());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(View);
    FKismetEditorUtilities::CompileBlueprint(View, EBlueprintCompileOptions::SkipSave);
    if (!View->IsUpToDate() || !View->GeneratedClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Create UI 失败：生成的 %s 未通过蓝图编译，未写入配置；请修复或删除该新资产。"), *AssetName);
        return nullptr;
    }
    View->MarkPackageDirty();
    return View;
}

/** 每张配置表拥有独立的标识枚举，不修改工程原有业务枚举。 */
UUserDefinedEnum* LyraUIFrameworkEditorTools::ResolveOrCreateNameEnum(UDataTable* DataTable, IAssetTools& AssetTools)
{
    if (!DataTable)
    {
        return nullptr;
    }
    const FString AssetName = TEXT("E_") + DataTable->GetName() + TEXT("_UIName");
    const FString Folder = FPackageName::GetLongPackagePath(DataTable->GetOutermost()->GetName());
    bool bExists = false;
    UObject* ExistingAsset = FindExistingAsset(Folder / AssetName, AssetName, bExists);
    if (bExists)
    {
        UUserDefinedEnum* ExistingEnum = Cast<UUserDefinedEnum>(ExistingAsset);
        if (ExistingEnum && ExistingEnum->GetOutermost()->GetMetaData()->GetValue(ExistingEnum, ConfigTableKey) == DataTable->GetPathName())
        {
            return ExistingEnum;
        }
        UE_LOG(LogTemp, Error, TEXT("Create UI 冲突：%s 不是当前配置表生成的标识枚举。"), *AssetName);
        return nullptr;
    }
    UEnumFactory* Factory = NewObject<UEnumFactory>();
    UUserDefinedEnum* NameEnum = Cast<UUserDefinedEnum>(AssetTools.CreateAsset(AssetName, Folder, UUserDefinedEnum::StaticClass(), Factory));
    if (NameEnum)
    {
        NameEnum->GetOutermost()->GetMetaData()->SetValue(NameEnum, ConfigTableKey, *DataTable->GetPathName());
        NameEnum->MarkPackageDirty();
    }
    return NameEnum;
}

/** 使用枚举标识的源文本查重，避免切换语言后追加重复项。 */
bool LyraUIFrameworkEditorTools::EnsureNameEnumEntry(UUserDefinedEnum* NameEnum, FName UIName)
{
    if (!NameEnum || UIName.IsNone())
    {
        return false;
    }
    for (int32 Index = 0; Index < NameEnum->NumEnums() - 1; ++Index)
    {
        if (FName(*NameEnum->GetAuthoredNameStringByIndex(Index)) == UIName)
        {
            return true;
        }
    }
    const int32 Index = NameEnum->NumEnums() - 1;
    if (Index < 0 || Index >= 255)
    {
        UE_LOG(LogTemp, Error, TEXT("Create UI 失败：蓝图枚举最多支持 255 个界面，请继续使用 FName 入口或拆分配置表。"));
        return false;
    }
    NameEnum->Modify();
    FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(NameEnum);
    if (NameEnum->NumEnums() != Index + 2 ||
        !FEnumEditorUtils::SetEnumeratorDisplayName(NameEnum, Index, FText::AsCultureInvariant(UIName.ToString())))
    {
        if (NameEnum->NumEnums() == Index + 2)
        {
            FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(NameEnum, Index);
        }
        return false;
    }
    NameEnum->MarkPackageDirty();
    return true;
}
