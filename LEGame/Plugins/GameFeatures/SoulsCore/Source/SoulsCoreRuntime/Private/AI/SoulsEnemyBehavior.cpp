#include "AI/SoulsEnemyBehavior.h"

#include "AI/AIPawnData.h"
#include "AI/SoulsEnemy.h"
#include "AI/SoulsEnemyController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsEnemyBehavior)

namespace
{
constexpr float DecisionInterval = 0.1f;
constexpr float MoveRetryInterval = 0.5f;
constexpr float ArrivalTolerance = 55.0f;

// 按名称读取该黑板的独立实例键，拒绝跨敌人同步的键。
template<typename KeyType>
const KeyType* FindKey(const UBlackboardData& Blackboard, FName Name)
{
    const FBlackboardEntry* Entry = Blackboard.GetKey(Blackboard.GetKeyID(Name));
    return Entry && !Entry->bInstanceSynced ? Cast<KeyType>(Entry->KeyType.Get()) : nullptr;
}

// 添加缺少的黑板键，保留已有的资产配置。
template<typename KeyType>
KeyType* AddKey(UBlackboardData& Blackboard, FName Name)
{
    if (Blackboard.GetKeyID(Name) != FBlackboard::InvalidKey)
    {
        return nullptr;
    }
    FBlackboardEntry& Entry = Blackboard.Keys.AddDefaulted_GetRef();
    Entry.EntryName = Name;
    KeyType* Type = NewObject<KeyType>(&Blackboard);
    Entry.KeyType = Type;
    return Type;
}
}

// 检查节点使用的键类型、枚举定义和目标对象约束。
bool SoulsEnemyBlackboard::HasRequiredKeys(const UBlackboardData& Blackboard)
{
    const UBlackboardKeyType_Enum* IntentKey = FindKey<UBlackboardKeyType_Enum>(Blackboard, Intent);
    const UBlackboardKeyType_Object* TargetKey = FindKey<UBlackboardKeyType_Object>(Blackboard, TargetActor);
    return IntentKey && IntentKey->EnumType == StaticEnum<ESoulsEnemyIntent>() && TargetKey && TargetKey->BaseClass &&
        APawn::StaticClass()->IsChildOf(TargetKey->BaseClass) &&
        FindKey<UBlackboardKeyType_Vector>(Blackboard, LastSeenLocation) &&
        FindKey<UBlackboardKeyType_Vector>(Blackboard, HomeLocation);
}

// 创建可保存的树节点；运行时的旧配置也使用相同的树结构。
bool SoulsEnemyBlackboard::BuildDefaultTree(UBehaviorTree& Tree, UBlackboardData& Blackboard)
{
    if (Tree.RootNode || (Tree.BlackboardAsset && Tree.BlackboardAsset != &Blackboard))
    {
        return false;
    }
    if (UBlackboardKeyType_Enum* IntentKey = AddKey<UBlackboardKeyType_Enum>(Blackboard, Intent))
    {
        IntentKey->EnumType = StaticEnum<ESoulsEnemyIntent>();
    }
    if (UBlackboardKeyType_Object* TargetKey = AddKey<UBlackboardKeyType_Object>(Blackboard, TargetActor))
    {
        TargetKey->BaseClass = APawn::StaticClass();
    }
    AddKey<UBlackboardKeyType_Vector>(Blackboard, LastSeenLocation);
    AddKey<UBlackboardKeyType_Vector>(Blackboard, HomeLocation);
    if (!HasRequiredKeys(Blackboard))
    {
        return false;
    }

    Tree.BlackboardAsset = &Blackboard;
    UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(&Tree);
    Tree.RootNode = Root;
    Root->Services.Add(NewObject<UBTService_SoulsEnemyContext>(&Tree));

    // 给每个动作分支接入唯一的黑板意图和中断条件。
    const auto AddBranch = [&Tree, Root](ESoulsEnemyIntent BranchIntent, UBTTaskNode* Task)
    {
        FBTCompositeChild& Child = Root->Children.AddDefaulted_GetRef();
        Child.ChildTask = Task;
        if (BranchIntent != ESoulsEnemyIntent::Idle)
        {
            UBTDecorator_SoulsEnemyIntent* Condition = NewObject<UBTDecorator_SoulsEnemyIntent>(&Tree);
            Condition->Configure(BranchIntent);
            Child.Decorators.Add(Condition);
        }
    };
    // 为追击、调查和回位分别创建有独立配置的导航节点。
    const auto AddMove = [&Tree, &AddBranch](ESoulsEnemyIntent MoveIntent)
    {
        UBTTask_SoulsEnemyMove* Task = NewObject<UBTTask_SoulsEnemyMove>(&Tree);
        Task->MoveIntent = MoveIntent;
        AddBranch(MoveIntent, Task);
    };
    AddBranch(ESoulsEnemyIntent::Recover, NewObject<UBTTask_Wait>(&Tree));
    AddMove(ESoulsEnemyIntent::ReturnHome);
    AddBranch(ESoulsEnemyIntent::Attack, NewObject<UBTTask_SoulsEnemyAttack>(&Tree));
    AddBranch(ESoulsEnemyIntent::Hold, NewObject<UBTTask_Wait>(&Tree));
    AddMove(ESoulsEnemyIntent::Chase);
    AddMove(ESoulsEnemyIntent::Investigate);
    AddBranch(ESoulsEnemyIntent::Idle, NewObject<UBTTask_Wait>(&Tree));
    return true;
}

// 根 Service 只采集敌情，所有持续动作在任务节点中推进。
UBTService_SoulsEnemyContext::UBTService_SoulsEnemyContext()
{
    NodeName = TEXT("Update Souls Enemy Context");
    Interval = DecisionInterval;
    RandomDeviation = 0.0f;
    bNotifyTick = true;
}

// 保留引擎 Service 的计时，再刷新普通敌人的黑板。
void UBTService_SoulsEnemyContext::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    if (ASoulsEnemyController* Controller = Cast<ASoulsEnemyController>(OwnerComp.GetAIOwner()))
    {
        Controller->RefreshEnemyContext();
    }
}

// 根据 Intent 枚举值选择分支，意图切换时中断原任务。
void UBTDecorator_SoulsEnemyIntent::Configure(ESoulsEnemyIntent Intent)
{
    NodeName = TEXT("Souls Enemy Intent");
    BlackboardKey.AddEnumFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_SoulsEnemyIntent, BlackboardKey), StaticEnum<ESoulsEnemyIntent>());
    BlackboardKey.SelectedKeyName = SoulsEnemyBlackboard::Intent;
    IntValue = int32(Intent);
    StringValue = StaticEnum<ESoulsEnemyIntent>()->GetNameByValue(IntValue).ToString();
    OperationType = EArithmeticKeyOperation::Equal;
    FlowAbortMode = EBTFlowAbortMode::Both;
#if WITH_EDITORONLY_DATA
    ArithmeticOperation = EArithmeticKeyOperation::Equal;
#endif
}

// 为每只敌人实例化移动节点，避免不同控制器共享请求编号。
UBTTask_SoulsEnemyMove::UBTTask_SoulsEnemyMove()
{
    NodeName = TEXT("Souls Enemy Move");
    bCreateNodeInstance = true;
    bNotifyTick = true;
}

// 每次进入分支时重新建立本任务的导航请求。
EBTNodeResult::Type UBTTask_SoulsEnemyMove::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    MoveRequestId = FAIRequestID::InvalidRequest;
    NextMoveTime = 0.0;
    return UpdateMove(OwnerComp);
}

// 持续推进导航并报告任务完成状态。
void UBTTask_SoulsEnemyMove::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const EBTNodeResult::Type Result = UpdateMove(OwnerComp);
    if (Result != EBTNodeResult::InProgress)
    {
        FinishLatentTask(OwnerComp, Result);
    }
}

// 只结束本任务拥有的请求，防止取消其他分支发起的移动。
EBTNodeResult::Type UBTTask_SoulsEnemyMove::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    if (AAIController* Controller = OwnerComp.GetAIOwner(); Controller && MoveRequestId.IsValid() &&
        Controller->GetCurrentMoveRequestID() == MoveRequestId)
    {
        Controller->StopMovement();
    }
    MoveRequestId = FAIRequestID::InvalidRequest;
    return EBTNodeResult::Aborted;
}

// 发出完整路径请求，回位必须通过实际距离检查才算完成。
EBTNodeResult::Type UBTTask_SoulsEnemyMove::UpdateMove(UBehaviorTreeComponent& OwnerComp)
{
    ASoulsEnemyController* Controller = Cast<ASoulsEnemyController>(OwnerComp.GetAIOwner());
    const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    const ASoulsEnemy* Enemy = Controller ? Cast<ASoulsEnemy>(Controller->GetPawn()) : nullptr;
    const UAIPawnData* Data = Enemy ? Enemy->GetAIPawnData() : nullptr;
    if (!Data || !Blackboard)
    {
        return EBTNodeResult::Failed;
    }
    if (Controller->IsHitReacting() || Controller->IsAttacking())
    {
        AbortTask(OwnerComp, nullptr);
        return EBTNodeResult::InProgress;
    }
    if (MoveRequestId.IsValid() && Controller->GetCurrentMoveRequestID() == MoveRequestId &&
        Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
    {
        return EBTNodeResult::InProgress;
    }

    const bool bPreviousMoveEnded = MoveRequestId.IsValid();
    MoveRequestId = FAIRequestID::InvalidRequest;
    const bool bReturning = MoveIntent == ESoulsEnemyIntent::ReturnHome;
    const bool bChasing = MoveIntent == ESoulsEnemyIntent::Chase;
    const double Now = Enemy->GetWorld()->GetTimeSeconds();
    const FVector PawnLocation = Enemy->GetNavAgentLocation();
    const float StopRange = bChasing ? Data->AttackRange * 0.75f : ArrivalTolerance * 0.5f;
    FAIMoveRequest Request;
    if (bChasing)
    {
        const APawn* Target = Cast<APawn>(Blackboard->GetValueAsObject(SoulsEnemyBlackboard::TargetActor));
        if (!IsValid(Target))
        {
            return EBTNodeResult::InProgress;
        }
        if (FVector::DistSquared(PawnLocation, Target->GetNavAgentLocation()) <= FMath::Square(Data->AttackRange))
        {
            return EBTNodeResult::InProgress;
        }
        Request.SetGoalActor(Target);
    }
    else
    {
        const FName GoalKey = bReturning ? SoulsEnemyBlackboard::HomeLocation : SoulsEnemyBlackboard::LastSeenLocation;
        if (!Blackboard->IsVectorValueSet(GoalKey))
        {
            return EBTNodeResult::InProgress;
        }
        const FVector Goal = Blackboard->GetValueAsVector(GoalKey);
        if (FVector::DistSquared(PawnLocation, Goal) <= FMath::Square(ArrivalTolerance))
        {
            if (bReturning)
            {
                Controller->FinishReturnHome();
                return EBTNodeResult::Succeeded;
            }
            return EBTNodeResult::InProgress;
        }
        Request.SetGoalLocation(Goal);
    }
    const UPathFollowingComponent* PathFollowing = Controller->GetPathFollowingComponent();
    if (bPreviousMoveEnded && !bReturning && (!bChasing || !PathFollowing || !PathFollowing->DidMoveReachGoal()))
    {
        Controller->RequestReturnHome();
        return EBTNodeResult::Failed;
    }
    if (Now < NextMoveTime)
    {
        return EBTNodeResult::InProgress;
    }

    NextMoveTime = Now + MoveRetryInterval;
    Request.SetUsePathfinding(true).SetAllowPartialPath(false).SetProjectGoalLocation(!bChasing)
        .SetReachTestIncludesAgentRadius(false).SetReachTestIncludesGoalRadius(false).SetAcceptanceRadius(StopRange);
    const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        MoveRequestId = Result.MoveId;
    }
    else if (Result.Code == EPathFollowingRequestResult::Failed && !bReturning)
    {
        Controller->RequestReturnHome();
        return EBTNodeResult::Failed;
    }
    return EBTNodeResult::InProgress;
}

// 节点实例记录是否已经触发本次挥击。
UBTTask_SoulsEnemyAttack::UBTTask_SoulsEnemyAttack()
{
    NodeName = TEXT("Souls Enemy Melee Attack");
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
}

// 首次进入攻击分支时停止移动并重置本次动作的状态。
EBTNodeResult::Type UBTTask_SoulsEnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    bStartedAttack = false;
    if (ASoulsEnemyController* Controller = Cast<ASoulsEnemyController>(OwnerComp.GetAIOwner()))
    {
        Controller->StopMovement();
        return EBTNodeResult::InProgress;
    }
    return EBTNodeResult::Failed;
}

// 让角色平滑转向后出招，并等待 ASC 移除攻击标签。
void UBTTask_SoulsEnemyAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    ASoulsEnemyController* Controller = Cast<ASoulsEnemyController>(OwnerComp.GetAIOwner());
    if (!Controller)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }
    if (bStartedAttack)
    {
        if (!Controller->IsAttacking())
        {
            FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        }
        return;
    }

    const ESoulsEnemyAttackResult Result = Controller->TryMeleeAttack(DeltaSeconds);
    if (Result == ESoulsEnemyAttackResult::Started)
    {
        bStartedAttack = true;
    }
    else if (Result == ESoulsEnemyAttackResult::Unavailable)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
    }
}

// 从动作结束时计间隔；尚在转身便被中断时允许重新决策。
void UBTTask_SoulsEnemyAttack::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    if (ASoulsEnemyController* Controller = Cast<ASoulsEnemyController>(OwnerComp.GetAIOwner());
        Controller && (bStartedAttack || TaskResult == EBTNodeResult::Failed))
    {
        Controller->ScheduleAttackRetry(bStartedAttack);
    }
}
