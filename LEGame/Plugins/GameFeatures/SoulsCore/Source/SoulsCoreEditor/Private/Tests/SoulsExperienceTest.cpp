#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/SoulsCameraMode.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/SoulsCharacter.h"
#include "Character/SoulsHeroComponent.h"
#include "Character/SoulsPawnData.h"
#include "Components/ProgressBar.h"
#include "Development/SoulsSetupLibrary.h"
#include "Development/SoulsTrainingTarget.h"
#include "Editor.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/SoulsInputConfig.h"
#include "InputMappingContext.h"
#include "Targeting/SoulsLockOnComponent.h"
#include "Tests/AutomationEditorCommon.h"
#include "UI/SoulsHUDWidget.h"
#include "UObject/UObjectIterator.h"

namespace
{
    /** 等待真实 Experience 完成初始化后验证角色、输入、HUD 和锁定生命事件。 */
    class FSoulsExperienceReadyCommand : public IAutomationLatentCommand
    {
    public:
        // 保存当前测试和超时起点。
        explicit FSoulsExperienceReadyCommand(FAutomationTestBase* InTest)
            : Test(InTest), StartTime(FPlatformTime::Seconds())
        {
        }

        // 在 PIE 主线程中等待加载并执行实际玩法检查。
        virtual bool Update() override
        {
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
            ASoulsCharacter* Character = PC ? Cast<ASoulsCharacter>(PC->GetPawn()) : nullptr;
            USoulsHeroComponent* Hero = Character ? Character->FindComponentByClass<USoulsHeroComponent>() : nullptr;
            USoulsHUDWidget* HUD = nullptr;
            for (TObjectIterator<USoulsHUDWidget> It; It; ++It)
            {
                if (It->GetWorld() == World && It->IsActivated())
                {
                    HUD = *It;
                    break;
                }
            }
            if (!Character || !Hero || !Hero->IsReadyToBindInputs() || !HUD)
            {
                if (FPlatformTime::Seconds() - StartTime < 45.0)
                {
                    return false;
                }
                Test->AddError(FString::Printf(TEXT("Experience 初始化超时：Pawn=%s，输入=%d，HUD=%s"),
                    *GetNameSafe(Character), Hero && Hero->IsReadyToBindInputs(), *GetNameSafe(HUD)));
                return true;
            }
            USoulsLockOnComponent* Lock = Character->FindComponentByClass<USoulsLockOnComponent>();
            if (ReadyTime == 0.0)
            {
                ReadyTime = FPlatformTime::Seconds();
            }
            if (FPlatformTime::Seconds() - ReadyTime < 1.0)
            {
                return false;
            }
            if (bWaitingForDeath)
            {
                Test->TestNull(TEXT("目标死亡后自动解除锁定"), Lock->GetTargetActor());
                Test->TestTrue(TEXT("解除锁定恢复自由朝向"), Character->GetCharacterMovement()->bOrientRotationToMovement);
                Test->TestEqual(TEXT("目标死亡后隐藏目标血条"), TargetBar->GetVisibility(), ESlateVisibility::Collapsed);
                return true;
            }
            const ULyraPawnExtensionComponent* Extension = ULyraPawnExtensionComponent::FindPawnExtensionComponent(Character);
            const USoulsPawnData* Data = Extension ? Extension->GetPawnData<USoulsPawnData>() : nullptr;
            Test->TestNotNull(TEXT("角色使用 Souls PawnData"), Data);
            if (!Data || !Lock)
            {
                Test->AddError(TEXT("角色缺少 PawnData 或锁定组件"));
                return true;
            }
            if (!Test->TestNotNull(TEXT("自由相机引用有效"), Data->DefaultCameraMode.Get()) ||
                !Test->TestNotNull(TEXT("锁定相机引用有效"), Data->LockOnCameraMode.Get()))
            {
                return true;
            }
            Test->TestEqual(TEXT("使用配置的角色蓝图"), Character->GetClass()->GetName(), FString(TEXT("B_Hero_Souls_C")));
            Test->TestEqual(TEXT("使用自由相机蓝图"), Data->DefaultCameraMode->GetName(), FString(TEXT("CM_Souls_Free_C")));
            Test->TestEqual(TEXT("使用锁定相机蓝图"), Data->LockOnCameraMode->GetName(), FString(TEXT("CM_Souls_LockOn_C")));
            const USoulsInputConfig* Config = Cast<USoulsInputConfig>(Data->InputConfig);
            const UEnhancedInputLocalPlayerSubsystem* Input = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
            Test->TestTrue(TEXT("独立 IMC 已加入本地玩家"), Config && Input && Input->HasMappingContext(Config->MappingContext));
            Test->TestTrue(TEXT("自由移动面向移动方向"), Character->GetCharacterMovement()->bOrientRotationToMovement);
            UProgressBar* PlayerBar = Cast<UProgressBar>(HUD->GetWidgetFromName(TEXT("PlayerHealthBar")));
            TargetBar = Cast<UProgressBar>(HUD->GetWidgetFromName(TEXT("TargetHealthBar")));
            Test->TestNotNull(TEXT("HUD 蓝图绑定玩家血条"), PlayerBar);
            Test->TestNotNull(TEXT("HUD 蓝图绑定目标血条"), TargetBar.Get());
            if (!PlayerBar || !TargetBar.IsValid())
            {
                return true;
            }
            Test->TestTrue(TEXT("玩家血条读取真实生命值"), PlayerBar->GetPercent() > 0.0f);
            int32 TargetCount = 0;
            for (TActorIterator<ASoulsTrainingTarget> It(World); It; ++It)
            {
                ++TargetCount;
            }
            Test->TestEqual(TEXT("地图包含两个训练目标"), TargetCount, 2);
            Lock->ToggleLockOn();
            ASoulsTrainingTarget* Target = Cast<ASoulsTrainingTarget>(Lock->GetTargetActor());
            if (!Test->TestNotNull(TEXT("从出生点可以锁定训练目标"), Target))
            {
                return true;
            }
            Lock->SwitchTarget(Lock->GetTargetActor()->GetActorLocation().Y < 0.0f ? 1.0f : -1.0f);
            ASoulsTrainingTarget* SwitchedTarget = Cast<ASoulsTrainingTarget>(Lock->GetTargetActor());
            Test->TestNotEqual(TEXT("可以切换到另一个训练目标"), SwitchedTarget, Target);
            if (!Test->TestNotNull(TEXT("切换后的目标有效"), SwitchedTarget))
            {
                return true;
            }
            Target = SwitchedTarget;
            Test->TestTrue(TEXT("锁定后面向控制器方向"), Character->GetCharacterMovement()->bUseControllerDesiredRotation);
            ULyraHealthComponent* Health = Target->FindComponentByClass<ULyraHealthComponent>();
            const float Before = Health->GetHealth();
            Target->ApplyTestDamage(25.0f);
            Test->TestEqual(TEXT("训练目标经过 GAS 扣除生命"), Health->GetHealth(), Before - 25.0f);
            Test->TestTrue(TEXT("目标血条通过事件同步生命比例"), FMath::IsNearlyEqual(TargetBar->GetPercent(), Health->GetHealthNormalized()));
            Target->ApplyTestDamage(Health->GetHealth());
            bWaitingForDeath = true;
            return false;
        }

    private:
        FAutomationTestBase* Test;
        double StartTime;
        double ReadyTime = 0.0;
        bool bWaitingForDeath = false;
        TWeakObjectPtr<UProgressBar> TargetBar;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsExperienceTest, "SoulsCore.Experience.EmptySoulsMap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 从实际地图启动 PIE，确保资产引用最终形成可运行的玩法链路。
bool FSoulsExperienceTest::RunTest(const FString& Parameters)
{
    if (!USoulsSetupLibrary::PrepareContentForSetup())
    {
        AddError(TEXT("无法挂载 SoulsCore 内容目录"));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/SoulsCore/Maps/EmptySoulsMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FSoulsExperienceReadyCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
