#include "DemoTools.h"
#include <string>
#include <vector>

void DemoTools::Init(HWND hWnd) {
    hwnd = hWnd;
    showGrid = false;
    showDebug = false;
    screenshotCounter = 0;
}

void DemoTools::Update(float deltaTime) {
    // nothing heavy
}

static void DrawGrid(HDC hdc, RECT rc) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(40,40,60));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    for (int x = rc.left; x < rc.right; x += 20) {
        MoveToEx(hdc, x, rc.top, nullptr);
        LineTo(hdc, x, rc.bottom);
    }
    for (int y = rc.top; y < rc.bottom; y += 20) {
        MoveToEx(hdc, rc.left, y, nullptr);
        LineTo(hdc, rc.right, y);
    }
    SelectObject(hdc, old);
    DeleteObject(pen);
}

void DemoTools::Render(HDC hdc) {
    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.top < 130) rc.top = 130;

    // Background
    HBRUSH bg = CreateSolidBrush(RGB(12,14,22));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    if (showGrid) DrawGrid(hdc, rc);

    // Debug overlay
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220,220,220));
    wchar_t buf[256];
    swprintf_s(buf, L"Tools Demo - Grid: %s  Debug: %s  Screenshots: %d",
        showGrid ? L"On" : L"Off", showDebug ? L"On" : L"Off", screenshotCounter);
    TextOut(hdc, rc.left + 10, rc.top + 10, buf, lstrlenW(buf));

    if (showDebug) {
        // draw some debug boxes
        HPEN p = CreatePen(PS_SOLID, 1, RGB(100,255,100));
        HPEN oldp = (HPEN)SelectObject(hdc, p);
        Rectangle(hdc, rc.right - 220, rc.top + 20, rc.right - 20, rc.top + 120);
        SelectObject(hdc, oldp);
        DeleteObject(p);
    }
}

void DemoTools::Shutdown() {
    hwnd = nullptr;
}

void DemoTools::HandleKeyDown(int keyCode) {
    unsigned long now = GetTickCount();
    if (now - lastKeyToggleTime < 150) return; // debounce
    lastKeyToggleTime = now;

    switch (keyCode) {
        case 'G': showGrid = !showGrid; break;
        case 'D': showDebug = !showDebug; break;
        case 'S': TakeScreenshot(); break;
    }
}

void DemoTools::TakeScreenshot() {
    if (!hwnd) return;
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    HDC hdcSrc = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcSrc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcSrc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, hBmp);
    BitBlt(hdcMem, 0, 0, w, h, hdcSrc, 0, 0, SRCCOPY);

    // Save simple 24-bit BMP
    BITMAP bmp; GetObject(hBmp, sizeof(bmp), &bmp);
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    int lineSize = ((bi.biWidth * 3 + 3) & ~3);
    std::vector<char> pixels(lineSize * bi.biHeight);
    BITMAPINFO biInfo = {};
    biInfo.bmiHeader = bi;
    GetDIBits(hdcMem, hBmp, 0, bi.biHeight, pixels.data(), &biInfo, DIB_RGB_COLORS);

    wchar_t filename[260];
    swprintf_s(filename, L"screenshot_%03d.bmp", screenshotCounter++);

    FILE* f = nullptr;
    _wfopen_s(&f, filename, L"wb");
    if (f) {
        BITMAPFILEHEADER fh = {};
        fh.bfType = 0x4D42;
        fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        fh.bfSize = fh.bfOffBits + (DWORD)pixels.size();
        fwrite(&fh, sizeof(fh), 1, f);
        fwrite(&bi, sizeof(bi), 1, f);
        fwrite(pixels.data(), 1, pixels.size(), f);
        fclose(f);
    }

    SelectObject(hdcMem, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcSrc);
}
