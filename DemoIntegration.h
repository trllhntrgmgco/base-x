#pragma once
#include "DemoBase.h"
#include <windows.h>

class DemoIntegration : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
};
