#include "Modules/ModuleManager.h"

class FArcadeCabVRSDKModule : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FArcadeCabVRSDKModule, ArcadeCabVRSDK)
