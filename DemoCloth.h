#pragma once
#include "DemoBase.h"
#include <windows.h>
#include <vector>

struct ClothNode {
    double x,y; double px,py; // position and previous for Verlet
    bool fixed{false};
};
struct ClothSpring { int a,b; double rest, k; };

class DemoCloth : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
private:
    std::vector<ClothNode> nodes;
    std::vector<ClothSpring> springs;
    int cols{20}, rows{15};
    double spacing{16.0};
    double gravity{900.0};
    double damping{0.995};
    RECT bounds{10,130,790,590};
    void BuildCloth(double sx, double sy);
    void SatisfyConstraints(int iterations);
};
