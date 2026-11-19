#include "Development/SoulsSetupLibrary.h"

#include "Interfaces/IPluginManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoulsSetupLibrary)

// 首次没有 GameFeatureData 时，也允许编辑器把生成资产写入插件目录。
bool USoulsSetupLibrary::PrepareContentForSetup()
{
#if WITH_EDITOR
    return IPluginManager::Get().MountExplicitlyLoadedPlugin(TEXT("SoulsCore"));
#else
    return false;
#endif
}
