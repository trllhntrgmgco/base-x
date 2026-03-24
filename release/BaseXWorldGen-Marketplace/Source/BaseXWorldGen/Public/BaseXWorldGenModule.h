#pragma once

#include "Modules/ModuleManager.h"

class FBaseXWorldGenModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
