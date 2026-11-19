#include "Character/SoulsPawnData.h"

#include "Camera/SoulsCameraMode.h"
#include "Character/SoulsCharacter.h"
#include "Input/SoulsInputConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsPawnData)

// 内置输入配置保证新建 PawnData 即具备基础控制。
USoulsPawnData::USoulsPawnData(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PawnClass = ASoulsCharacter::StaticClass();
    InputConfig = CreateDefaultSubobject<USoulsInputConfig>(TEXT("SoulsInputConfig"));
    DefaultCameraMode = USoulsCameraMode::StaticClass();
}
