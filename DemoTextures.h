#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DemoBase.h"
#include "TextureLibrary.h"
#include <vector>
#include <string>

class DemoTextures : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
    
    // Input handling
    void HandleMouseDown(int x, int y, bool leftButton = true);
    void HandleMouseUp(int x, int y, bool leftButton = true);
    void HandleMouseMove(int x, int y);
    void HandleKeyDown(int keyCode);
    void HandleKeyUp(int keyCode);

private:
    HWND hwnd;
    
    // Demo state
    enum DemoMode {
        MODE_GALLERY,       // Show texture gallery
        MODE_FILTERING,     // Demonstrate filtering modes
        MODE_WRAPPING,      // Demonstrate wrapping modes
        MODE_BLENDING,      // Demonstrate blending modes
        MODE_PROCEDURAL,    // Show procedural textures
        MODE_ATLAS,         // Demonstrate texture atlas
        MODE_ANIMATED,      // Animated textures
        MODE_COUNT
    };
    
    DemoMode currentMode;
    float animationTime;
    bool showHelp;
    
    // Texture gallery
    struct TextureDisplay {
        std::string name;
        Texture* texture;
        RECT displayRect;
        bool highlighted;
    };
    
    std::vector<TextureDisplay> textureGallery;
    int selectedTexture;
    
    // Animation parameters
    float scrollOffset;
    float zoomFactor;
    POINT mousePos;
    bool mouseDragging;
    POINT dragStart;
    
    // Atlas for demo
    TextureAtlas* demoAtlas;
    
    // Demo textures
    void CreateDemoTextures();
    void CreateBuiltInTextures();
    void CreateAnimatedTextures();
    void UpdateAnimatedTextures(float deltaTime);
    
    // Rendering functions
    void RenderGallery(HDC hdc, const RECT& rc);
    void RenderFiltering(HDC hdc, const RECT& rc);
    void RenderWrapping(HDC hdc, const RECT& rc);
    void RenderBlending(HDC hdc, const RECT& rc);
    void RenderProcedural(HDC hdc, const RECT& rc);
    void RenderAtlas(HDC hdc, const RECT& rc);
    void RenderAnimated(HDC hdc, const RECT& rc);
    void RenderHUD(HDC hdc, const RECT& rc);
    void RenderHelp(HDC hdc, const RECT& rc);
    
    // Utility functions
    void UpdateTextureGallery();
    void CycleModes();
    RECT GetTextureRect(int index, const RECT& bounds) const;
    void DrawTextureWithInfo(HDC hdc, const Texture& texture, const RECT& rect, 
                           const std::string& info = "");
    void DrawGrid(HDC hdc, const RECT& rect, int gridSize = 20);
    
    // Color constants for enhanced visibility
    static const COLORREF NEON_CYAN;
    static const COLORREF NEON_GREEN;
    static const COLORREF NEON_YELLOW;
    static const COLORREF NEON_MAGENTA;
    static const COLORREF MIDNIGHT_BLUE;
};