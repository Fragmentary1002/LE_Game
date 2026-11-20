#pragma once

#include "AITypes.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "SoulsEnemyBehavior.generated.h"

class UBlackboardData;

UENUM()
enum class ESoulsEnemyIntent : uint8
{
    Idle,
    Hold,
    Chase,
    Investigate,
    Attack,
    Recover,
    ReturnHome
};

namespace SoulsEnemyBlackboard
{
    inline const FName Intent(TEXT("Intent"));
    inline const FName TargetActor(TEXT("TargetActor"));
    inline const FName LastSeenLocation(TEXT("LastSeenLocation"));
    inline const FName HomeLocation(TEXT("HomeLocation"));

    // 填充新建的树与黑板，供编辑器生成资产及旧配置运行时使用。
    SOULSCORERUNTIME_API bool BuildDefaultTree(UBehaviorTree& Tree, UBlackboardData& Blackboard);

    // 校验黑板键的类型和实例隔离约定。
    SOULSCORERUNTIME_API bool HasRequiredKeys(const UBlackboardData& Blackboard);
}

UCLASS()
class UBTService_SoulsEnemyContext : public UBTService
{
    GENERATED_BODY()

public:
    // 按固定间隔更新敌情与当前意图。
    UBTService_SoulsEnemyContext();

protected:
    // 刷新黑板，具体移动和攻击由各分支任务执行。
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

UCLASS()
class UBTDecorator_SoulsEnemyIntent : public UBTDecorator_Blackboard
{
    GENERATED_BODY()

public:
    // 设置黑板意图条件及分支中断方式。
    void Configure(ESoulsEnemyIntent Intent);
};

UCLASS()
class UBTTask_SoulsEnemyMove : public UBTTaskNode
{
    GENERATED_BODY()

public:
    // 为每个敌人保存独立的导航请求。
    UBTTask_SoulsEnemyMove();

    UPROPERTY(EditAnywhere, Category = "Souls")
    ESoulsEnemyIntent MoveIntent = ESoulsEnemyIntent::Chase;

protected:
    // 初始化本次任务并发出第一条导航请求。
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    // 等待当前请求完成，必要时再次靠近或有限频率重试回位。
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    // 分支切换时只取消本任务发出的导航请求。
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
    // 检查当前位置并推进追击、搜索或回位。
    EBTNodeResult::Type UpdateMove(UBehaviorTreeComponent& OwnerComp);

    FAIRequestID MoveRequestId = FAIRequestID::InvalidRequest;
    double NextMoveTime = 0.0;
};

UCLASS()
class UBTTask_SoulsEnemyAttack : public UBTTaskNode
{
    GENERATED_BODY()

public:
    // 为每次行为树运行独立跟踪攻击的启动状态。
    UBTTask_SoulsEnemyAttack();

protected:
    // 停止追击并准备向敌对目标转身。
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    // 转向到位后启动已有 GAS 攻击，等待动作结束。
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    // 任务结束或被中断时，记录本次动作的重试或冷却时间。
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
    bool bStartedAttack = false;
};
