#pragma once
#include "DemoBase.h"
#include <vector>
#include <windows.h>

struct SoftNode {
    double x, y;
    double vx, vy;
    double invMass; // 0 for fixed
};

struct SoftSpring {
    int a, b; // indices into nodes
    double restLength;
    double stiffness;
};

class DemoSoftBody : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;

private:
    std::vector<SoftNode> nodes;
    std::vector<SoftSpring> springs;
    int cols = 8;
    int rows = 6;
    double spacing = 24.0;
    double gravity = 500.0;
    double damping = 0.98;
    RECT bounds{10,130,790,590};
    bool initialized = false;

    void BuildGrid(double startX, double startY);
    void SolveSprings(double dt);
    void Integrate(double dt);
};
