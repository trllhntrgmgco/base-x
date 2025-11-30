#pragma once
#include "DemoBase.h"
#include "TextureLibrary.h"
#include <vector>
#include <string>

class DemoArtGallery : public DemoBase {
public:
    DemoArtGallery();
    ~DemoArtGallery();

    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;

    void HandleMouseDown(int x, int y, bool leftButton = true);
    void HandleMouseUp(int x, int y, bool leftButton = true);
    void HandleMouseMove(int x, int y);
    void HandleKeyDown(int keyCode);
    void HandleKeyUp(int keyCode);

private:
    HWND hwnd;
    std::vector<std::string> artworks;
    int selectedIndex;

    void CreateGalleryAssets();
    void UpdateGalleryList();
};
