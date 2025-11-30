// Base-X-Game-Engine-X.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Base-X-Game-Engine-X.h"
#include "resource.h"
#include <commctrl.h>
#include <commdlg.h>  // Add for dialog box support
#include <mmsystem.h>

// Demo includes
#include "DemoBase.h"
#include "..\DemoFPS.h"
#include "..\DemoGraphics.h"
#include "..\DemoAudio.h"
#include "DemoPhysicsCollision.h"
#include "DemoGolf.h"
#include "DemoWorld3D.h"
#include "DemoSolarSystem.h"
#include "DemoPhysicsLayer3.h"
#include "DemoPhysicsBall.h"
#include "DemoPhysicsGravity.h"
#include "DemoPhysicsParticles.h"
#include "DemoSoftBody.h"
#include "DemoCloth.h"
#include "DemoFluid.h"
#include "DemoIntegration.h"
#include "DemoNetworking.h"
#include "DemoAI.h"
#include "DemoTools.h"
#include "DemoTextures.h"
#include "DemoArtGallery.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comctl32.lib")

#define MAX_LOADSTRING 100

// Midnight Edition Color Scheme
#define MIDNIGHT_BACKGROUND RGB(8, 8, 25)        // Deep midnight blue-black
#define MIDNIGHT_SURFACE RGB(15, 15, 35)         // Dark surface color
#define MIDNIGHT_ACCENT RGB(25, 25, 50)          // Slightly lighter accent
#define MIDNIGHT_TEXT RGB(200, 205, 255)         // Soft blue-white text
#define MIDNIGHT_TEXT_DIM RGB(150, 155, 200)     // Dimmer text
#define MIDNIGHT_BORDER RGB(40, 45, 80)          // Subtle border color
#define MIDNIGHT_EDIT_BG RGB(18, 18, 40)         // Edit control background

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
// Midnight Edition Dark Theme Brushes
HBRUSH hBrushBackground = nullptr;
HBRUSH hBrushText = nullptr;
HBRUSH hBrushMidnightAccent = nullptr;
DWORD lastTime = 0;
int frameCount = 0;
float fps = 0.0f;
DemoBase* currentDemo = nullptr;
HWND hTabControl = nullptr;

// Add fullscreen support variable
bool isFullscreen = false;
RECT windowedRect;              // Store windowed position for fullscreen toggle

// Physics settings global variables
float globalGravity = 500.0f;
float globalBounce = 0.8f;
float globalFriction = 0.95f;

// New Graphics/Audio settings globals
int graphicsResolutionIndex = 0; // 0=800x600,1=1024x768,2=1280x720,3=1920x1080
bool graphicsFullscreenSetting = false;
bool graphicsVSync = true;
int graphicsTextureQuality = 2; // 0=Low,1=Med,2=High

float audioMasterVolume = 1.0f;
float audioMusicVolume = 0.8f;
float audioEffectsVolume = 0.8f;
bool audioMuted = false;

// Prototype for LoadDemo to ensure it's visible before WndProc
void LoadDemo(int demoId, HWND hWnd);
void LoadDemoFromMenu(int menuId, HWND hWnd);
void UpdateDemo(float deltaTime);

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    SettingsDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

void PollJoystick()
{
    JOYINFOEX joyInfo;
    joyInfo.dwSize = sizeof(JOYINFOEX);
    joyInfo.dwFlags = JOY_RETURNALL;

    if (joyGetPosEx(JOYSTICKID1, &joyInfo) == JOYERR_NOERROR)
    {
        // joyInfo.dwXpos, joyInfo.dwYpos, joyInfo.dwButtons, etc.
        // TODO: Use joystick data in your engine
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BASEXGAMEENGINEX, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BASEXGAMEENGINEX));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BASEXGAMEENGINEX));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_BASEXGAMEENGINEX);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, L"Base-X Game Engine - Midnight Edition", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 800, 600, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   // Store initial window rect for fullscreen toggle
   GetWindowRect(hWnd, &windowedRect);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   // Initialize common controls
   INITCOMMONCONTROLSEX icex;
   icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
   icex.dwICC = ICC_TAB_CLASSES;
   InitCommonControlsEx(&icex);

   // Create tab control with proper positioning and midnight styling
   hTabControl = CreateWindowEx(0, WC_TABCONTROL, L"", 
       WS_CHILD | WS_VISIBLE | WS_TABSTOP,
       10, 40, 760, 40, hWnd, (HMENU)5001, hInst, nullptr);

   if (hTabControl) {
       // Set tab control to use midnight colors
       SendMessage(hTabControl, CCM_SETBKCOLOR, 0, (LPARAM)MIDNIGHT_SURFACE);
       
       TCITEM tie;
       tie.mask = TCIF_TEXT;
       
       // Tab 0: Systems (FPS demo)
       tie.pszText = const_cast<LPWSTR>(L"Systems");
       TabCtrl_InsertItem(hTabControl, 0, &tie);

       // Tab 1: Graphics demo
       tie.pszText = const_cast<LPWSTR>(L"Graphics");
       TabCtrl_InsertItem(hTabControl, 1, &tie);

       // Tab 2: Physics demo
       tie.pszText = const_cast<LPWSTR>(L"Physics");
       TabCtrl_InsertItem(hTabControl, 2, &tie);

       // Tab 3: Audio demo
       tie.pszText = const_cast<LPWSTR>(L"Audio");
       TabCtrl_InsertItem(hTabControl, 3, &tie);

       // Tab 4: Golf 3D demo
       tie.pszText = const_cast<LPWSTR>(L"Golf 3D");
       TabCtrl_InsertItem(hTabControl, 4, &tie);

       // Tab 5: World 3D demo
       tie.pszText = const_cast<LPWSTR>(L"World 3D");
       TabCtrl_InsertItem(hTabControl, 5, &tie);

       // Tab 6: Solar System demo
       tie.pszText = const_cast<LPWSTR>(L"Solar System");
       TabCtrl_InsertItem(hTabControl, 6, &tie);
   }

   // Initialize Midnight Edition brushes
   hBrushBackground = CreateSolidBrush(MIDNIGHT_BACKGROUND);   // Deep midnight background
   hBrushText = CreateSolidBrush(MIDNIGHT_SURFACE);            // Surface brush for controls
   hBrushMidnightAccent = CreateSolidBrush(MIDNIGHT_ACCENT);   // Accent brush for highlights

   // Load initial demo
   LoadDemo(0, hWnd);

   // Start high-resolution timer for smoother frame timing
   timeBeginPeriod(1);
   // Use SetTimer still, but measure precise delta between ticks
   SetTimer(hWnd, 1, 16, nullptr);

   // initialize lastTime used for update delta measurement
   lastTime = timeGetTime();

   return TRUE;
}



//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        // Main demo menu items
        case IDM_DEMO_SYSTEMS:
            LoadDemo(0, hWnd);
            TabCtrl_SetCurSel(hTabControl, 0);
            break;
        case IDM_DEMO_GRAPHICS:
            LoadDemo(1, hWnd);
            TabCtrl_SetCurSel(hTabControl, 1);
            break;
        case IDM_DEMO_PHYSICS:
        case IDM_PHYSICS_COLLISION:
            LoadDemo(2, hWnd);
            TabCtrl_SetCurSel(hTabControl, 2);
            break;
        case IDM_DEMO_AUDIO:
            LoadDemo(3, hWnd);
            TabCtrl_SetCurSel(hTabControl, 3);
            break;
        case IDM_DEMO_3D_WORLD:
            LoadDemo(5, hWnd);
            TabCtrl_SetCurSel(hTabControl, 5);
            break;
        case IDM_DEMO_GOLF:
            LoadDemo(4, hWnd);
            TabCtrl_SetCurSel(hTabControl, 4);
            break;
        case IDM_DEMO_SOLAR_SYSTEM:
            LoadDemo(6, hWnd);
            TabCtrl_SetCurSel(hTabControl, 6);
            break;
        // Physics submenu items
        case IDM_PHYSICS_BALL:
            LoadDemoFromMenu(IDM_PHYSICS_BALL, hWnd);
            break;
        case IDM_PHYSICS_GRAVITY:
            LoadDemoFromMenu(IDM_PHYSICS_GRAVITY, hWnd);
            break;
        case IDM_PHYSICS_RESET:
            if (currentDemo) {
                currentDemo->Shutdown();
                currentDemo->Init(hWnd);
            }
            break;
        case IDM_PHYSICS_PAUSE:
            // Toggle pause - could be implemented in demos
            MessageBox(hWnd, L"Pause/Resume functionality would be implemented here", L"Physics Control", MB_OK | MB_ICONINFORMATION);
            break;
        // Advanced demo examples
        case IDM_DEMO_PARTICLES:
            LoadDemoFromMenu(IDM_DEMO_PARTICLES, hWnd);
            break;
        case IDM_DEMO_SOFTBODY:
            LoadDemoFromMenu(IDM_DEMO_SOFTBODY, hWnd);
            break;
        case IDM_DEMO_CLOTH:
            LoadDemoFromMenu(IDM_DEMO_CLOTH, hWnd);
            break;
        case IDM_DEMO_FLUID:
            LoadDemoFromMenu(IDM_DEMO_FLUID, hWnd);
            break;
        case IDM_DEMO_INTEGRATION:
            LoadDemoFromMenu(IDM_DEMO_INTEGRATION, hWnd);
            break;
        case IDM_DEMO_NETWORKING:
            LoadDemoFromMenu(IDM_DEMO_NETWORKING, hWnd);
            break;
        case IDM_DEMO_AI:
            LoadDemoFromMenu(IDM_DEMO_AI, hWnd);
            break;
        case IDM_DEMO_TOOLS:
            LoadDemoFromMenu(IDM_DEMO_TOOLS, hWnd);
            break;
        case IDM_DEMO_TEXTURES:
            LoadDemoFromMenu(IDM_DEMO_TEXTURES, hWnd);
            break;
        case IDM_DEMO_ARTGALLERY:
            LoadDemoFromMenu(IDM_DEMO_ARTGALLERY, hWnd);
            break;
        // View menu items
        case IDM_VIEW_800x600:
            if (isFullscreen) break; // Don't resize in fullscreen
            SetWindowPos(hWnd, nullptr, 0, 0, 800, 600, SWP_NOMOVE | SWP_NOZORDER);
            break;
        case IDM_VIEW_1024x768:
            if (isFullscreen) break;
            SetWindowPos(hWnd, nullptr, 0, 0, 1024, 768, SWP_NOMOVE | SWP_NOZORDER);
            break;
        case IDM_VIEW_1280x720:
            if (isFullscreen) break;
            SetWindowPos(hWnd, nullptr, 0, 0, 1280, 720, SWP_NOMOVE | SWP_NOZORDER);
            break;
        case IDM_VIEW_1920x1080:
            if (isFullscreen) break;
            SetWindowPos(hWnd, nullptr, 0, 0, 1920, 1080, SWP_NOMOVE | SWP_NOZORDER);
            break;
        case IDM_VIEW_FULLSCREEN:
            {
                if (!isFullscreen) {
                    // Go to fullscreen
                    GetWindowRect(hWnd, &windowedRect);
                    SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
                    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
                    SetWindowPos(hWnd, HWND_TOP, 0, 0, screenWidth, screenHeight, 
                                SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    isFullscreen = true;
                } else {
                    // Return to windowed
                    SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                    SetWindowPos(hWnd, HWND_NOTOPMOST, 
                                windowedRect.left, windowedRect.top,
                                windowedRect.right - windowedRect.left,
                                windowedRect.bottom - windowedRect.top,
                                SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                    isFullscreen = false;
                }
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            break;
        case IDM_SETTINGS:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hWnd, SettingsDialogProc);
            break;
        case IDM_SETTINGS_PHYSICS:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hWnd, SettingsDialogProc);
            break;
        case IDM_ABOUT: 
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        }
    }
    break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rect;
            GetClientRect(hWnd, &rect);

            // Create back buffer
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP hbm = CreateCompatibleBitmap(hdc, rect.right - rect.left, rect.bottom - rect.top);
            HBITMAP hOldBmp = (HBITMAP)SelectObject(memDC, hbm);

            // Make a local rect for drawing into the backbuffer
            RECT localRect = rect;

            // Adjust rect to not paint over controls (unless fullscreen)
            if (!isFullscreen) {
                localRect.top = 90;

                // Paint the header area with a subtle gradient effect on the backbuffer
                RECT headerRect = {0, 0, localRect.right, 90};

                // Create a subtle gradient from midnight background to slightly lighter
                TRIVERTEX vertex[2];
                vertex[0].x = 0;
                vertex[0].y = 0;
                vertex[0].Red = GetRValue(MIDNIGHT_BACKGROUND) << 8;
                vertex[0].Green = GetGValue(MIDNIGHT_BACKGROUND) << 8;
                vertex[0].Blue = GetBValue(MIDNIGHT_BACKGROUND) << 8;
                vertex[0].Alpha = 0x0000;

                vertex[1].x = localRect.right;
                vertex[1].y = 90;
                vertex[1].Red = GetRValue(MIDNIGHT_SURFACE) << 8;
                vertex[1].Green = GetGValue(MIDNIGHT_SURFACE) << 8;
                vertex[1].Blue = GetBValue(MIDNIGHT_SURFACE) << 8;
                vertex[1].Alpha = 0x0000;

                GRADIENT_RECT gRect = {0, 1};
                GradientFill(memDC, vertex, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

                // Add a subtle border line at the bottom of the header
                HPEN hPen = CreatePen(PS_SOLID, 1, MIDNIGHT_BORDER);
                HPEN hOldPen = (HPEN)SelectObject(memDC, hPen);
                MoveToEx(memDC, 0, 89, nullptr);
                LineTo(memDC, localRect.right, 89);
                SelectObject(memDC, hOldPen);
                DeleteObject(hPen);
            }

            // Fill the main demo area in the backbuffer
            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, hBrushBackground);
            FillRect(memDC, &localRect, hBrushBackground);
            SelectObject(memDC, oldBrush);

            // Let current demo render into the backbuffer
            if (currentDemo) {
                currentDemo->Render(memDC);
            }

            // Add a subtle midnight watermark in the corner when not fullscreen
            if (!isFullscreen) {
                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, MIDNIGHT_TEXT_DIM);
                HFONT hFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);
                TextOut(memDC, localRect.right - 120, localRect.bottom - 20, L"Midnight Edition", 16);
                SelectObject(memDC, hOldFont);
                DeleteObject(hFont);
            }

            // Blit backbuffer to screen
            BitBlt(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top, memDC, 0, 0, SRCCOPY);

            // Clean up backbuffer objects
            SelectObject(memDC, hOldBmp);
            DeleteObject(hbm);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        KillTimer(hWnd, 1); // Stop the update timer
        if (currentDemo) {
            currentDemo->Shutdown();
            delete currentDemo;
            currentDemo = nullptr;
        }
        // Clean up Midnight Edition brushes
        if (hBrushBackground) {
            DeleteObject(hBrushBackground);
            hBrushBackground = nullptr;
        }
        if (hBrushText) {
            DeleteObject(hBrushText);
            hBrushText = nullptr;
        }
        if (hBrushMidnightAccent) {
            DeleteObject(hBrushMidnightAccent);
            hBrushMidnightAccent = nullptr;
        }
        // End high-resolution timer
        timeEndPeriod(1);
        PostQuitMessage(0);
        break;

    case WM_TIMER:
        if (wParam == 1) { // Our demo update timer
            if (currentDemo) {
                try {
                    // compute high-resolution delta since last update
                    DWORD currentTick = timeGetTime();
                    DWORD deltaMs = currentTick - lastTime;
                    if (deltaMs == 0) deltaMs = 1; // avoid zero
                    // clamp large deltas to avoid huge jumps
                    if (deltaMs > 200) deltaMs = 200;
                    lastTime = currentTick;

                    currentDemo->Update((float)deltaMs); // pass actual ms

                    // Invalidate only the demo area to avoid flickering
                    RECT rect;
                    GetClientRect(hWnd, &rect);
                    if (!isFullscreen) {
                        rect.top = 90;
                    }
                    InvalidateRect(hWnd, &rect, FALSE);
                } catch (...) {
                    // If demo update crashes, safely reload a working demo
                    try {
                        if (currentDemo) {
                            currentDemo->Shutdown();
                            delete currentDemo;
                        }
                        currentDemo = nullptr;
                        
                        // Try to load FPS demo as fallback
                        currentDemo = new DemoFPS();
                        if (currentDemo) {
                            currentDemo->Init(hWnd);
                            // Reset tab to Systems (FPS demo)
                            if (hTabControl) {
                                TabCtrl_SetCurSel(hTabControl, 0);
                            }
                        }
                    } catch (...) {
                        // If even FPS demo fails, just set to null
                        if (currentDemo) {
                            try {
                                delete currentDemo;
                            } catch (...) {}
                            currentDemo = nullptr;
                        }
                    }
                }
            }
        }
        break;
    case WM_SIZE:
        {
            // Resize tab control when window is resized (not in fullscreen)
            if (!isFullscreen) {
                RECT rect;
                GetClientRect(hWnd, &rect);
                if (hTabControl) {
                    SetWindowPos(hTabControl, nullptr, 10, 40, rect.right - 20, 40, SWP_NOZORDER);
                }
            }
        }
        break;
    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);

        // Check if click is in the demo area (below controls or anywhere in fullscreen)
        int minY = isFullscreen ? 0 : 90;
        if (y > minY && currentDemo) {
            // Forward to current demo if it supports input with enhanced error handling
            try {
                DemoPhysicsCollision* physicsDemo = dynamic_cast<DemoPhysicsCollision*>(currentDemo);
                if (physicsDemo) {
                    physicsDemo->HandleMouseDown(x, y, true);
                } else {
                    DemoGolf* golfDemo = dynamic_cast<DemoGolf*>(currentDemo);
                    if (golfDemo) {
                        golfDemo->HandleMouseDown(x, y, true);
                    } else {
                        DemoSolarSystem* solarDemo = dynamic_cast<DemoSolarSystem*>(currentDemo);
                        if (solarDemo) {
                            solarDemo->HandleMouseDown(x, y, true);
                        } else {
                            DemoTextures* texturesDemo = dynamic_cast<DemoTextures*>(currentDemo);
                            if (texturesDemo) {
                                texturesDemo->HandleMouseDown(x, y, true);
                            } else {
                                // Handle other demos with mouse input here
                                DemoPhysicsLayer3* layer3Demo = dynamic_cast<DemoPhysicsLayer3*>(currentDemo);
                                if (layer3Demo) {
                                    layer3Demo->HandleMouseDown(x, y, true);
                                }
                            }
                        }
                    }
                }

                // Also handle ArtGallery if active
                DemoArtGallery* artDemo = dynamic_cast<DemoArtGallery*>(currentDemo);
                if (artDemo) {
                    artDemo->HandleMouseDown(x, y, true);
                }

            } catch (...) {
                // If demo crashes, safely reload the FPS demo
                try {
                    if (currentDemo) {
                        currentDemo->Shutdown();
                        delete currentDemo;
                    }
                    currentDemo = new DemoFPS();
                    if (currentDemo) {
                        currentDemo->Init(hWnd);
                    }
                    // Reset tab to Systems
                    if (hTabControl) {
                        TabCtrl_SetCurSel(hTabControl, 0);
                    }
                } catch (...) {
                    currentDemo = nullptr;
                }
            }
        }
    }
    break;
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        
        if (currentDemo) {
            try {
                DemoPhysicsCollision* physicsDemo = dynamic_cast<DemoPhysicsCollision*>(currentDemo);
                if (physicsDemo) {
                    physicsDemo->HandleMouseMove(x, y);
                } else {
                    DemoGolf* golfDemo = dynamic_cast<DemoGolf*>(currentDemo);
                    if (golfDemo) {
                        golfDemo->HandleMouseMove(x, y);
                    } else {
                        DemoSolarSystem* solarDemo = dynamic_cast<DemoSolarSystem*>(currentDemo);
                        if (solarDemo) {
                            solarDemo->HandleMouseMove(x, y);
                        } else {
                            DemoTextures* texturesDemo = dynamic_cast<DemoTextures*>(currentDemo);
                            if (texturesDemo) {
                                texturesDemo->HandleMouseMove(x, y);
                            } else {
                                // Handle other demos with mouse input here
                                DemoPhysicsLayer3* layer3Demo = dynamic_cast<DemoPhysicsLayer3*>(currentDemo);
                                if (layer3Demo) {
                                    layer3Demo->HandleMouseMove(x, y);
                                }
                            }
                        }
                    }
                }

                // Also forward to ArtGallery if active
                DemoArtGallery* artDemo = dynamic_cast<DemoArtGallery*>(currentDemo);
                if (artDemo) {
                    artDemo->HandleMouseMove(x, y);
                }

            } catch (...) {
                // Silently continue on mouse move errors to avoid disrupting user experience
            }
        }
    }
    break;
    case WM_LBUTTONUP:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        
        if (currentDemo) {
            try {
                DemoPhysicsCollision* physicsDemo = dynamic_cast<DemoPhysicsCollision*>(currentDemo);
                if (physicsDemo) {
                    physicsDemo->HandleMouseUp(x, y, true);
                } else {
                    DemoGolf* golfDemo = dynamic_cast<DemoGolf*>(currentDemo);
                    if (golfDemo) {
                        golfDemo->HandleMouseUp(x, y, true);
                    } else {
                        DemoSolarSystem* solarDemo = dynamic_cast<DemoSolarSystem*>(currentDemo);
                        if (solarDemo) {
                            solarDemo->HandleMouseUp(x, y, true);
                        } else {
                            DemoTextures* texturesDemo = dynamic_cast<DemoTextures*>(currentDemo);
                            if (texturesDemo) {
                                texturesDemo->HandleMouseUp(x, y, true);
                            } else {
                                // Handle other demos with mouse input here
                                DemoPhysicsLayer3* layer3Demo = dynamic_cast<DemoPhysicsLayer3*>(currentDemo);
                                if (layer3Demo) {
                                    layer3Demo->HandleMouseUp(x, y, true);
                                }
                            }
                        }
                    }
                }

                // Also forward to ArtGallery if active
                DemoArtGallery* artDemo = dynamic_cast<DemoArtGallery*>(currentDemo);
                if (artDemo) {
                    artDemo->HandleMouseUp(x, y, true);
                }

            } catch (...) {
                // Silently continue on mouse up errors
            }
        }
    }
    break;
    case WM_KEYDOWN:
    {
        // Handle ESC key to exit fullscreen
        if (wParam == VK_ESCAPE && isFullscreen) {
            SendMessage(hWnd, WM_COMMAND, IDM_VIEW_FULLSCREEN, 0);
        }
        
        // Forward to current demo with error handling
        if (currentDemo) {
            try {
                DemoPhysicsCollision* physicsDemo = dynamic_cast<DemoPhysicsCollision*>(currentDemo);
                if (physicsDemo) {
                    physicsDemo->HandleKeyDown((int)wParam);
                } else {
                    DemoGolf* golfDemo = dynamic_cast<DemoGolf*>(currentDemo);
                    if (golfDemo) {
                        golfDemo->HandleKeyDown((int)wParam);
                    } else {
                        DemoSolarSystem* solarDemo = dynamic_cast<DemoSolarSystem*>(currentDemo);
                        if (solarDemo) {
                            solarDemo->HandleKeyDown((int)wParam);
                        } else {
                            DemoTextures* texturesDemo = dynamic_cast<DemoTextures*>(currentDemo);
                            if (texturesDemo) {
                                texturesDemo->HandleKeyDown((int)wParam);
                            } else {
                                // Handle other demos with keyboard input here
                                DemoPhysicsLayer3* layer3Demo = dynamic_cast<DemoPhysicsLayer3*>(currentDemo);
                                if (layer3Demo) {
                                    layer3Demo->HandleKeyDown((int)wParam);
                                }
                            }
                        }
                    }
                }

                // Also forward to ArtGallery if active
                DemoArtGallery* artDemo = dynamic_cast<DemoArtGallery*>(currentDemo);
                if (artDemo) {
                    artDemo->HandleKeyDown((int)wParam);
                }

                // Also forward to Tools if active
                DemoTools* toolsDemo = dynamic_cast<DemoTools*>(currentDemo);
                if (toolsDemo) {
                    toolsDemo->HandleKeyDown((int)wParam);
                }

            } catch (...) {
                // Silently continue on key down errors
            }
        }
    }
    break;
    case WM_KEYUP:
    {
        if (currentDemo) {
            try {
                DemoPhysicsCollision* physicsDemo = dynamic_cast<DemoPhysicsCollision*>(currentDemo);
                if (physicsDemo) {
                    physicsDemo->HandleKeyUp((int)wParam);
                } else {
                    DemoGolf* golfDemo = dynamic_cast<DemoGolf*>(currentDemo);
                    if (golfDemo) {
                        golfDemo->HandleKeyUp((int)wParam);
                    } else {
                        DemoSolarSystem* solarDemo = dynamic_cast<DemoSolarSystem*>(currentDemo);
                        if (solarDemo) {
                            solarDemo->HandleKeyUp((int)wParam);
                        } else {
                            DemoTextures* texturesDemo = dynamic_cast<DemoTextures*>(currentDemo);
                            if (texturesDemo) {
                                texturesDemo->HandleKeyUp((int)wParam);
                            } else {
                                // Handle other demos with keyboard input here
                                DemoPhysicsLayer3* layer3Demo = dynamic_cast<DemoPhysicsLayer3*>(currentDemo);
                                if (layer3Demo) {
                                    layer3Demo->HandleKeyUp((int)wParam);
                                }
                            }
                        }
                    }
                }

                // Also forward to ArtGallery if active
                DemoArtGallery* artDemo = dynamic_cast<DemoArtGallery*>(currentDemo);
                if (artDemo) {
                    artDemo->HandleKeyUp((int)wParam);
                }

            } catch (...) {
                // Silently continue on key up errors
            }
        }
    }
    break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        if (!isFullscreen) { // Only apply custom colors when not fullscreen
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, MIDNIGHT_SURFACE);
            SetTextColor(hdc, MIDNIGHT_TEXT);
            return (INT_PTR)hBrushText;
        }
    }
    break;
    case WM_CTLCOLORDLG:
        if (!isFullscreen) {
            return (INT_PTR)hBrushBackground;
        }
        break;
    case WM_CTLCOLOREDIT:
    {
        if (!isFullscreen) {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, MIDNIGHT_EDIT_BG);
            SetTextColor(hdc, MIDNIGHT_TEXT);
            return (INT_PTR)CreateSolidBrush(MIDNIGHT_EDIT_BG);
        }
    }
    break;
    case WM_NOTIFY:
    {
        LPNMHDR lpnmhdr = (LPNMHDR)lParam;
        if (lpnmhdr->code == TCN_SELCHANGE && lpnmhdr->hwndFrom == hTabControl) {
            int sel = TabCtrl_GetCurSel(hTabControl);
            LoadDemo(sel, hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        // Handle custom draw for tab control to apply midnight theme
        else if (lpnmhdr->code == NM_CUSTOMDRAW && lpnmhdr->hwndFrom == hTabControl) {
            LPNMCUSTOMDRAW lpnmcd = (LPNMCUSTOMDRAW)lParam;
            if (lpnmcd->dwDrawStage == CDDS_PREPAINT) {
                return CDRF_NOTIFYITEMDRAW;
            }
            else if (lpnmcd->dwDrawStage == CDDS_ITEMPREPAINT) {
                SetBkColor(lpnmcd->hdc, MIDNIGHT_SURFACE);
                SetTextColor(lpnmcd->hdc, MIDNIGHT_TEXT);
                return CDRF_NEWFONT;
            }
        }
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Message handler for settings dialog box.
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    static HWND hTab = nullptr;
    static HWND hResCombo = nullptr;
    static HWND hFullscreenChk = nullptr;
    static HWND hVSyncChk = nullptr;
    static HWND hTexCombo = nullptr;

    static HWND hMasterEdit = nullptr;
    static HWND hMusicEdit = nullptr;
    static HWND hEffectsEdit = nullptr;
    static HWND hMuteChk = nullptr;

    static HWND hGravityEdit = nullptr;
    static HWND hBounceEdit = nullptr;
    static HWND hFrictionEdit = nullptr;

    switch (message)
    {
    case WM_INITDIALOG:
        {
            // Create tab control
            hTab = CreateWindowEx(0, WC_TABCONTROL, L"",
                WS_CHILD | WS_VISIBLE | TCS_TABS,
                10, 10, 420, 26, hDlg, (HMENU)IDC_SETTINGS_TAB, hInst, nullptr);

            TCITEM tie;
            tie.mask = TCIF_TEXT;
            tie.pszText = const_cast<LPWSTR>(L"Graphics");
            TabCtrl_InsertItem(hTab, 0, &tie);
            tie.pszText = const_cast<LPWSTR>(L"Audio");
            TabCtrl_InsertItem(hTab, 1, &tie);
            tie.pszText = const_cast<LPWSTR>(L"Physics");
            TabCtrl_InsertItem(hTab, 2, &tie);

            RECT rcClient;
            GetClientRect(hDlg, &rcClient);

            // Graphics controls
            hResCombo = CreateWindowEx(0, WC_COMBOBOX, nullptr,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                20, 50, 200, 200, hDlg, (HMENU)IDC_SETTINGS_RESOLUTION, hInst, nullptr);
            SendMessage(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"800 x 600");
            SendMessage(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"1024 x 768");
            SendMessage(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"1280 x 720");
            SendMessage(hResCombo, CB_ADDSTRING, 0, (LPARAM)L"1920 x 1080");
            SendMessage(hResCombo, CB_SETCURSEL, (WPARAM)graphicsResolutionIndex, 0);

            hFullscreenChk = CreateWindowEx(0, L"BUTTON", L"Fullscreen",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                240, 50, 120, 20, hDlg, (HMENU)IDC_SETTINGS_FULLSCREEN, hInst, nullptr);
            if (graphicsFullscreenSetting) SendMessage(hFullscreenChk, BM_SETCHECK, BST_CHECKED, 0);

            hVSyncChk = CreateWindowEx(0, L"BUTTON", L"VSync",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                240, 80, 120, 20, hDlg, (HMENU)IDC_SETTINGS_VSYNC, hInst, nullptr);
            if (graphicsVSync) SendMessage(hVSyncChk, BM_SETCHECK, BST_CHECKED, 0);

            hTexCombo = CreateWindowEx(0, WC_COMBOBOX, nullptr,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                20, 90, 200, 200, hDlg, (HMENU)IDC_SETTINGS_TEXTURE_QUALITY, hInst, nullptr);
            SendMessage(hTexCombo, CB_ADDSTRING, 0, (LPARAM)L"Low");
            SendMessage(hTexCombo, CB_ADDSTRING, 0, (LPARAM)L"Medium");
            SendMessage(hTexCombo, CB_ADDSTRING, 0, (LPARAM)L"High");
            SendMessage(hTexCombo, CB_SETCURSEL, (WPARAM)graphicsTextureQuality, 0);

            // Audio controls
            hMasterEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOVSCROLL,
                20, 50 + 140, 80, 22, hDlg, (HMENU)IDC_SETTINGS_AUDIO_MASTER, hInst, nullptr);
            hMusicEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOVSCROLL,
                120, 50 + 140, 80, 22, hDlg, (HMENU)IDC_SETTINGS_AUDIO_MUSIC, hInst, nullptr);
            hEffectsEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOVSCROLL,
                220, 50 + 140, 80, 22, hDlg, (HMENU)IDC_SETTINGS_AUDIO_EFFECTS, hInst, nullptr);
            hMuteChk = CreateWindowEx(0, L"BUTTON", L"Mute",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                320, 50 + 140, 80, 20, hDlg, (HMENU)IDC_SETTINGS_AUDIO_MUTE, hInst, nullptr);

            // Fill audio edits
            wchar_t buf[32];
            swprintf_s(buf, L"%.2f", audioMasterVolume);
            SetWindowText(hMasterEdit, buf);
            swprintf_s(buf, L"%.2f", audioMusicVolume);
            SetWindowText(hMusicEdit, buf);
            swprintf_s(buf, L"%.2f", audioEffectsVolume);
            SetWindowText(hEffectsEdit, buf);
            if (audioMuted) SendMessage(hMuteChk, BM_SETCHECK, BST_CHECKED, 0);

            // Physics controls
            hGravityEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOVSCROLL,
                20, 50 + 280, 120, 22, hDlg, (HMENU)IDC_SETTINGS_GRAVITY, hInst, nullptr);
            hBounceEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOVSCROLL,
                160, 50 + 280, 80, 22, hDlg, (HMENU)IDC_SETTINGS_BOUNCE, hInst, nullptr);
            hFrictionEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOVSCROLL,
                260, 50 + 280, 80, 22, hDlg, (HMENU)IDC_SETTINGS_FRICTION, hInst, nullptr);

            // Fill physics edits with current values
            swprintf_s(buf, L"%.0f", globalGravity);
            SetWindowText(hGravityEdit, buf);
            swprintf_s(buf, L"%.2f", globalBounce);
            SetWindowText(hBounceEdit, buf);
            swprintf_s(buf, L"%.2f", globalFriction);
            SetWindowText(hFrictionEdit, buf);

            // Initially show Graphics page and hide others by moving controls offscreen if needed
            // We'll use TabCtrl_GetCurSel and ShowWindow to control visibility when tabs change

            return (INT_PTR)TRUE;
        }

    case WM_NOTIFY:
        {
            LPNMHDR lpnmhdr = (LPNMHDR)lParam;
            if (lpnmhdr->idFrom == IDC_SETTINGS_TAB && lpnmhdr->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(hTab);
                // Show/hide controls based on sel
                bool showGraphics = (sel == 0);
                bool showAudio = (sel == 1);
                bool showPhysics = (sel == 2);

                ShowWindow(hResCombo, showGraphics ? SW_SHOW : SW_HIDE);
                ShowWindow(hFullscreenChk, showGraphics ? SW_SHOW : SW_HIDE);
                ShowWindow(hVSyncChk, showGraphics ? SW_SHOW : SW_HIDE);
                ShowWindow(hTexCombo, showGraphics ? SW_SHOW : SW_HIDE);

                ShowWindow(hMasterEdit, showAudio ? SW_SHOW : SW_HIDE);
                ShowWindow(hMusicEdit, showAudio ? SW_SHOW : SW_HIDE);
                ShowWindow(hEffectsEdit, showAudio ? SW_SHOW : SW_HIDE);
                ShowWindow(hMuteChk, showAudio ? SW_SHOW : SW_HIDE);

                ShowWindow(hGravityEdit, showPhysics ? SW_SHOW : SW_HIDE);
                ShowWindow(hBounceEdit, showPhysics ? SW_SHOW : SW_HIDE);
                ShowWindow(hFrictionEdit, showPhysics ? SW_SHOW : SW_HIDE);
            }
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDAPPLY)
        {
            // Apply settings from controls
            wchar_t buffer[64];

            // Graphics
            if (hResCombo) {
                graphicsResolutionIndex = (int)SendMessage(hResCombo, CB_GETCURSEL, 0, 0);
            }
            if (hFullscreenChk) {
                graphicsFullscreenSetting = (SendMessage(hFullscreenChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            if (hVSyncChk) {
                graphicsVSync = (SendMessage(hVSyncChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            if (hTexCombo) {
                graphicsTextureQuality = (int)SendMessage(hTexCombo, CB_GETCURSEL, 0, 0);
            }

            // Audio
            if (hMasterEdit) {
                GetWindowText(hMasterEdit, buffer, 64);
                audioMasterVolume = (float)_wtof(buffer);
            }
            if (hMusicEdit) {
                GetWindowText(hMusicEdit, buffer, 64);
                audioMusicVolume = (float)_wtof(buffer);
            }
            if (hEffectsEdit) {
                GetWindowText(hEffectsEdit, buffer, 64);
                audioEffectsVolume = (float)_wtof(buffer);
            }
            if (hMuteChk) {
                audioMuted = (SendMessage(hMuteChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
            }

            // Physics
            if (hGravityEdit) {
                GetWindowText(hGravityEdit, buffer, 64);
                globalGravity = (float)_wtof(buffer);
            }
            if (hBounceEdit) {
                GetWindowText(hBounceEdit, buffer, 64);
                globalBounce = (float)_wtof(buffer);
            }
            if (hFrictionEdit) {
                GetWindowText(hFrictionEdit, buffer, 64);
                globalFriction = (float)_wtof(buffer);
            }

            // Apply to current demo if it's a physics demo
            if (currentDemo) {
                DemoPhysicsCollision* physicsDemo = dynamic_cast<DemoPhysicsCollision*>(currentDemo);
                if (physicsDemo) {
                    physicsDemo->SetPhysicsSettings(globalGravity, globalBounce, globalFriction);
                }
            }

            if (LOWORD(wParam) == IDOK) {
                EndDialog(hDlg, LOWORD(wParam));
            }
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void LoadDemo(int demoId, HWND hWnd) {
    try {
        if (currentDemo) {
            try {
                currentDemo->Shutdown();
            } catch (...) {
                // If shutdown fails, continue with cleanup
            }
            delete currentDemo;
            currentDemo = nullptr;
        }
        
        // Fixed switch statement to handle all 7 tabs correctly with proper error handling
        switch (demoId) {
            case 0: // Tab 0: Systems - Enhanced BaseX Engine Demo (FPS)
                try {
                    currentDemo = new DemoFPS();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case 1: // Tab 1: Graphics - Advanced BaseX Graphics Demo
                try {
                    currentDemo = new DemoGraphics();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case 2: // Tab 2: Physics - Enhanced collision demo with BaseX physics
                try {
                    currentDemo = new DemoPhysicsCollision();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case 3: // Tab 3: Audio - BaseX Audio and Wave Generation Demo
                try {
                    currentDemo = new DemoAudio();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case 4: // Tab 4: Golf 3D - Simple one-hole golf demo
                try {
                    currentDemo = new DemoGolf();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case 5: // Tab 5: World 3D - Basic 3D world demo with BaseX
                try {
                    currentDemo = new DemoWorld3D();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case 6: // Tab 6: Solar System - Enhanced Solar System Simulation
                try {
                    currentDemo = new DemoSolarSystem();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            default:
                // Safe fallback - always try FPS demo as it's most stable
                try {
                    currentDemo = new DemoFPS();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
        }
        
        if (currentDemo) {
            try {
                currentDemo->Init(hWnd);
            } catch (...) {
                // If init fails, delete the demo and set to nullptr
                delete currentDemo;
                currentDemo = nullptr;
                
                // Try to fall back to FPS demo if it wasn't the failing demo
                if (demoId != 0) {
                    try {
                        currentDemo = new DemoFPS();
                        if (currentDemo) {
                            currentDemo->Init(hWnd);
                        }
                    } catch (...) {
                        if (currentDemo) {
                            delete currentDemo;
                            currentDemo = nullptr;
                        }
                    }
                }
            }
        }
        
        // If we still don't have a working demo, show an error message but don't crash
        if (!currentDemo) {
            // Create a safe fallback message instead of crashing
            MessageBox(hWnd, L"Failed to load demo. Please check that all required files are present.", 
                      L"Demo Load Error", MB_OK | MB_ICONWARNING);
        }
        
    } catch (...) {
        // If entire load process fails, ensure currentDemo is null and show error
        if (currentDemo) {
            try {
                delete currentDemo;
            } catch (...) {}
            currentDemo = nullptr;
        }
        MessageBox(hWnd, L"Critical error loading demo. Application may be unstable.", 
                  L"Critical Error", MB_OK | MB_ICONERROR);
    }
}

void LoadDemoFromMenu(int menuId, HWND hWnd) {
    try {
        if (currentDemo) {
            try {
                currentDemo->Shutdown();
            } catch (...) {
                // If shutdown fails, continue with cleanup
            }
            delete currentDemo;
            currentDemo = nullptr;
        }

        // Load specific demos based on menu ID
        switch (menuId) {
            case IDM_PHYSICS_BALL:
                try {
                    currentDemo = new DemoPhysicsBall();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_PHYSICS_GRAVITY:
                try {
                    currentDemo = new DemoPhysicsGravity();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_PARTICLES:
                try {
                    currentDemo = new DemoPhysicsParticles();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_SOFTBODY:
                try {
                    currentDemo = new DemoSoftBody();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_CLOTH:
                try {
                    currentDemo = new DemoCloth();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_FLUID:
                try {
                    currentDemo = new DemoFluid();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_INTEGRATION:
                try {
                    currentDemo = new DemoIntegration();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_NETWORKING:
                try {
                    currentDemo = new DemoNetworking();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_AI:
                try {
                    currentDemo = new DemoAI();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_TOOLS:
                try {
                    currentDemo = new DemoTools();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_TEXTURES:
                try {
                    currentDemo = new DemoTextures();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            case IDM_DEMO_ARTGALLERY:
                try {
                    currentDemo = new DemoArtGallery();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
            default:
                // Fallback to FPS demo
                try {
                    currentDemo = new DemoFPS();
                } catch (...) {
                    currentDemo = nullptr;
                }
                break;
        }

        if (currentDemo) {
            try {
                currentDemo->Init(hWnd);
            } catch (...) {
                // If init fails, delete the demo and set to nullptr
                try { delete currentDemo; } catch(...) {}
                currentDemo = nullptr;

                // Try to fall back to FPS demo
                try {
                    currentDemo = new DemoFPS();
                    if (currentDemo) {
                        try {
                            currentDemo->Init(hWnd);
                        } catch (...) {
                            try { delete currentDemo; } catch(...) {}
                            currentDemo = nullptr;
                        }
                    }
                } catch (...) {
                    if (currentDemo) { try { delete currentDemo; } catch(...) {} currentDemo = nullptr; }
                }
            }
        }

        // If we still don't have a working demo, show an error message but don't crash
        if (!currentDemo) {
            MessageBox(hWnd, L"Failed to load demo. Please check that all required files are present.", 
                       L"Demo Load Error", MB_OK | MB_ICONWARNING);
        }

    } catch (...) {
        if (currentDemo) {
            try { delete currentDemo; } catch(...) {}
            currentDemo = nullptr;
        }
        MessageBox(hWnd, L"Critical error loading demo from menu. Application may be unstable.", 
                   L"Critical Error", MB_OK | MB_ICONERROR);
    }
}