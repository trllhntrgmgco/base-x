#include "DemoArtGallery.h"
#include <windows.h>
#include <sstream>

DemoArtGallery::DemoArtGallery() {
    hwnd = nullptr;
    selectedIndex = -1;
}

DemoArtGallery::~DemoArtGallery() {
    Shutdown();
}

void DemoArtGallery::Init(HWND hWnd) {
    hwnd = hWnd;
    CreateGalleryAssets();
    UpdateGalleryList();
}

void DemoArtGallery::CreateGalleryAssets() {
    TextureLibrary& lib = GetTextureLibrary();
    // Use existing procedural textures and built-ins
    lib.CreateProceduralTexture("gallery_painting_1", 256, 256, "gradient", {RGB(200,50,50), RGB(250,200,180)});
    lib.CreateProceduralTexture("gallery_painting_2", 256, 256, "checkerboard", {RGB(200,200,255), RGB(255,200,200), RGB(8,8,8)});
    lib.CreateProceduralTexture("gallery_painting_3", 256, 256, "noise", {RGB(123,0,0)});
}

void DemoArtGallery::UpdateGalleryList() {
    TextureLibrary& lib = GetTextureLibrary();
    artworks = lib.GetLoadedTextureNames();
    // Pick the first gallery item if available
    for (size_t i = 0; i < artworks.size(); ++i) {
        if (artworks[i].find("gallery_painting_") != std::string::npos) {
            selectedIndex = (int)i;
            break;
        }
    }
}

void DemoArtGallery::Update(float deltaTime) {
    // No heavy logic for this demo, maybe rotate selection with arrow keys
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
        if (!artworks.empty()) selectedIndex = (selectedIndex + 1) % (int)artworks.size();
    }
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
        if (!artworks.empty()) selectedIndex = (selectedIndex - 1 + (int)artworks.size()) % (int)artworks.size();
    }
}

void DemoArtGallery::Render(HDC hdc) {
    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.top < 130) rc.top = 130;

    // Background
    HBRUSH bg = CreateSolidBrush(RGB(30,30,30));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    // Draw selected artwork large
    if (selectedIndex >= 0 && selectedIndex < (int)artworks.size()) {
        TextureLibrary& lib = GetTextureLibrary();
        Texture* t = lib.GetTexture(artworks[selectedIndex]);
        if (t) {
            int w = min(512, rc.right - rc.left - 200);
            int h = min(512, rc.bottom - rc.top - 200);
            int x = rc.left + (rc.right - rc.left - w) / 2;
            int y = rc.top + 60;
            TextureRenderer::DrawTexture(hdc, *t, x, y, w, h);

            // Frame
            HPEN pen = CreatePen(PS_SOLID, 6, RGB(240,240,240));
            HPEN old = (HPEN)SelectObject(hdc, pen);
            Rectangle(hdc, x-10, y-10, x+w+10, y+h+10);
            SelectObject(hdc, old);
            DeleteObject(pen);

            // Title
            SetTextColor(hdc, RGB(240,240,240));
            std::wstring title = std::wstring(artworks[selectedIndex].begin(), artworks[selectedIndex].end());
            TextOut(hdc, x, y+h+20, title.c_str(), (int)title.length());
        }
    }

    // Draw thumbnails
    int thumbY = rc.bottom - 120;
    int thumbX = rc.left + 20;
    for (size_t i = 0; i < artworks.size(); ++i) {
        TextureLibrary& lib = GetTextureLibrary();
        Texture* t = lib.GetTexture(artworks[i]);
        if (!t) continue;
        TextureRenderer::DrawTexture(hdc, *t, thumbX, thumbY, 80, 80);
        if ((int)i == selectedIndex) {
            HPEN pen = CreatePen(PS_SOLID, 3, RGB(255,215,0));
            HPEN old = (HPEN)SelectObject(hdc, pen);
            Rectangle(hdc, thumbX-4, thumbY-4, thumbX+84, thumbY+84);
            SelectObject(hdc, old);
            DeleteObject(pen);
        }
        thumbX += 90;
    }
}

void DemoArtGallery::Shutdown() {
    // Nothing special
}

void DemoArtGallery::HandleMouseDown(int x, int y, bool leftButton) {
    RECT rc; GetClientRect(hwnd, &rc);
    int thumbY = rc.bottom - 120;
    int thumbX = rc.left + 20;
    for (size_t i = 0; i < artworks.size(); ++i) {
        RECT r = {thumbX, thumbY, thumbX+80, thumbY+80};
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            selectedIndex = (int)i;
            break;
        }
        thumbX += 90;
    }
}

void DemoArtGallery::HandleMouseUp(int x, int y, bool leftButton) {}
void DemoArtGallery::HandleMouseMove(int x, int y) {}
void DemoArtGallery::HandleKeyDown(int keyCode) {}
void DemoArtGallery::HandleKeyUp(int keyCode) {}
