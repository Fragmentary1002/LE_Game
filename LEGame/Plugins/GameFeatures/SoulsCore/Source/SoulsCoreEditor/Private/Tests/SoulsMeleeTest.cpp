#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Character/LyraHealthComponent.h"
#include "Character/SoulsCharacter.h"
#include "Character/SoulsHeroComponent.h"
#include "Combat/SoulsAttackDefinition.h"
#include "Combat/SoulsMeleeTraceComponent.h"
#include "Combat/SoulsMeleeWeapon.h"
#include "Development/SoulsCombatDummy.h"
#include "Development/SoulsSetupLibrary.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Targeting/SoulsLockOnComponent.h"
#include "Targeting/SoulsTargetComponent.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
    /** 在真实 PIE 中验证动画开窗、一次命中、取消以及伤害死亡链路。 */
    class FSoulsMeleeLoopCommand : public IAutomationLatentCommand
    {
    public:
        // 保存测试对象并启动全流程超时计时。
        explicit FSoulsMeleeLoopCommand(FAutomationTestBase* InTest)
            : Test(InTest), StartTime(FPlatformTime::Seconds())
        {
        }

        // 每帧推进真实动作阶段，避免依赖固定帧率或手动调用动画通知。
        virtual bool Update() override
        {
            if (FPlatformTime::Seconds() - StartTime > 60.0)
            {
                Test->AddError(FString::Printf(TEXT("近战集成测试超时，阶段=%d"), Phase));
                Cleanup();
                return true;
            }
            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
            ASoulsCharacter* Character = PC ? Cast<ASoulsCharacter>(PC->GetPawn()) : nullptr;
            USoulsHeroComponent* Hero = Character ? Character->FindComponentByClass<USoulsHeroComponent>() : nullptr;
            USoulsMeleeTraceComponent* Trace = Character ? Character->FindComponentByClass<USoulsMeleeTraceComponent>() : nullptr;
            UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
            if (!Character || !Hero || !Hero->IsReadyToBindInputs() || !Trace || !Trace->GetWeapon() || !ASC)
            {
                return false;
            }
            const USoulsAttackDefinition* Definition = Trace->GetAttackDefinition();
            if (!Definition)
            {
                Test->AddError(TEXT("玩家没有配置轻攻击数据"));
                return true;
            }
            if (Phase == 0)
            {
                for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
                {
                    if (Spec.Ability && Spec.Ability->AbilityTags.HasTagExact(
                        FGameplayTag::RequestGameplayTag(TEXT("Ability.Souls.LightAttack"))))
                    {
                        AttackHandle = Spec.Handle;
                        break;
                    }
                }
                if (!Test->TestTrue(TEXT("Experience 已授予轻攻击能力"), AttackHandle.IsValid()))
                {
                    return true;
                }
                // 隔离地图中原有目标，防止测试命中被无关角色遮挡。
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    if (*It != Character && It->FindComponentByClass<USoulsTargetComponent>())
                    {
                        It->SetActorLocation(FVector(20000.0f, 20000.0f, 200.0f));
                    }
                }
                Character->SetActorLocation(FVector(0.0f, 0.0f, 120.0f));
                Character->SetActorRotation(FRotator::ZeroRotator);
                PC->SetControlRotation(FRotator::ZeroRotator);
                UClass* DummyClass = LoadClass<ASoulsCombatDummy>(nullptr,
                    TEXT("/SoulsCore/Development/B_SoulsCombatDummy.B_SoulsCombatDummy_C"));
                if (!DummyClass)
                {
                    // 保持资产目录调整时的明确失败信息，不以原生无动画靶掩盖配置错误。
                    Test->AddError(TEXT("无法加载 /SoulsCore/Development/B_SoulsCombatDummy"));
                    return true;
                }
                FActorSpawnParameters Parameters;
                Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                Dummy = World->SpawnActor<ASoulsCombatDummy>(DummyClass, FVector(5000.0f, 0.0f, 200.0f), FRotator::ZeroRotator, Parameters);
                if (!Dummy.IsValid())
                {
                    Test->AddError(TEXT("无法创建带动画的战斗训练敌人"));
                    return true;
                }
                Dummy->GetCharacterMovement()->DisableMovement();
                UClass* CombatSetClass = LoadClass<UAttributeSet>(nullptr, TEXT("/Script/LyraGame.LyraCombatSet"));
                UClass* CombatInitClass = LoadClass<UGameplayEffect>(nullptr,
                    TEXT("/SoulsCore/Combat/Effects/GE_Souls_CombatInit.GE_Souls_CombatInit_C"));
                if (!CombatSetClass || !CombatInitClass)
                {
                    Test->AddError(TEXT("无法为测试敌人配置独立的反击伤害属性"));
                    return true;
                }
                UAbilitySystemComponent* DummyASC = Dummy->GetAbilitySystemComponent();
                DummyASC->AddAttributeSetSubobject(NewObject<UAttributeSet>(Dummy.Get(), CombatSetClass));
                const FGameplayEffectSpecHandle InitSpec = DummyASC->MakeOutgoingSpec(CombatInitClass,
                    1.0f, DummyASC->MakeEffectContext());
                if (InitSpec.IsValid())
                {
                    DummyASC->ApplyGameplayEffectSpecToSelf(*InitSpec.Data.Get());
                }
                InitialHealth = ULyraHealthComponent::FindHealthComponent(Dummy.Get())->GetHealth();
                TraceRef = Trace;
                HitHandle = Trace->OnHit.AddRaw(this, &FSoulsMeleeLoopCommand::RecordHit);
                if (!Test->TestTrue(TEXT("真实能力能够激活"), ASC->TryActivateAbility(AttackHandle)))
                {
                    Cleanup();
                    return true;
                }
                Phase = 1;
                return false;
            }
            if (Phase == 1)
            {
                if (!Trace->IsHitWindowEnabled())
                {
                    if (!Trace->IsAttackActive())
                    {
                        Test->AddError(TEXT("真实 Montage 未产生近战有效窗口"));
                        Cleanup();
                        return true;
                    }
                    return false;
                }
                Test->TestFalse(TEXT("有效窗口内拒绝重复攻击"), ASC->TryActivateAbility(AttackHandle));
                FVector Root;
                FVector Tip;
                Trace->GetWeapon()->GetBladePoints(Root, Tip);
                Dummy->SetActorLocation((Root + Tip) * 0.5f, false, nullptr, ETeleportType::TeleportPhysics);
                Phase = 2;
                return false;
            }
            if (Phase == 2)
            {
                ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Dummy.Get());
                if (Health && Health->GetHealth() < InitialHealth)
                {
                    Test->TestEqual(TEXT("一次轨迹接触造成配置的25伤害"), Health->GetHealth(), InitialHealth - 25.0f);
                    Test->TestTrue(TEXT("非致死伤害启动真实受击能力"), Dummy->GetAbilitySystemComponent()->HasMatchingGameplayTag(
                        FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact"))));
                    Phase = 3;
                }
                else if (!Trace->IsAttackActive())
                {
                    Test->AddError(TEXT("真实 Montage 武器轨迹没有命中刀刃附近的敌人"));
                    Cleanup();
                    return true;
                }
                return false;
            }
            if (Phase == 3)
            {
                if (Trace->IsAttackActive())
                {
                    return false;
                }
                Test->TestEqual(TEXT("同次攻击跨帧只发送一次目标命中"), DummyHitCount, 1);
                Test->TestEqual(TEXT("同次攻击跨帧不会重复扣血"), ULyraHealthComponent::FindHealthComponent(Dummy.Get())->GetHealth(), InitialHealth - 25.0f);
                Test->TestFalse(TEXT("完整动作结束关闭命中窗口"), Trace->IsHitWindowEnabled());
                Dummy->SetActorLocation(FVector(5000.0f, 0.0f, 200.0f));
                if (!Test->TestTrue(TEXT("动作结束后可以开始新攻击"), ASC->TryActivateAbility(AttackHandle)))
                {
                    Cleanup();
                    return true;
                }
                Phase = 4;
                return false;
            }
            if (Phase == 4)
            {
                if (!Trace->IsHitWindowEnabled())
                {
                    return false;
                }
                ASC->CancelAbilityHandle(AttackHandle);
                Test->TestFalse(TEXT("取消能力立即关闭伤害窗口"), Trace->IsHitWindowEnabled());
                Test->TestFalse(TEXT("取消能力立即释放攻击状态"), Trace->IsAttackActive());
                Test->TestEqual(TEXT("空挥和取消不改变远处目标生命"), ULyraHealthComponent::FindHealthComponent(Dummy.Get())->GetHealth(), InitialHealth - 25.0f);
                Test->TestTrue(TEXT("取消后能够开始用于受击打断的攻击"), ASC->TryActivateAbility(AttackHandle));
                Phase = 7;
                return false;
            }
            if (Phase == 5)
            {
                // 此阶段固定刀刃与敌人，只测环境阻挡，不依赖占位动作的美术轨迹。
                Test->TestTrue(TEXT("固定轨迹测试能够开始攻击"), Trace->BeginAttack());
                Trace->SetHitWindowEnabled(true);
                Trace->EndAttack();
                Test->TestEqual(TEXT("墙体隔断时重叠刀刃也不能产生有效命中"), DummyHitCount, 1);
                if (Wall.IsValid())
                {
                    Wall->Destroy();
                }
                Test->TestTrue(TEXT("移除墙体后开始对照攻击"), Trace->BeginAttack());
                Trace->SetHitWindowEnabled(true);
                Trace->EndAttack();
                Test->TestEqual(TEXT("无墙体对照确认刀刃真实覆盖敌人"), DummyHitCount, 2);
                PrepareDeathCase(Character, ASC, Definition);
                Phase = 6;
                return false;
            }
            if (Phase == 6)
            {
                USoulsLockOnComponent* Lock = Character->FindComponentByClass<USoulsLockOnComponent>();
                Test->TestNull(TEXT("目标死亡后自动解除锁定"), Lock ? Lock->GetTargetActor() : nullptr);
                Test->TestFalse(TEXT("致死帧不进入普通受击状态"), Dummy.IsValid() && Dummy->GetAbilitySystemComponent()->HasMatchingGameplayTag(
                    FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact"))));
                ApplyDamage(ASC, Character, Definition);
                Test->TestFalse(TEXT("玩家死亡后不能再激活轻攻击"), ASC->TryActivateAbility(AttackHandle));
                Test->TestFalse(TEXT("玩家死亡后伤害窗口保持关闭"), Trace->IsHitWindowEnabled());
                Cleanup();
                return true;
            }
            if (Phase == 7)
            {
                if (!Trace->IsHitWindowEnabled())
                {
                    return false;
                }
                const float HealthBefore = ULyraHealthComponent::FindHealthComponent(Character)->GetHealth();
                ApplyDamage(Dummy->GetAbilitySystemComponent(), Character, Definition, false);
                Test->TestEqual(TEXT("敌人真实伤害效果使玩家扣25生命"),
                    ULyraHealthComponent::FindHealthComponent(Character)->GetHealth(), HealthBefore - 25.0f);
                Test->TestTrue(TEXT("玩家非致死受击进入硬直状态"), ASC->HasMatchingGameplayTag(
                    FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact"))));
                Test->TestFalse(TEXT("真实受击立即取消玩家攻击"), Trace->IsAttackActive());
                Test->TestFalse(TEXT("真实受击立即关闭伤害窗口"), Trace->IsHitWindowEnabled());
                Phase = 8;
                return false;
            }
            if (Phase == 8)
            {
                if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact"))))
                {
                    return false;
                }
                Test->TestTrue(TEXT("受击动作结束恢复玩家行走"), Character->GetCharacterMovement()->MovementMode == MOVE_Walking);
                PrepareWallCase(World, Character, Trace);
                Phase = 5;
                return false;
            }
            return false;
        }

    private:
        // 记录对测试敌人的外部命中通知，用于验证跨帧去重。
        void RecordHit(const FHitResult& Hit)
        {
            if (Hit.GetActor() == Dummy.Get())
            {
                ++DummyHitCount;
            }
        }

        // 在角色与固定刀刃之间创建有真实碰撞的薄墙。
        void PrepareWallCase(UWorld* World, ASoulsCharacter* Character, USoulsMeleeTraceComponent* Trace)
        {
            ASoulsMeleeWeapon* Weapon = Trace->GetWeapon();
            Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Weapon->SetActorTransform(FTransform(FRotator::ZeroRotator, Character->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f)));
            Dummy->SetActorLocation(Character->GetActorLocation() + FVector(155.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
            Dummy->GetCharacterMovement()->DisableMovement();
            Wall = World->SpawnActor<AStaticMeshActor>(Character->GetActorLocation() + FVector(65.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
            if (Wall.IsValid())
            {
                Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
                Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
                Wall->SetActorScale3D(FVector(0.1f, 3.0f, 4.0f));
                Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            }
            Test->TestNotNull(TEXT("环境遮挡对照墙创建成功"), Wall.Get());
        }

        // 将敌人放入正前方并锁定，再通过标准伤害 GE 造成死亡。
        void PrepareDeathCase(ASoulsCharacter* Character, UAbilitySystemComponent* ASC, const USoulsAttackDefinition* Definition)
        {
            Dummy->SetActorLocation(Character->GetActorLocation() + FVector(300.0f, 0.0f, 0.0f));
            USoulsLockOnComponent* Lock = Character->FindComponentByClass<USoulsLockOnComponent>();
            if (Lock)
            {
                Lock->ClearTarget();
                Lock->ToggleLockOn();
                Test->TestEqual(TEXT("死亡测试前锁定训练敌人"), Lock->GetTargetActor(), static_cast<AActor*>(Dummy.Get()));
            }
            // 对照攻击刚产生的硬直需要取消，才能验证致死帧不会重新启动普通受击。
            Dummy->GetAbilitySystemComponent()->CancelAllAbilities();
            ApplyDamage(ASC, Dummy.Get(), Definition);
        }

        // 使用攻击者来源和真实伤害 GE，通过 HealthSet 进入死亡事件链路。
        void ApplyDamage(UAbilitySystemComponent* SourceASC, AActor* Target,
            const USoulsAttackDefinition* Definition, bool bLethal = true)
        {
            UAbilitySystemComponent* TargetASC = Target ? Target->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
            if (ASoulsCharacter* Player = Cast<ASoulsCharacter>(Target))
            {
                TargetASC = Player->GetAbilitySystemComponent();
            }
            if (!TargetASC)
            {
                Test->AddError(TEXT("死亡测试目标没有能力系统"));
                return;
            }
            // 调低生命保证固定伤害致死，真实扣血仍由现有伤害效果执行。
            if (bLethal)
            {
                TargetASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 1.0f);
            }
            FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
            // 玩家自伤会被队伍规则拒绝，改用敌人的 ASC 作为伤害来源。
            UAbilitySystemComponent* DamageSource = Target == SourceASC->GetAvatarActor() && Dummy.IsValid()
                ? Dummy->GetAbilitySystemComponent() : SourceASC;
            Context = DamageSource->MakeEffectContext();
            Context.AddInstigator(DamageSource->GetAvatarActor(), DamageSource->GetAvatarActor());
            FGameplayEffectSpecHandle Spec = DamageSource->MakeOutgoingSpec(Definition->DamageEffectClass, 1.0f, Context);
            if (Spec.IsValid())
            {
                DamageSource->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
            }
            if (bLethal)
            {
                Test->TestTrue(TEXT("标准伤害效果使目标进入死亡状态"), ULyraHealthComponent::FindHealthComponent(Target)->IsDeadOrDying());
            }
        }

        // 清理原生委托，确保延迟命令销毁后不会留下悬空回调。
        void Cleanup()
        {
            if (TraceRef.IsValid())
            {
                TraceRef->OnHit.Remove(HitHandle);
                TraceRef->EndAttack();
            }
        }

        FAutomationTestBase* Test;
        double StartTime;
        int32 Phase = 0;
        float InitialHealth = 0.0f;
        int32 DummyHitCount = 0;
        FGameplayAbilitySpecHandle AttackHandle;
        TWeakObjectPtr<USoulsMeleeTraceComponent> TraceRef;
        TWeakObjectPtr<ASoulsCombatDummy> Dummy;
        TWeakObjectPtr<AStaticMeshActor> Wall;
        FDelegateHandle HitHandle;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulsMeleeTest, "SoulsCore.Combat.SingleAttackLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 加载实际 Souls 地图运行真实能力和动画，最终关闭 PIE 释放测试对象。
bool FSoulsMeleeTest::RunTest(const FString& Parameters)
{
    if (!USoulsSetupLibrary::PrepareContentForSetup())
    {
        AddError(TEXT("无法挂载 SoulsCore 内容目录"));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/SoulsCore/Maps/EmptySoulsMap")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FSoulsMeleeLoopCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
