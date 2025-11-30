#include "DemoTextures.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <memory>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>

// Undefine possible macro conflicts from Windows headers
#undef Nearest
#undef Linear
#undef Repeat
#undef Clamp
#undef Mirror
#undef DrawText
#undef Rectangle
#undef min
#undef max

// Static COLORREF definitions (only once, outside any function)
const COLORREF DemoTextures::NEON_CYAN = RGB(51, 255, 255);
const COLORREF DemoTextures::NEON_GREEN = RGB(51, 255, 51);
const COLORREF DemoTextures::NEON_YELLOW = RGB(255, 255, 51);
const COLORREF DemoTextures::NEON_MAGENTA = RGB(255, 51, 255);
const COLORREF DemoTextures::MIDNIGHT_BLUE = RGB(10, 10, 40);

void DemoTextures::Init(HWND hWnd) {
    hwnd = hWnd;
    currentMode = MODE_GALLERY;
    animationTime = 0.0f;
    showHelp = true;
    selectedTexture = 0;
    scrollOffset = 0.0f;
    zoomFactor = 1.0f;
    mousePos = {0, 0};
    mouseDragging = false;
    dragStart = {0, 0};
    demoAtlas = nullptr;
    
    CreateDemoTextures();
    UpdateTextureGallery();
    
    // Create texture atlas demonstration
    TextureLibrary& textureLib = GetTextureLibrary();
    demoAtlas = textureLib.CreateAtlas("demo_atlas", 512, 512);
    
    if (demoAtlas) {
        // Add some textures to the atlas
        Texture* white = textureLib.GetTexture("white");
        Texture* black = textureLib.GetTexture("black");
        Texture* checkerboard = textureLib.GetTexture("checkerboard");
        Texture* noise = textureLib.GetTexture("noise");
        
        if (white) demoAtlas->AddTexture("white", *white);
        if (black) demoAtlas->AddTexture("black", *black);
        if (checkerboard) demoAtlas->AddTexture("checkerboard", *checkerboard);
        if (noise) demoAtlas->AddTexture("noise", *noise);
        
        demoAtlas->Build();
    }
}

void DemoTextures::CreateDemoTextures() {
    TextureLibrary& texLib = GetTextureLibrary();
    
    // Create various demonstration textures
    
    // Solid colors
    texLib.CreateProceduralTexture("red", 64, 64, "solid", std::vector<COLORREF>{RGB(255, 0, 0)});
    texLib.CreateProceduralTexture("green", 64, 64, "solid", std::vector<COLORREF>{RGB(0, 255, 0)});
    texLib.CreateProceduralTexture("blue", 64, 64, "solid", std::vector<COLORREF>{RGB(0, 0, 255)});
    texLib.CreateProceduralTexture("cyan", 64, 64, "solid", std::vector<COLORREF>{NEON_CYAN});
    texLib.CreateProceduralTexture("magenta", 64, 64, "solid", std::vector<COLORREF>{NEON_MAGENTA});
    texLib.CreateProceduralTexture("yellow", 64, 64, "solid", std::vector<COLORREF>{NEON_YELLOW});
    
    // Gradients
    texLib.CreateProceduralTexture("gradient_red_blue", 128, 128, "gradient", std::vector<COLORREF>{RGB(255, 0, 0), RGB(0, 0, 255)});
    texLib.CreateProceduralTexture("gradient_green_yellow", 128, 128, "gradient", std::vector<COLORREF>{RGB(0, 255, 0), RGB(255, 255, 0)});
    texLib.CreateProceduralTexture("gradient_cyan_magenta", 128, 128, "gradient", std::vector<COLORREF>{NEON_CYAN, NEON_MAGENTA});
    
    // Checkerboards
    texLib.CreateProceduralTexture("checkerboard_small", 64, 64, "checkerboard", std::vector<COLORREF>{RGB(255, 255, 255), RGB(0, 0, 0)});
    texLib.CreateProceduralTexture("checkerboard_large", 64, 64, "checkerboard", std::vector<COLORREF>{NEON_CYAN, MIDNIGHT_BLUE});
    texLib.CreateProceduralTexture("checkerboard_colored", 64, 64, "checkerboard", std::vector<COLORREF>{RGB(255, 100, 100), RGB(100, 255, 100)});
    
    // Noise textures
    std::vector<COLORREF> noiseParams1 = {RGB(123, 0, 0)};
    texLib.CreateProceduralTexture("noise_fine", 128, 128, "noise", noiseParams1);
    std::vector<COLORREF> noiseParams2 = {RGB(200, 0, 0)};
    texLib.CreateProceduralTexture("noise_coarse", 64, 64, "noise", noiseParams2);
    
    // Create some procedural textures using the ProceduralTextures namespace
    CreateBuiltInTextures();
}

void DemoTextures::CreateBuiltInTextures() {
    TextureLibrary& textureLib = GetTextureLibrary();
    
    // Create simple procedural textures using built-in methods
    auto cloudsTexture = std::make_unique<Texture>("clouds", 128, 128, TextureFormat::RGB24);
    cloudsTexture->CreateNoise(128, 128, 42);
    textureLib.SetTexture("clouds", std::move(cloudsTexture));
    
    auto brickTexture = std::make_unique<Texture>("brick", 128, 128, TextureFormat::RGB24);
    brickTexture->CreateCheckerboard(128, 128, 16, RGB(180, 100, 80), RGB(200, 200, 200));
    textureLib.SetTexture("brick", std::move(brickTexture));
    
    auto perlinTexture = std::make_unique<Texture>("perlin", 128, 128, TextureFormat::RGB24);
    perlinTexture->CreateNoise(128, 128, 123);
    textureLib.SetTexture("perlin", std::move(perlinTexture));
    
    // Create built-in textures from the library
    textureLib.CreateBuiltInTextures();
}

void DemoTextures::CreateAnimatedTextures() {
    TextureLibrary& textureLib = GetTextureLibrary();
    
    // Create time-based animated textures
    float time = animationTime * 0.001f; // Convert to seconds
    
    // Animated wave texture
    auto waveTexture = std::make_unique<Texture>("wave", 128, 128, TextureFormat::RGB24);
    waveTexture->CreateSolid(128, 128, RGB(0, 100, 200));
    
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            float fx = (float)x / 128.0f;
            float fy = (float)y / 128.0f;
            
            float wave = sin(fx * 8.0f + time * 2.0f) * sin(fy * 6.0f + time * 1.5f);
            wave = (wave + 1.0f) * 0.5f;
            
            BYTE intensity = (BYTE)(wave * 100 + 155);
            waveTexture->SetPixel(x, y, RGB(0, intensity, 255));
        }
    }
    
    textureLib.SetTexture("animated_wave", std::move(waveTexture));
    
    // Animated plasma texture
    auto plasmaTexture = std::make_unique<Texture>("plasma", 128, 128, TextureFormat::RGB24);
    
    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 128; x++) {
            float fx = (float)x / 128.0f;
            float fy = (float)y / 128.0f;
            
            float plasma = sin(fx * 10.0f + time) + sin(fy * 8.0f + time * 0.7f) + 
                          sin((fx + fy) * 6.0f + time * 1.3f) + sin(sqrt(fx*fx + fy*fy) * 12.0f + time * 2.0f);
            plasma = (plasma + 4.0f) / 8.0f;
            
            BYTE r = (BYTE)(sin(plasma * 3.14159f + time) * 127 + 128);
            BYTE g = (BYTE)(sin(plasma * 3.14159f + time + 2.0f) * 127 + 128);
            BYTE b = (BYTE)(sin(plasma * 3.14159f + time + 4.0f) * 127 + 128);
            
            plasmaTexture->SetPixel(x, y, RGB(r, g, b));
        }
    }
    
    textureLib.SetTexture("animated_plasma", std::move(plasmaTexture));
}

void DemoTextures::UpdateAnimatedTextures(float deltaTime) {
    animationTime += deltaTime;
    CreateAnimatedTextures();
}

void DemoTextures::Update(float deltaTime) {
    // Update animations
    UpdateAnimatedTextures(deltaTime);

    // Handle scrolling
    if (GetAsyncKeyState(VK_UP) & 0x8000) {
        scrollOffset -= 200.0f * deltaTime / 1000.0f;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
        scrollOffset += 200.0f * deltaTime / 1000.0f;
    }

    // Handle zooming
    if (GetAsyncKeyState(VK_ADD) & 0x8000 || GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) {
        zoomFactor *= 1.0f + deltaTime / 1000.0f;
        zoomFactor = std::min(zoomFactor, 4.0f);
    }
    if (GetAsyncKeyState(VK_SUBTRACT) & 0x8000 || GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) {
        zoomFactor *= 1.0f - deltaTime / 1000.0f;
        zoomFactor = std::max(zoomFactor, 0.25f);
    }
    // Clamp scroll offset
    scrollOffset = std::max(0.0f, scrollOffset);
}

void DemoTextures::Render(HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    if (rc.top < 130) rc.top = 130; // Account for UI controls
    
    // Clear background with midnight blue
    HBRUSH bgBrush = CreateSolidBrush(MIDNIGHT_BLUE);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    // Render current mode
    switch (currentMode) {
        case MODE_GALLERY:
            RenderGallery(hdc, rc);
            break;
        case MODE_FILTERING:
            RenderFiltering(hdc, rc);
            break;
        case MODE_WRAPPING:
            RenderWrapping(hdc, rc);
            break;
        case MODE_BLENDING:
            RenderBlending(hdc, rc);
            break;
        case MODE_PROCEDURAL:
            RenderProcedural(hdc, rc);
            break;
        case MODE_ATLAS:
            RenderAtlas(hdc, rc);
            break;
        case MODE_ANIMATED:
            RenderAnimated(hdc, rc);
            break;
    }
    
    RenderHUD(hdc, rc);
    
    if (showHelp) {
        RenderHelp(hdc, rc);
    }
}

void DemoTextures::RenderGallery(HDC hdc, const RECT& rc) {
    int cols = 4;
    size_t rows = 0;
    if (!textureGallery.empty()) rows = (textureGallery.size() + cols - 1) / cols; // round up
    int cellWidth = (rc.right - rc.left - 50) / cols;
    int cellHeight = 120;
    
    SetBkMode(hdc, TRANSPARENT);
    
    for (size_t i = 0; i < textureGallery.size(); ++i) {
        int col = static_cast<int>(i % cols);
        int row = static_cast<int>(i / cols);
        
        int x = rc.left + 25 + col * cellWidth;
        int y = rc.top + 25 + row * cellHeight - (int)scrollOffset;
        
        if (y + cellHeight < rc.top || y > rc.bottom) continue; // Skip if not visible
        
        RECT cellRect = {x, y, x + cellWidth - 10, y + cellHeight - 10};
        RECT texRect = {x + 5, y + 5, x + cellWidth - 15, y + 90};
        
        // Highlight selected texture
        if (selectedTexture >= 0 && static_cast<size_t>(selectedTexture) == i) {
            HPEN highlightPen = CreatePen(PS_SOLID, 3, NEON_YELLOW);
            HPEN oldPen = (HPEN)SelectObject(hdc, highlightPen);
            Rectangle(hdc, cellRect.left - 2, cellRect.top - 2, cellRect.right + 2, cellRect.bottom + 2);
            SelectObject(hdc, oldPen);
            DeleteObject(highlightPen);
        }
        
        // Draw texture
        if (textureGallery[i].texture) {
            TextureRenderer::DrawTexture(hdc, *textureGallery[i].texture, 
                                       texRect.left, texRect.top, 
                                       texRect.right - texRect.left, 
                                       texRect.bottom - texRect.top);
        }
        
        // Draw border
        HPEN borderPen = CreatePen(PS_SOLID, 1, NEON_CYAN);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        Rectangle(hdc, texRect.left, texRect.top, texRect.right, texRect.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
        
        // Draw texture name
        SetTextColor(hdc, NEON_GREEN);
        RECT nameRect = {x + 5, y + 95, x + cellWidth - 15, y + cellHeight - 5};
        std::wstring wName(textureGallery[i].name.begin(), textureGallery[i].name.end());
        DrawTextW(hdc, wName.c_str(), -1, &nameRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void DemoTextures::RenderFiltering(HDC hdc, const RECT& rc) {
    TextureLibrary& textureLib = GetTextureLibrary();
    Texture* texture = textureLib.GetTexture("checkerboard");
    if (!texture) return;
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_YELLOW);
    
    // Draw title
    RECT titleRect = {rc.left, rc.top + 10, rc.right, rc.top + 40};
    DrawTextW(hdc, L"Texture Filtering Modes", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    int startY = rc.top + 50;
    int rectSize = 150;
    int spacing = 200;
    
    // Nearest filtering
    TextureFilter oldFilter = texture->GetFilter();
    texture->SetFilter(TextureFilter::TF_Nearest);
    
    RECT nearestRect = {rc.left + 50, startY, rc.left + 50 + rectSize, startY + rectSize};
    TextureRenderer::DrawTexture(hdc, *texture, nearestRect.left, nearestRect.top, 
                                rectSize, rectSize);
    
    SetTextColor(hdc, NEON_GREEN);
    RECT nearestLabel = {nearestRect.left, nearestRect.bottom + 5, 
                        nearestRect.right, nearestRect.bottom + 25};
    DrawTextW(hdc, L"Nearest", -1, &nearestLabel, DT_CENTER);
    
    // Linear filtering
    texture->SetFilter(TextureFilter::TF_Linear);
    
    RECT linearRect = {rc.left + 50 + spacing, startY, 
                      rc.left + 50 + spacing + rectSize, startY + rectSize};
    TextureRenderer::DrawTexture(hdc, *texture, linearRect.left, linearRect.top, 
                                rectSize, rectSize);
    
    RECT linearLabel = {linearRect.left, linearRect.bottom + 5, 
                       linearRect.right, linearRect.bottom + 25};
    DrawTextW(hdc, L"Linear", -1, &linearLabel, DT_CENTER);
    
    // Restore original filter
    texture->SetFilter(oldFilter);
}

void DemoTextures::RenderWrapping(HDC hdc, const RECT& rc) {
    TextureLibrary& textureLib = GetTextureLibrary();
    Texture* texture = textureLib.GetTexture("gradient_red_blue");
    if (!texture) return;
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_YELLOW);
    
    // Draw title
    RECT titleRect = {rc.left, rc.top + 10, rc.right, rc.top + 40};
    DrawTextW(hdc, L"Texture Wrapping Modes", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    int startY = rc.top + 50;
    int rectSize = 120;
    int spacing = 150;
    
    TextureWrap modes[3] = {TextureWrap::TW_Repeat, TextureWrap::TW_Clamp, TextureWrap::TW_Mirror};
    const wchar_t* modeNames[] = {L"Repeat", L"Clamp", L"Mirror"};
    
    TextureWrap oldWrap = texture->GetWrap();
    
    for (int i = 0; i < 3; i++) {
        texture->SetWrap(modes[i]);
        
        RECT wrapRect = {rc.left + 50 + i * spacing, startY, 
                        rc.left + 50 + i * spacing + rectSize, startY + rectSize};
        
        // Draw tiled texture to show wrapping
        RECT tileArea = {wrapRect.left - 20, wrapRect.top - 20, 
                        wrapRect.right + 20, wrapRect.bottom + 20};
        TextureRenderer::DrawTiledTexture(hdc, *texture, tileArea, 40, 40);
        
        // Draw border
        HPEN borderPen = CreatePen(PS_SOLID, 2, NEON_CYAN);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        Rectangle(hdc, wrapRect.left, wrapRect.top, wrapRect.right, wrapRect.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
        
        SetTextColor(hdc, NEON_GREEN);
        RECT label = {wrapRect.left, wrapRect.bottom + 5, 
                     wrapRect.right, wrapRect.bottom + 25};
        DrawTextW(hdc, modeNames[i], -1, &label, DT_CENTER);
    }
    
    texture->SetWrap(oldWrap);
}

void DemoTextures::RenderBlending(HDC hdc, const RECT& rc) {
    TextureLibrary& textureLib = GetTextureLibrary();
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_YELLOW);
    
    // Draw title
    RECT titleRect = {rc.left, rc.top + 10, rc.right, rc.top + 40};
    DrawTextW(hdc, L"Texture Blending Modes", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    Texture* texture1 = textureLib.GetTexture("checkerboard");
    Texture* texture2 = textureLib.GetTexture("gradient_red_blue");
    
    if (!texture1 || !texture2) return;
    
    int startY = rc.top + 50;
    int rectSize = 100;
    int spacing = 130;
    
    const wchar_t* blendNames[] = {L"Normal", L"Additive", L"Multiply"};
    
    for (int i = 0; i < 3; i++) {
        int x = rc.left + 50 + i * spacing;
        int y = startY;
        
        // Draw first texture
        TextureRenderer::DrawTexture(hdc, *texture1, x, y, rectSize, rectSize);
        
        // Draw second texture with some transparency effect (simplified)
        TextureRenderer::DrawTexture(hdc, *texture2, x + 20, y + 20, rectSize/2, rectSize/2);
        
        // Draw border
        HPEN borderPen = CreatePen(PS_SOLID, 1, NEON_CYAN);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        Rectangle(hdc, x, y, x + rectSize, y + rectSize);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
        
        SetTextColor(hdc, NEON_GREEN);
        RECT label = {x, y + rectSize + 5, x + rectSize, y + rectSize + 25};
        DrawTextW(hdc, blendNames[i], -1, &label, DT_CENTER);
    }
}

void DemoTextures::RenderProcedural(HDC hdc, const RECT& rc) {
    TextureLibrary& textureLib = GetTextureLibrary();
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_YELLOW);
    
    // Draw title
    RECT titleRect = {rc.left, rc.top + 10, rc.right, rc.top + 40};
    DrawTextW(hdc, L"Procedural Textures", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    const char* proceduralTextures[] = {"clouds", "brick", "perlin", "noise_fine"};
    const wchar_t* textureNames[] = {L"Clouds", L"Brick", L"Perlin Noise", L"Fine Noise"};
    
    int startY = rc.top + 50;
    int rectSize = 100;
    int spacing = 130;
    
    for (int i = 0; i < 4; i++) {
        Texture* texture = textureLib.GetTexture(proceduralTextures[i]);
        if (!texture) continue;
        
        int x = rc.left + 50 + (i % 4) * spacing;
        int y = startY + (i / 4) * 140;
        
        RECT texRect = {x, y, x + rectSize, y + rectSize};
        TextureRenderer::DrawTexture(hdc, *texture, x, y, rectSize, rectSize);
        
        // Draw border
        HPEN borderPen = CreatePen(PS_SOLID, 1, NEON_CYAN);
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        Rectangle(hdc, texRect.left, texRect.top, texRect.right, texRect.bottom);
        SelectObject(hdc, oldPen);
        DeleteObject(borderPen);
        
        SetTextColor(hdc, NEON_GREEN);
        RECT label = {x, y + rectSize + 5, x + rectSize, y + rectSize + 25};
        DrawTextW(hdc, textureNames[i], -1, &label, DT_CENTER);
    }
}

void DemoTextures::RenderAtlas(HDC hdc, const RECT& rc) {
    if (!demoAtlas) return;
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_YELLOW);
    
    // Draw title
    RECT titleRect = {rc.left, rc.top + 10, rc.right, rc.top + 40};
    DrawTextW(hdc, L"Texture Atlas Demonstration", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    // Draw the atlas texture
    const Texture& atlasTexture = demoAtlas->GetAtlasTexture();
    int atlasSize = 256;
    int startX = rc.left + 50;
    int startY = rc.top + 50;
    
    TextureRenderer::DrawTexture(hdc, atlasTexture, startX, startY, atlasSize, atlasSize);
    
    // Draw border
    HPEN borderPen = CreatePen(PS_SOLID, 2, NEON_CYAN);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    Rectangle(hdc, startX, startY, startX + atlasSize, startY + atlasSize);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
    
    // Draw atlas info
    SetTextColor(hdc, NEON_GREEN);
    RECT infoRect = {startX + atlasSize + 20, startY, rc.right - 20, startY + 200};
    
    std::wstringstream info;
    info << L"Atlas Size: " << atlasTexture.GetWidth() << L"x" << atlasTexture.GetHeight() << L"\n\n";
    info << L"Contains:\n";
    info << L"• White texture\n";
    info << L"• Black texture\n";
    info << L"• Checkerboard\n";
    info << L"• Noise texture\n\n";
    info << L"UV Coordinates are automatically\n";
    info << L"calculated for each sub-texture.";
    
    DrawTextW(hdc, info.str().c_str(), -1, &infoRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void DemoTextures::RenderAnimated(HDC hdc, const RECT& rc) {
    TextureLibrary& textureLib = GetTextureLibrary();
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_YELLOW);
    
    // Draw title
    RECT titleRect = {rc.left, rc.top + 10, rc.right, rc.top + 40};
    DrawTextW(hdc, L"Animated Textures", -1, &titleRect, DT_CENTER | DT_VCENTER);
    
    int startY = rc.top + 50;
    int rectSize = 150;
    int spacing = 200;
    
    // Animated wave
    Texture* waveTexture = textureLib.GetTexture("animated_wave");
    if (waveTexture) {
        RECT waveRect = {rc.left + 50, startY, rc.left + 50 + rectSize, startY + rectSize};
        TextureRenderer::DrawTexture(hdc, *waveTexture, waveRect.left, waveRect.top, 
                                    rectSize, rectSize);
        
        SetTextColor(hdc, NEON_GREEN);
        RECT waveLabel = {waveRect.left, waveRect.bottom + 5, 
                         waveRect.right, waveRect.bottom + 25};
        DrawTextW(hdc, L"Animated Wave", -1, &waveLabel, DT_CENTER);
    }
    
    // Animated plasma
    Texture* plasmaTexture = textureLib.GetTexture("animated_plasma");
    if (plasmaTexture) {
        RECT plasmaRect = {rc.left + 50 + spacing, startY, 
                          rc.left + 50 + spacing + rectSize, startY + rectSize};
        TextureRenderer::DrawTexture(hdc, *plasmaTexture, plasmaRect.left, plasmaRect.top, 
                                    rectSize, rectSize);
        
        SetTextColor(hdc, NEON_GREEN);
        RECT plasmaLabel = {plasmaRect.left, plasmaRect.bottom + 5, 
                           plasmaRect.right, plasmaRect.bottom + 25};
        DrawTextW(hdc, L"Animated Plasma", -1, &plasmaLabel, DT_CENTER);
    }
}

void DemoTextures::RenderHUD(HDC hdc, const RECT& rc) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_CYAN);
    
    // Mode indicator
    const wchar_t* modeNames[] = {
        L"Gallery", L"Filtering", L"Wrapping", L"Blending", 
        L"Procedural", L"Atlas", L"Animated"
    };
    
    std::wstring modeText = L"Mode: ";
    modeText += modeNames[currentMode];
    modeText += L" (Press TAB to cycle)";
    
    TextOut(hdc, rc.left + 10, rc.top + 5, modeText.c_str(), (int)modeText.length());
    
    // Memory usage
    TextureLibrary& textureLib = GetTextureLibrary();
    size_t memUsage = textureLib.GetMemoryUsage();
    std::wstringstream memText;
    memText << L"Memory: " << (memUsage / 1024) << L" KB | Textures: " << textureLib.GetTextureCount();
    
    SetTextColor(hdc, NEON_GREEN);
    std::wstring memStr = memText.str();
    TextOut(hdc, rc.right - 250, rc.top + 5, memStr.c_str(), (int)memStr.length());
}

void DemoTextures::RenderHelp(HDC hdc, const RECT& rc) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, NEON_YELLOW);
    
    RECT helpRect = {rc.left + 10, rc.bottom - 120, rc.right - 10, rc.bottom - 10};
    
    const wchar_t* helpText = 
        L"Controls:\n"
        L"TAB - Cycle demo modes | H - Toggle help | ?? - Scroll | +/- - Zoom\n"
        L"Mouse - Select textures | Space - Reset view\n"
        L"1-7 - Jump to specific mode";
    
    DrawTextW(hdc, helpText, -1, &helpRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void DemoTextures::UpdateTextureGallery() {
    textureGallery.clear();
    
    TextureLibrary& textureLib = GetTextureLibrary();
    std::vector<std::string> textureNames = textureLib.GetLoadedTextureNames();
    
    for (const std::string& name : textureNames) {
        TextureDisplay display;
        display.name = name;
        display.texture = textureLib.GetTexture(name);
        display.highlighted = false;
        textureGallery.push_back(display);
    }
}

void DemoTextures::CycleModes() {
    currentMode = static_cast<DemoMode>((static_cast<int>(currentMode) + 1) % MODE_COUNT);
    scrollOffset = 0.0f;
    zoomFactor = 1.0f;
}

void DemoTextures::HandleMouseDown(int x, int y, bool leftButton) {
    if (!leftButton) return;
    
    mouseDragging = true;
    dragStart = {x, y};
    mousePos = {x, y};
    
    // Check for texture selection in gallery mode
    if (currentMode == MODE_GALLERY) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (rc.top < 130) rc.top = 130;
        
        int cols = 4;
        int cellWidth = (rc.right - rc.left - 50) / cols;
        int cellHeight = 120;
        
        for (size_t i = 0; i < textureGallery.size(); ++i) {
            int col = static_cast<int>(i % cols);
            int row = static_cast<int>(i / cols);
            
            int tx = rc.left + 25 + col * cellWidth;
            int ty = rc.top + 25 + row * cellHeight - (int)scrollOffset;
            
            RECT cellRect = {tx, ty, tx + cellWidth - 10, ty + cellHeight - 10};
            
            if (x >= cellRect.left && x <= cellRect.right && 
                y >= cellRect.top && y <= cellRect.bottom) {
                selectedTexture = static_cast<int>(i);
                break;
            }
        }
    }
}

void DemoTextures::HandleMouseUp(int x, int y, bool leftButton) {
    mouseDragging = false;
}

void DemoTextures::HandleMouseMove(int x, int y) {
    mousePos = {x, y};
    if (mouseDragging && currentMode == MODE_GALLERY) {
        int deltaY = y - dragStart.y;
        scrollOffset -= deltaY * 0.5f;
        scrollOffset = std::max(0.0f, scrollOffset);
        dragStart = {x, y};
    }
}

void DemoTextures::HandleKeyDown(int keyCode) {
    switch (keyCode) {
        case VK_TAB:
            CycleModes();
            break;
        case 'H':
            showHelp = !showHelp;
            break;
        case VK_SPACE:
            scrollOffset = 0.0f;
            zoomFactor = 1.0f;
            break;
        case '1': currentMode = MODE_GALLERY; break;
        case '2': currentMode = MODE_FILTERING; break;
        case '3': currentMode = MODE_WRAPPING; break;
        case '4': currentMode = MODE_BLENDING; break;
        case '5': currentMode = MODE_PROCEDURAL; break;
        case '6': currentMode = MODE_ATLAS; break;
        case '7': currentMode = MODE_ANIMATED; break;
    }
}

void DemoTextures::HandleKeyUp(int keyCode) {
    // Not used in this demo
}

void DemoTextures::Shutdown() {
    TextureLibrary& textureLib = GetTextureLibrary();
    if (demoAtlas) {
        textureLib.DestroyAtlas("demo_atlas");
        demoAtlas = nullptr;
    }
    
    textureGallery.clear();
}
