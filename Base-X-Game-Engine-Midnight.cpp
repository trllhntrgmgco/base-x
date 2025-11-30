// Base-X-Game-Engine-Midnight.cpp : "Midnight Edition" dark mode for Base-X Game Engine
#include "framework.h"
#include "Base-X-Game-Engine-X.h"
#include "resource.h"
#include <commctrl.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

#define MAX_LOADSTRING 100

// Midnight Edition Global Variables
HINSTANCE hInstMidnight;
WCHAR szTitleMidnight[MAX_LOADSTRING];
WCHAR szWindowClassMidnight[MAX_LOADSTRING];
HBRUSH hBrushMidnightBackground = nullptr;
HBRUSH hBrushMidnightText = nullptr;

ATOM MyRegisterClassMidnight(HINSTANCE hInstance);
BOOL InitInstanceMidnight(HINSTANCE, int);
LRESULT CALLBACK WndProcMidnight(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitleMidnight, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BASEXGAMEENGINEX, szWindowClassMidnight, MAX_LOADSTRING);
    MyRegisterClassMidnight(hInstance);

    if (!InitInstanceMidnight(hInstance, nCmdShow))
    {
        return FALSE;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClassMidnight(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProcMidnight;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BASEXGAMEENGINEX));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_BASEXGAMEENGINEX);
    wcex.lpszClassName  = szWindowClassMidnight;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

BOOL InitInstanceMidnight(HINSTANCE hInstance, int nCmdShow)
{
    hInstMidnight = hInstance;

    HWND hWnd = CreateWindowW(szWindowClassMidnight, szTitleMidnight, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Midnight Edition brushes
    hBrushMidnightBackground = CreateSolidBrush(RGB(10, 10, 30)); // deep midnight blue
    hBrushMidnightText = CreateSolidBrush(RGB(200, 200, 255));    // soft blue-white

    return TRUE;
}

LRESULT CALLBACK WndProcMidnight(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);
            FillRect(hdc, &rect, hBrushMidnightBackground);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(200, 200, 255));
            TextOut(hdc, 20, 20, L"Midnight Edition", 16);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(10, 10, 30));
            SetTextColor(hdc, RGB(200, 200, 255));
            return (INT_PTR)hBrushMidnightBackground;
        }
    case WM_CTLCOLORDLG:
        return (INT_PTR)hBrushMidnightBackground;
    case WM_DESTROY:
        if (hBrushMidnightBackground) {
            DeleteObject(hBrushMidnightBackground);
            hBrushMidnightBackground = nullptr;
        }
        if (hBrushMidnightText) {
            DeleteObject(hBrushMidnightText);
            hBrushMidnightText = nullptr;
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}