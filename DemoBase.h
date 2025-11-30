#pragma once
#include <windows.h>

class DemoBase {
public:
    virtual ~DemoBase() {}
    virtual void Init(HWND hWnd) = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Render(HDC hdc) = 0;
    virtual void Shutdown() = 0;
};

// Forward declaration for LoadDemo
void LoadDemo(int demoId, HWND hWnd);