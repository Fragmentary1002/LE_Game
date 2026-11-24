#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "LEAbilityDefinition.h"
#include "LEAbilityDefinitionEditor.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "LEAbilityFrameworkEditor"

namespace
{
    class FLEAbilityDefinitionActions final : public FAssetTypeActions_Base
    {
    public:
        // 在内容浏览器中显示技能定义资产类型。
        virtual FText GetName() const override { return LOCTEXT("AssetName", "技能定义"); }

        // 使用蓝绿色区分技能定义资产。
        virtual FColor GetTypeColor() const override { return FColor(32, 153, 148); }

        // 为技能定义资产注册专用编辑器。
        virtual UClass* GetSupportedClass() const override { return ULEAbilityDefinition::StaticClass(); }

        // 将资产归入 Gameplay 分类。
        virtual uint32 GetCategories() override { return EAssetTypeCategories::Gameplay; }

        // 为每个选中的技能资产打开独立编辑器。
        virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> Host) override
        {
            for (UObject* Object : InObjects)
            {
                if (ULEAbilityDefinition* Definition = Cast<ULEAbilityDefinition>(Object))
                {
                    MakeShared<FLEAbilityDefinitionEditor>()->Open(*Definition, Host);
                }
            }
        }
    };

    class FLEAbilityFrameworkEditorModule final : public IModuleInterface
    {
    public:
        // 注册双击技能定义时使用的资产操作。
        virtual void StartupModule() override
        {
            Actions = MakeShared<FLEAbilityDefinitionActions>();
            FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(Actions.ToSharedRef());
        }

        // 在模块卸载前解除资产类型注册。
        virtual void ShutdownModule() override
        {
            if (FAssetToolsModule* AssetTools = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
            {
                AssetTools->Get().UnregisterAssetTypeActions(Actions.ToSharedRef());
            }
            Actions.Reset();
        }

    private:
        TSharedPtr<IAssetTypeActions> Actions;
    };
}

// 加载专用技能编辑器模块。
IMPLEMENT_MODULE(FLEAbilityFrameworkEditorModule, LEAbilityFrameworkEditor)

#undef LOCTEXT_NAMESPACE
