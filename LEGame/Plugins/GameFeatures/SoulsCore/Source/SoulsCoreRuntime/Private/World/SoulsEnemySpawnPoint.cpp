#include "World/SoulsEnemySpawnPoint.h"

#include "AI/AIPawnData.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "World/SoulsEncounterSubsystem.h"

#if WITH_EDITORONLY_DATA
#include "Components/ArrowComponent.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsEnemySpawnPoint)

// 建立出生变换，并在编辑器视口显示朝向。
ASoulsEnemySpawnPoint::ASoulsEnemySpawnPoint()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SpawnRoot"));
#if WITH_EDITORONLY_DATA
    if (UArrowComponent* Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("SpawnDirection")))
    {
        Arrow->SetupAttachment(RootComponent);
        Arrow->SetIsVisualizationComponent(true);
    }
#endif
}

// 登记锚点，实际生成由遭遇子系统统一调度。
void ASoulsEnemySpawnPoint::BeginPlay()
{
    Super::BeginPlay();
    if (UWorld* World = GetWorld(); World && IsValid(this) && !IsActorBeingDestroyed())
    {
        if (USoulsEncounterSubsystem* Encounters = World->GetSubsystem<USoulsEncounterSubsystem>())
        {
            Encounters->RegisterSpawnPoint(this);
        }
    }
}

// 撤销登记并清理实例，保留当前刷新周期的击杀状态。
void ASoulsEnemySpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (USoulsEncounterSubsystem* Encounters = World->GetSubsystem<USoulsEncounterSubsystem>())
        {
            Encounters->UnregisterSpawnPoint(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
// 只在编辑器世界放置时初始化标识，地图加载和 PIE 沿用序列化值。
void ASoulsEnemySpawnPoint::PostActorCreated()
{
    Super::PostActorCreated();
    if (!IsTemplate() && GetWorld() && GetWorld()->WorldType == EWorldType::Editor && !SpawnId.IsValid())
    {
        SpawnId = FGuid::NewGuid();
    }
}

// 复制品使用新标识，原锚点的存档键保持稳定。
void ASoulsEnemySpawnPoint::PostEditImport()
{
    Super::PostEditImport();
    if (!IsTemplate() && GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
    {
        SpawnId = FGuid::NewGuid();
    }
}
#endif
