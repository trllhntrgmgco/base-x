#pragma once
#include "DemoBase.h"
#include <windows.h>

class DemoTools : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;

    // optional input helpers
    void HandleKeyDown(int keyCode);

private:
    HWND hwnd = nullptr;
    bool showGrid = false;
    bool showDebug = false;
    int screenshotCounter = 0;
    unsigned long lastKeyToggleTime = 0;

    void TakeScreenshot();
};
