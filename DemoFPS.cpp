// DemoFPS.cpp : Simple FPS graphics demo for MainOS with Start button
#include <windows.h>
#include <mmsystem.h>
#include "DemoFPS.h"

#pragma comment(lib, "winmm.lib")

#define ID_BTN_START 2001

static BOOL demoRunning = FALSE;

// Window procedure for the demo (for button handling)
LRESULT CALLBACK DemoFPSWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        CreateWindowEx(0, L"BUTTON", L"Start Demo", WS_CHILD | WS_VISIBLE,
            10, 120, 100, 30, hWnd, (HMENU)ID_BTN_START, GetModuleHandle(NULL), NULL);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_START) {
            demoRunning = TRUE;
            SetTimer(hWnd, 1, 16, NULL); // ~60 FPS
        }
        break;
    case WM_TIMER:
        if (demoRunning) {
            // RunDemoFPS(hWnd); // This is no longer needed
        }
        break;
    case WM_DESTROY:
        KillTimer(hWnd, 1);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// Exported function to create the demo window
extern "C" __declspec(dllexport)
HWND CreateDemoFPSWindow(HWND hParent)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = DemoFPSWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"DemoFPSWindow";
    RegisterClass(&wc);

    return CreateWindowEx(0, L"DemoFPSWindow", L"Demo FPS", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, 300, 200, hParent, NULL, GetModuleHandle(NULL), NULL);
}

// Legacy functions for compatibility
extern "C" __declspec(dllexport)
void RunDemoFPS(HWND hWnd)
{
    // This function is kept for compatibility but not used in the new architecture
    // The new architecture uses the DemoFPS class methods instead
}

static DWORD lastTime = 0;
static int frameCount = 0;
static float fps = 0.0f;
static int animPos = 0;
static BOOL demoInitialized = FALSE;

void DemoFPS::Init(HWND hWnd) {
    demoInitialized = TRUE;
    lastTime = timeGetTime();
    frameCount = 0;
    fps = 0.0f;
    animPos = 0;
}

void DemoFPS::Update(float deltaTime) {
    if (!demoInitialized) return;
    
    // Update animation position
    animPos = (animPos + 2) % 400; // Move 2 pixels per frame, wrap at 400
    
    // Calculate FPS
    DWORD currentTime = timeGetTime();
    frameCount++;
    if (currentTime - lastTime >= 1000) {
        fps = frameCount * 1000.0f / (currentTime - lastTime);
        frameCount = 0;
        lastTime = currentTime;
    }
}

void DemoFPS::Render(HDC hdc) {
    if (!demoInitialized) return;
    
    RECT rect;
    GetClipBox(hdc, &rect);
    
    // Create animated rectangle
    HBRUSH brush = CreateSolidBrush(RGB(70, 130, 180));
    RECT animRect = { animPos, rect.top + 50, animPos + 50, rect.top + 100 };
    
    // Make sure rectangle stays within bounds
    if (animRect.right > rect.right) {
        animRect.left = rect.right - 50;
        animRect.right = rect.right;
    }
    
    FillRect(hdc, &animRect, brush);
    DeleteObject(brush);
    
    // Draw FPS counter
    WCHAR fpsText[64];
    wsprintf(fpsText, L"FPS: %.1f", fps);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    TextOut(hdc, rect.left + 10, rect.top + 10, fpsText, lstrlen(fpsText));
    
    // Draw demo info
    SetTextColor(hdc, RGB(180, 180, 180));
    TextOut(hdc, rect.left + 10, rect.top + 30, L"Systems Demo - Animated Rectangle", 34);
}

void DemoFPS::Shutdown() {
    demoInitialized = FALSE;
}