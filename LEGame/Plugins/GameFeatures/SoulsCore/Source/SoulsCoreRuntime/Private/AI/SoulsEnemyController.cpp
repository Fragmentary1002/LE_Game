#include "AI/SoulsEnemyController.h"

#include "AI/AIPawnData.h"
#include "AI/SoulsEnemy.h"
#include "AI/SoulsEnemyBehavior.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Teams/LyraTeamSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsEnemyController)

namespace
{
constexpr float AttackFacingTolerance = 12.0f;
constexpr float AttackTurnRate = 540.0f;
constexpr float ReacquireDelay = 1.0f;
constexpr float AttackRetryDelay = 0.15f;

// 仅处理尚未进入死亡流程并且仍有生命值的角色。
bool IsLiving(const AActor* Actor)
{
    const ULyraHealthComponent* Health = IsValid(Actor) ? ULyraHealthComponent::FindHealthComponent(Actor) : nullptr;
    return Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f;
}

// 通过同一标签启动或取消普通敌人的轻攻击能力。
const FGameplayTagContainer& GetLightAttackTags()
{
    static const FGameplayTagContainer Tags(FGameplayTag::RequestGameplayTag(TEXT("Ability.Souls.LightAttack")));
    return Tags;
}
}

// 控制器不创建 PlayerState，决策由行为树组件推进。
ASoulsEnemyController::ASoulsEnemyController()
{
    bWantsPlayerState = false;
}

// 接管时记录该实例的出生点，已初始化过的 Pawn 可以直接启动。
void ASoulsEnemyController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (const ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn()))
    {
        HomeLocation = Enemy->GetNavAgentLocation();
        HomeRotation = FRotator(0.0f, Enemy->GetActorRotation().Yaw, 0.0f);
        StartBehaviorFromPawnData();
    }
}

// 优先启动配置资产，旧数据没有指定树时使用同一套默认分支。
void ASoulsEnemyController::StartBehaviorFromPawnData()
{
    const ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn());
    const UAIPawnData* Data = IsValid(Enemy) ? Enemy->GetAIPawnData() : nullptr;
    if (!HasAuthority() || bBehaviorStarted || !IsLiving(Enemy) || !Data || !Enemy->IsBehaviorReady())
    {
        return;
    }

    UBehaviorTree* Tree = Data->BehaviorTree.Get();
    if (!Tree)
    {
        static const FName DefaultTreeName(TEXT("SoulsEnemyDefaultBehaviorTree"));
        UWorld* World = GetWorld();
        Tree = FindObjectFast<UBehaviorTree>(World, DefaultTreeName);
        if (!Tree)
        {
            Tree = NewObject<UBehaviorTree>(World, DefaultTreeName, RF_Transient);
            UBlackboardData* BlackboardAsset = NewObject<UBlackboardData>(Tree, NAME_None, RF_Transient);
            SoulsEnemyBlackboard::BuildDefaultTree(*Tree, *BlackboardAsset);
        }
    }

    UBlackboardComponent* BlackboardComp = nullptr;
    if (!Tree || !Tree->RootNode || !Tree->BlackboardAsset ||
        !SoulsEnemyBlackboard::HasRequiredKeys(*Tree->BlackboardAsset) ||
        !UseBlackboard(Tree->BlackboardAsset, BlackboardComp) || !BlackboardComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("Souls enemy %s has an invalid behavior tree or blackboard."), *GetName());
        return;
    }

    bReturningHome = false;
    NextAttackTime = NextAcquireTime = 0.0;
    ClearTarget();
    BlackboardComp->SetValueAsVector(SoulsEnemyBlackboard::HomeLocation, HomeLocation);
    BlackboardComp->SetValueAsEnum(SoulsEnemyBlackboard::Intent, uint8(ESoulsEnemyIntent::Idle));
    bBehaviorStarted = true;
    if (!RunBehaviorTree(Tree))
    {
        UE_LOG(LogTemp, Warning, TEXT("Souls enemy %s could not start its behavior tree."), *GetName());
        StopCombat();
    }
}

// 先中断行为树任务，再结束近战、导航和本次交战。
void ASoulsEnemyController::StopCombat()
{
    bBehaviorStarted = false;
    bReturningHome = false;
    if (UBehaviorTreeComponent* Behavior = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
    {
        Behavior->StopTree(EBTStopMode::Forced);
    }
    StopMovement();
    if (const ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn()))
    {
        Enemy->GetLyraAbilitySystemComponent()->CancelAbilities(&GetLightAttackTags());
    }
    ClearTarget();
    if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
    {
        BlackboardComp->SetValueAsEnum(SoulsEnemyBlackboard::Intent, uint8(ESoulsEnemyIntent::Idle));
    }
}

// 在父类清空 Pawn 引用前关闭所有行为。
void ASoulsEnemyController::OnUnPossess()
{
    StopCombat();
    Super::OnUnPossess();
}

// 只把存活、受玩家控制且阵营不同的 Pawn 当作威胁。
bool ASoulsEnemyController::IsHostilePlayer(const ASoulsEnemy& Enemy, const APawn* Candidate) const
{
    if (!IsLiving(Candidate) || !Candidate->IsPlayerControlled())
    {
        return false;
    }
    const ULyraTeamSubsystem* Teams = GetWorld()->GetSubsystem<ULyraTeamSubsystem>();
    return Teams && Teams->CompareTeams(&Enemy, Candidate) == ELyraTeamComparison::DifferentTeams;
}

// 索敌只使用当前可见的位置，并检查目标是否仍处于出生点的活动范围内。
APawn* ASoulsEnemyController::FindTarget(const ASoulsEnemy& Enemy, const UAIPawnData& Data) const
{
    APawn* Nearest = nullptr;
    double NearestDistanceSq = FMath::Square(Data.AcquireRadius);
    const double LeashDistanceSq = FMath::Square(Data.LeashRadius);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* Player = It->Get();
        APawn* Candidate = Player ? Player->GetPawn() : nullptr;
        if (!IsHostilePlayer(Enemy, Candidate))
        {
            continue;
        }
        const FVector CandidateLocation = Candidate->GetNavAgentLocation();
        const double DistanceSq = FVector::DistSquared(Enemy.GetNavAgentLocation(), CandidateLocation);
        if (DistanceSq <= NearestDistanceSq && FVector::DistSquared(HomeLocation, CandidateLocation) <= LeashDistanceSq &&
            LineOfSightTo(Candidate))
        {
            Nearest = Candidate;
            NearestDistanceSq = DistanceSq;
        }
    }
    return Nearest;
}

// 把事实更新到黑板，最后只发布一次意图，供行为树原子切换分支。
void ASoulsEnemyController::RefreshEnemyContext()
{
    ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn());
    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!bBehaviorStarted || !IsLiving(Enemy) || !BlackboardComp)
    {
        return;
    }

    const UAIPawnData& Data = *Enemy->GetAIPawnData();
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bReturningHome && !Target.IsValid() && LastSeenTime < 0.0 && Now >= NextAcquireTime)
    {
        Target = FindTarget(*Enemy, Data);
    }

    APawn* CurrentTarget = Target.Get();
    bool bVisible = false;
    bool bInAttackRange = false;
    if (!bReturningHome && (CurrentTarget || LastSeenTime >= 0.0))
    {
        const FVector EnemyLocation = Enemy->GetNavAgentLocation();
        const double LeashSquared = FMath::Square(Data.LeashRadius);
        if (!IsHostilePlayer(*Enemy, CurrentTarget) ||
            FVector::DistSquared(EnemyLocation, HomeLocation) > LeashSquared ||
            FVector::DistSquared(CurrentTarget->GetNavAgentLocation(), HomeLocation) > LeashSquared)
        {
            RequestReturnHome();
        }
        else if ((bVisible = LineOfSightTo(CurrentTarget)))
        {
            const FVector SeenLocation = CurrentTarget->GetNavAgentLocation();
            BlackboardComp->SetValueAsVector(SoulsEnemyBlackboard::LastSeenLocation, SeenLocation);
            LastSeenTime = Now;
            bInAttackRange = FVector::DistSquared(EnemyLocation, SeenLocation) <= FMath::Square(Data.AttackRange);
        }
        else if (Now - LastSeenTime >= Data.SightGracePeriod)
        {
            RequestReturnHome();
        }
    }

    BlackboardComp->SetValueAsObject(SoulsEnemyBlackboard::TargetActor, bVisible ? CurrentTarget : nullptr);
    ESoulsEnemyIntent Intent = ESoulsEnemyIntent::Idle;
    if (IsHitReacting())
    {
        Intent = ESoulsEnemyIntent::Recover;
    }
    else if (bReturningHome)
    {
        Intent = ESoulsEnemyIntent::ReturnHome;
    }
    else if (IsAttacking())
    {
        const bool bAttackTaskActive = BlackboardComp->GetValueAsEnum(SoulsEnemyBlackboard::Intent) == uint8(ESoulsEnemyIntent::Attack);
        Intent = bAttackTaskActive ? ESoulsEnemyIntent::Attack : ESoulsEnemyIntent::Hold;
    }
    else if (bVisible)
    {
        Intent = !bInAttackRange ? ESoulsEnemyIntent::Chase :
            (Now >= NextAttackTime ? ESoulsEnemyIntent::Attack : ESoulsEnemyIntent::Hold);
    }
    else if (Target.IsValid())
    {
        Intent = ESoulsEnemyIntent::Investigate;
    }
    BlackboardComp->SetValueAsEnum(SoulsEnemyBlackboard::Intent, uint8(Intent));
}

// 清空黑板中的目标和最后一次看到玩家的位置。
void ASoulsEnemyController::ClearTarget()
{
    Target.Reset();
    LastSeenTime = -1.0;
    if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
    {
        BlackboardComp->ClearValue(SoulsEnemyBlackboard::TargetActor);
        BlackboardComp->ClearValue(SoulsEnemyBlackboard::LastSeenLocation);
    }
}

// 脱战后取消尚在执行的轻攻击，并让行为树转入回位。
void ASoulsEnemyController::RequestReturnHome()
{
    if (!bBehaviorStarted || bReturningHome)
    {
        return;
    }
    bReturningHome = true;
    ClearTarget();
    if (const ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn()))
    {
        Enemy->GetLyraAbilitySystemComponent()->CancelAbilities(&GetLightAttackTags());
    }
    if (UBlackboardComponent* BlackboardComp = GetBlackboardComponent())
    {
        const ESoulsEnemyIntent Intent = IsHitReacting() ? ESoulsEnemyIntent::Recover : ESoulsEnemyIntent::ReturnHome;
        BlackboardComp->SetValueAsEnum(SoulsEnemyBlackboard::Intent, uint8(Intent));
    }
}

// 回到出生位置后恢复原朝向，并保留当前生命。
void ASoulsEnemyController::FinishReturnHome()
{
    if (ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn()); bReturningHome && Enemy)
    {
        StopMovement();
        Enemy->GetCharacterMovement()->StopMovementImmediately();
        Enemy->SetActorRotation(HomeRotation);
        bReturningHome = false;
        NextAcquireTime = GetWorld()->GetTimeSeconds() + ReacquireDelay;
        GetBlackboardComponent()->SetValueAsEnum(SoulsEnemyBlackboard::Intent, uint8(ESoulsEnemyIntent::Idle));
    }
}

// 通过 ASC 的标签判断攻击是否仍在执行。
bool ASoulsEnemyController::IsAttacking() const
{
    static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Souls.Attacking"));
    const ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn());
    return Enemy && Enemy->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(Tag);
}

// 通过 ASC 的标签判断当前是否应暂停战斗移动。
bool ASoulsEnemyController::IsHitReacting() const
{
    static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Souls.HitReact"));
    const ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn());
    return Enemy && Enemy->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(Tag);
}

// 出招前再次确认当前距离、视线和朝向，实际攻击交给已有 GAS 能力。
ESoulsEnemyAttackResult ASoulsEnemyController::TryMeleeAttack(float DeltaSeconds)
{
    ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn());
    APawn* CurrentTarget = Target.Get();
    if (!bBehaviorStarted || bReturningHome || !IsLiving(Enemy) || !IsHostilePlayer(*Enemy, CurrentTarget) ||
        IsAttacking() || IsHitReacting() || GetWorld()->GetTimeSeconds() < NextAttackTime)
    {
        return ESoulsEnemyAttackResult::Unavailable;
    }
    const UAIPawnData& Data = *Enemy->GetAIPawnData();
    const FVector EnemyLocation = Enemy->GetNavAgentLocation();
    const FVector TargetLocation = CurrentTarget->GetNavAgentLocation();
    const FVector Offset = TargetLocation - EnemyLocation;
    if (Offset.SizeSquared() > FMath::Square(Data.AttackRange) ||
        FVector::DistSquared(EnemyLocation, HomeLocation) > FMath::Square(Data.LeashRadius) ||
        FVector::DistSquared(TargetLocation, HomeLocation) > FMath::Square(Data.LeashRadius) || !LineOfSightTo(CurrentTarget))
    {
        return ESoulsEnemyAttackResult::Unavailable;
    }

    const float OldYaw = Enemy->GetActorRotation().Yaw;
    const float DesiredYaw = Offset.IsNearlyZero() ? OldYaw : Offset.Rotation().Yaw;
    const float NewYaw = FMath::FixedTurn(OldYaw, DesiredYaw, AttackTurnRate * DeltaSeconds);
    Enemy->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
    if (FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, DesiredYaw)) > AttackFacingTolerance)
    {
        return ESoulsEnemyAttackResult::Turning;
    }
    return Enemy->GetLyraAbilitySystemComponent()->TryActivateAbilitiesByTag(GetLightAttackTags(), false)
        ? ESoulsEnemyAttackResult::Started : ESoulsEnemyAttackResult::Unavailable;
}

// 已激活的挥击使用配置的间隔，失败时留出短暂的重试间隔。
void ASoulsEnemyController::ScheduleAttackRetry(bool bAttackStarted)
{
    const ASoulsEnemy* Enemy = Cast<ASoulsEnemy>(GetPawn());
    if (bBehaviorStarted && Enemy)
    {
        const float Cooldown = bAttackStarted ? Enemy->GetAIPawnData()->AttackCooldown : 0.0f;
        const double Now = GetWorld()->GetTimeSeconds();
        if (bAttackStarted || Now >= NextAttackTime)
        {
            NextAttackTime = Now + FMath::Max(Cooldown, AttackRetryDelay);
        }
    }
}
