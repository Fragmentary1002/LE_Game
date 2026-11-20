#include "Development/SoulsCombatDummy.h"

#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/SoulsCombatReactionComponent.h"
#include "Combat/SoulsGameplayAbility_Death.h"
#include "Combat/SoulsGameplayAbility_HitReact.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffect.h"
#include "Targeting/SoulsTargetComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsCombatDummy)

// 保留 Lyra 角色的生命、死亡清理和团队实现，只将 ASC 改为角色自有。
ASoulsCombatDummy::ASoulsCombatDummy(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    AutoPossessAI = EAutoPossessAI::Disabled;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel6, ECR_Overlap);
    GetMesh()->SetCollisionResponseToChannel(ECC_GameTraceChannel6, ECR_Ignore);
    CombatAbilitySystem = CreateDefaultSubobject<ULyraAbilitySystemComponent>(TEXT("CombatAbilitySystem"));
    CombatAbilitySystem->SetIsReplicated(true);
    CombatAbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    CombatHealthSet = CreateDefaultSubobject<ULyraHealthSet>(TEXT("CombatHealthSet"));
    CombatReaction = CreateDefaultSubobject<USoulsCombatReactionComponent>(TEXT("CombatReaction"));
    TargetPoint = CreateDefaultSubobject<USoulsTargetComponent>(TEXT("TargetPoint"));
    TargetPoint->SetupAttachment(GetRootComponent());
    TargetPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    CombatAbilities.Add(USoulsGameplayAbility_HitReact::StaticClass());
    CombatAbilities.Add(USoulsGameplayAbility_Death::StaticClass());
}

// 基类 GetLyraAbilitySystemComponent 会通过该虚函数找到独立 ASC。
UAbilitySystemComponent* ASoulsCombatDummy::GetAbilitySystemComponent() const
{
    return CombatAbilitySystem;
}

// 初始化 ASC 和真实生命，并通过标准 GameplayAbilitySpec 授予配置的能力。
void ASoulsCombatDummy::BeginPlay()
{
    Super::BeginPlay();
    if (UnarmedAnimationLayer)
    {
        GetMesh()->LinkAnimClassLayers(UnarmedAnimationLayer);
    }
    CombatAbilitySystem->InitAbilityActorInfo(this, this);
    if (HasAuthority())
    {
        SetGenericTeamId(IntegerToGenericTeamId(TeamId));
        for (TSubclassOf<UAttributeSet> AttributeClass : AdditionalAttributeSets)
        {
            if (AttributeClass && AttributeClass != ULyraHealthSet::StaticClass())
            {
                UAttributeSet* Attribute = NewObject<UAttributeSet>(this, AttributeClass);
                CombatAbilitySystem->AddAttributeSetSubobject(Attribute);
            }
        }
        CombatAbilitySystem->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(), FMath::Max(InitialHealth, 1.0f));
    }
    if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(this))
    {
        Health->InitializeWithAbilitySystem(CombatAbilitySystem);
    }
    CombatReaction->InitializeWithAbilitySystem(CombatAbilitySystem);
    if (HasAuthority())
    {
        for (TSubclassOf<ULyraGameplayAbility> AbilityClass : CombatAbilities)
        {
            if (AbilityClass)
            {
                CombatAbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
            }
        }
        for (TSubclassOf<UGameplayEffect> EffectClass : StartupEffects)
        {
            if (EffectClass)
            {
                const FGameplayEffectSpecHandle Spec = CombatAbilitySystem->MakeOutgoingSpec(
                    EffectClass, 1.0f, CombatAbilitySystem->MakeEffectContext());
                if (Spec.IsValid())
                {
                    CombatAbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
                }
            }
        }
    }
}

// 独立 ASC 不由 PawnExtension 持有，必须显式释放监听和角色映射。
void ASoulsCombatDummy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CombatReaction->UninitializeFromAbilitySystem();
    if (ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(this))
    {
        Health->UninitializeFromAbilitySystem();
    }
    if (HasAuthority())
    {
        CombatAbilitySystem->CancelAllAbilities();
        CombatAbilitySystem->ClearAllAbilities();
    }
    CombatAbilitySystem->ClearActorInfo();
    Super::EndPlay(EndPlayReason);
}
