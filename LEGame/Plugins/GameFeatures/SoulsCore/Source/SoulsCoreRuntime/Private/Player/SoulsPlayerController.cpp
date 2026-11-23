#include "Player/SoulsPlayerController.h"

#include "GameFramework/Pawn.h"
#include "Player/Input/SoulsHeroComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsPlayerController)

// 沿用 Lyra 的玩家控制器初始化。
ASoulsPlayerController::ASoulsPlayerController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

// 在 Lyra 处理输入时序之前，派发本次已经完成采集的战斗意图。
void ASoulsPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
    if (APawn* ControlledPawn = GetPawn())
    {
        if (USoulsHeroComponent* Hero = ControlledPawn->FindComponentByClass<USoulsHeroComponent>())
        {
            Hero->FlushCombatInput();
        }
    }
    Super::PostProcessInput(DeltaTime, bGamePaused);
}
