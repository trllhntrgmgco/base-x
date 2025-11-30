#pragma once
#include "DemoBase.h"

// Forward declaration for RunDemoFPS
extern "C" __declspec(dllexport) void RunDemoFPS(HWND hWnd);

class DemoFPS : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
};