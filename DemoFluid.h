#pragma once
#include "DemoBase.h"
#include <windows.h>
#include <vector>

class DemoFluid : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
private:
    struct Particle {
        double x{0}, y{0};
        double vx{0}, vy{0};
        double ax{0}, ay{0};
        double density{0}, pressure{0};
    };

    // Simulation
    HWND hwnd{nullptr};
    RECT bounds{10,130,790,590}; // will be updated from window
    std::vector<Particle> particles;

    // Spatial grid (uniform)
    double h{16.0}; // smoothing length and grid cell size
    int gridW{0}, gridH{0};
    std::vector<int> gridHead;        // head index per cell
    std::vector<int> gridNext;        // linked list next per particle

    // Parameters
    double restDensity{1000.0};
    double gasK{2000.0};
    double mass{65.0};
    double viscosity{250.0};
    double damping{0.5};
    double gravity{800.0};
    int particleCount{800};

    // Helpers
    void RefitBounds();
    void BuildGrid();
    inline int CellIndex(int cx, int cy) const { return cy * gridW + cx; }
};
