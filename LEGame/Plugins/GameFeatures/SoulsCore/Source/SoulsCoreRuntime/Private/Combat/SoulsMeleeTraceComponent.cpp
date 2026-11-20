#include "Combat/SoulsMeleeTraceComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/SoulsAttackDefinition.h"
#include "Combat/SoulsMeleeWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Targeting/SoulsLockOnComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsMeleeTraceComponent)

// 关闭常态 Tick，攻击时在骨骼与物理更新之后读取武器变换。
USoulsMeleeTraceComponent::USoulsMeleeTraceComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    WeaponClass = ASoulsMeleeWeapon::StaticClass();
}

// 武器只由权威角色创建并复制，采样顺序依赖角色 Mesh。
void USoulsMeleeTraceComponent::BeginPlay()
{
    Super::BeginPlay();
    ACharacter* Character = GetPawn<ACharacter>();
    if (!Character)
    {
        return;
    }
    AddTickPrerequisiteComponent(Character->GetMesh());
    if (Character->HasAuthority() && WeaponClass)
    {
        FActorSpawnParameters Parameters;
        Parameters.Owner = Character;
        Parameters.Instigator = Character;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Weapon = GetWorld()->SpawnActor<ASoulsMeleeWeapon>(WeaponClass, FTransform::Identity, Parameters);
        if (Weapon)
        {
            Weapon->AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocket);
            Weapon->SetActorRelativeTransform(WeaponAttachTransform);
        }
    }
}

// 只允许存活、配置完整的权威角色开始新的单次攻击。
bool USoulsMeleeTraceComponent::BeginAttack()
{
    ACharacter* Character = GetPawn<ACharacter>();
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(GetOwner());
    if (!Character || !Character->HasAuthority() || bAttackActive || !Weapon ||
        !DefaultAttackDefinition || (Health && (Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)))
    {
        return false;
    }
    bAttackActive = true;
    bHitWindowEnabled = false;
    AttackElapsed = 0.0f;
    HitActors.Reset();
    Weapon->GetLocalBladePoints(LocalBladeRoot, LocalBladeTip);
    PreviousWeaponTransform = Weapon->GetActorTransform();
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    SavedMaxWalkSpeed = Movement->MaxWalkSpeed;
    Movement->StopMovementImmediately();
    Movement->MaxWalkSpeed = 0.0f;
    Movement->bOrientRotationToMovement = false;
    Movement->bUseControllerDesiredRotation = false;
    Character->bUseControllerRotationYaw = false;
    SetComponentTickEnabled(true);
    return true;
}

// 开窗采样当前姿势，避免从旧姿势或世界原点扫向武器。
void USoulsMeleeTraceComponent::SetHitWindowEnabled(bool bEnabled)
{
    if (!bAttackActive || !Weapon || !GetOwner()->HasAuthority())
    {
        return;
    }
    const bool bOpening = bEnabled && !bHitWindowEnabled;
    bHitWindowEnabled = bEnabled;
    if (bOpening)
    {
        PreviousWeaponTransform = Weapon->GetActorTransform();
        CheckInitialOverlap();
    }
}

// 释放攻击状态，死亡时不重新开启角色移动。
void USoulsMeleeTraceComponent::EndAttack()
{
    if (!bAttackActive)
    {
        return;
    }
    bAttackActive = false;
    bHitWindowEnabled = false;
    SetComponentTickEnabled(false);
    HitActors.Reset();
    if (ACharacter* Character = GetPawn<ACharacter>())
    {
        UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        Movement->MaxWalkSpeed = SavedMaxWalkSpeed;
        const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Character);
        if (!Health || (!Health->IsDeadOrDying() && Health->GetHealth() > 0.0f))
        {
            const USoulsLockOnComponent* Lock = Character->FindComponentByClass<USoulsLockOnComponent>();
            const bool bLocked = Lock && Lock->GetTargetActor();
            Movement->bOrientRotationToMovement = !bLocked;
            Movement->bUseControllerDesiredRotation = bLocked;
        }
    }
}

// 供锁定和输入扩展判断攻击是否拥有朝向控制权。
bool USoulsMeleeTraceComponent::IsAttackActive() const
{
    return bAttackActive;
}

// 供测试确认有效期或中断后的窗口状态。
bool USoulsMeleeTraceComponent::IsHitWindowEnabled() const
{
    return bHitWindowEnabled;
}

// 返回配置供能力创建 Montage 和伤害效果。
const USoulsAttackDefinition* USoulsMeleeTraceComponent::GetAttackDefinition() const
{
    return DefaultAttackDefinition;
}

// 返回实际附着的武器供效果来源和测试使用。
ASoulsMeleeWeapon* USoulsMeleeTraceComponent::GetWeapon() const
{
    return Weapon;
}

// 起手有限转向，进入有效期后保持动作承诺。
void USoulsMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bAttackActive || !Weapon || !DefaultAttackDefinition || !GetOwner()->HasAuthority())
    {
        EndAttack();
        return;
    }
    ACharacter* Character = GetPawn<ACharacter>();
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Character);
    if (!Character || (Health && (Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)))
    {
        EndAttack();
        return;
    }
    if (!bHitWindowEnabled && AttackElapsed < DefaultAttackDefinition->TurnDuration)
    {
        const USoulsLockOnComponent* Lock = Character->FindComponentByClass<USoulsLockOnComponent>();
        if (const AActor* Target = Lock ? Lock->GetTargetActor() : nullptr)
        {
            const FRotator Desired = (Target->GetActorLocation() - Character->GetActorLocation()).Rotation();
            const float Yaw = FMath::FixedTurn(Character->GetActorRotation().Yaw, Desired.Yaw,
                DefaultAttackDefinition->TurnRate * DeltaTime);
            Character->SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
        }
    }
    AttackElapsed += DeltaTime;
    if (bHitWindowEnabled)
    {
        SampleWeaponTrajectory();
    }
}

// 细分武器的平移与旋转，同时沿刀刃长度布置球形采样点。
void USoulsMeleeTraceComponent::SampleWeaponTrajectory()
{
    const FTransform Current = Weapon->GetActorTransform();
    const FTransform Previous = PreviousWeaponTransform;
    PreviousWeaponTransform = Current;
    const USoulsAttackDefinition* Definition = DefaultAttackDefinition;
    const float Travel = FMath::Max(
        FVector::Distance(Previous.TransformPosition(LocalBladeRoot), Current.TransformPosition(LocalBladeRoot)),
        FVector::Distance(Previous.TransformPosition(LocalBladeTip), Current.TransformPosition(LocalBladeTip)));
    const float Angle = FMath::RadiansToDegrees(Previous.GetRotation().AngularDistance(Current.GetRotation()));
    const int32 Steps = FMath::Clamp(FMath::Max(
        FMath::CeilToInt(Travel / FMath::Max(Definition->MaxSubstepDistance, 1.0f)),
        FMath::CeilToInt(Angle / FMath::Max(Definition->MaxSubstepAngle, 1.0f))),
        1, FMath::Clamp(Definition->MaxSubsteps, 1, 64));
    const int32 Samples = FMath::Clamp(Definition->BladeSampleCount, 2, 32);
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(SoulsMeleeTrace), false, GetOwner());
    Parameters.AddIgnoredActor(Weapon);
    Parameters.bFindInitialOverlaps = true;
    const FCollisionShape Sphere = FCollisionShape::MakeSphere(FMath::Max(Definition->TraceRadius, 1.0f));
    FTransform StepStart = Previous;
    for (int32 Step = 1; Step <= Steps && bHitWindowEnabled; ++Step)
    {
        FTransform StepEnd;
        StepEnd.Blend(Previous, Current, static_cast<float>(Step) / Steps);
        for (int32 Index = 0; Index < Samples && bHitWindowEnabled; ++Index)
        {
            const FVector LocalPoint = FMath::Lerp(LocalBladeRoot, LocalBladeTip, static_cast<float>(Index) / (Samples - 1));
            TArray<FHitResult> Hits;
            GetWorld()->SweepMultiByChannel(Hits, StepStart.TransformPosition(LocalPoint),
                StepEnd.TransformPosition(LocalPoint), FQuat::Identity, TraceChannel, Sphere, Parameters);
            for (const FHitResult& Hit : Hits)
            {
                ProcessHit(Hit);
                if (Hit.bBlockingHit || !bHitWindowEnabled)
                {
                    break;
                }
            }
        }
        StepStart = StepEnd;
    }
}

// 有效期开始就检查刀刃覆盖区域，包含上一帧已接触的目标。
void USoulsMeleeTraceComponent::CheckInitialOverlap()
{
    const int32 Samples = FMath::Clamp(DefaultAttackDefinition->BladeSampleCount, 2, 32);
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(SoulsMeleeInitial), false, GetOwner());
    Parameters.AddIgnoredActor(Weapon);
    for (int32 Index = 0; Index < Samples && bHitWindowEnabled; ++Index)
    {
        const FVector Point = Weapon->GetActorTransform().TransformPosition(
            FMath::Lerp(LocalBladeRoot, LocalBladeTip, static_cast<float>(Index) / (Samples - 1)));
        TArray<FOverlapResult> Overlaps;
        GetWorld()->OverlapMultiByChannel(Overlaps, Point, FQuat::Identity, TraceChannel,
            FCollisionShape::MakeSphere(FMath::Max(DefaultAttackDefinition->TraceRadius, 1.0f)), Parameters);
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (Overlap.GetActor() && Overlap.GetComponent())
            {
                FHitResult Hit(Overlap.GetActor(), Overlap.GetComponent(), Point, FVector::UpVector);
                Hit.TraceStart = Point;
                Hit.TraceEnd = Point;
                ProcessHit(Hit);
            }
        }
    }
}

// 按 ASC Avatar 统一多组件身份，命中集合先登记再通知以支持重入取消。
void USoulsMeleeTraceComponent::ProcessHit(const FHitResult& Hit)
{
    if (!bAttackActive || !bHitWindowEnabled || !GetOwner()->HasAuthority())
    {
        return;
    }
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hit.GetActor());
    AActor* Target = ASC ? ASC->GetAvatarActor() : nullptr;
    const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Target);
    if (!Target || Target == GetOwner() || HitActors.Contains(Target) ||
        (Health && (Health->IsDeadOrDying() || Health->GetHealth() <= 0.0f)))
    {
        return;
    }
    FVector Root;
    FVector Tip;
    Weapon->GetBladePoints(Root, Tip);
    if (!IsEnvironmentClear(GetOwner()->GetActorLocation(), Root, Target) ||
        !IsEnvironmentClear(Root, Hit.ImpactPoint, Target) ||
        !IsEnvironmentClear(GetOwner()->GetActorLocation(), Hit.ImpactPoint, Target))
    {
        return;
    }
    HitActors.Add(Target);
    OnHit.Broadcast(Hit);
}

// 只把静态和动态场景物作为遮挡，带能力系统的角色不充当环境墙体。
bool USoulsMeleeTraceComponent::IsEnvironmentClear(const FVector& Start, const FVector& End, const AActor* Target) const
{
    FCollisionQueryParams Parameters(SCENE_QUERY_STAT(SoulsMeleeOcclusion), false, GetOwner());
    Parameters.AddIgnoredActor(Weapon);
    Parameters.AddIgnoredActor(Target);
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(2.0f), Parameters);
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.GetActor() && !UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hit.GetActor()))
        {
            return false;
        }
    }
    return true;
}

// 场景卸载与能力中断都能安全释放本组件持有的临时状态。
void USoulsMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    EndAttack();
    OnHit.Clear();
    if (IsValid(Weapon) && GetOwner()->HasAuthority())
    {
        Weapon->Destroy();
    }
    Super::EndPlay(EndPlayReason);
}
