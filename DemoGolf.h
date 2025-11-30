#pragma once
#include <windows.h>
#include <vector>
#include "DemoBase.h"
#include "physics.h"

using namespace BaseXEngine;

// Golf Course Hole Definition
struct GolfHole {
    BaseXVector teePos;
    BaseXVector holePos;
    double holeRadius;
    int par;
    std::vector<BaseXVector> hazards;  // Water hazards, sand traps
    std::vector<BaseXVector> trees;    // Tree positions
    double fairwayWidth;
    std::wstring description;
    COLORREF fairwayColor;
    COLORREF roughColor;
};

// Ball trail particle for enhanced visuals
struct BallTrail {
    BaseXVector position;
    float life;
    float maxLife;
    COLORREF color;
};

class DemoGolf : public DemoBase {
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
    // Window & viewport
    HWND hwnd{nullptr};

    // Camera
    BaseXVector camPos{0.0, 120.0, 300.0};
    double camYaw{0.0};
    double camPitch{-0.35};
    bool cameraFollowsBall{true};

    // Projection
    double fovDeg{70.0};
    double nearZ{1.0};
    double farZ{5000.0};

    BaseXMatrix view; // rotation only

    // Course System - Enhanced with multiple holes
    std::vector<GolfHole> course;
    int currentHole{0};
    int totalHoles{9};  // 9-hole course
    std::vector<int> holeScores;
    int totalScore{0};
    int totalPar{0};

    // Ball system with enhanced trails
    BaseXVector ballPos{0.0, 10.0, -200.0};
    BaseXVector ballVel{0.0, 0.0, 0.0};
    double ballRadius{10.0};
    bool inHole{false};
    bool ballInMotion{false};
    std::vector<BallTrail> ballTrails;
    
    // Enhanced Physics
    double gravity{-980.0}; // units per s^2 (approx px ~ mm)
    double bounce{0.3};
    double rollingFriction{0.18};
    double airDrag{0.02};
    double windSpeed{0.0};
    BaseXVector windDirection{1.0, 0.0, 0.0};

    // Enhanced Bounds and Environment
    double boundLeft{-500.0};
    double boundRight{500.0};
    double boundNear{200.0};
    double boundFar{-1500.0};

    // Enhanced Gameplay
    int strokes{0};
    bool aiming{false};
    POINT aimStart{0,0};
    POINT aimCur{0,0};
    double aimElevDeg{8.0}; // small loft
    bool spaceHeld{false};
    bool showTrajectoryPreview{true};
    bool showHelp{true};
    
    // Enhanced Visual Effects
    double timeOfDay{12.0}; // 0-24 hours
    bool dynamicLighting{true};
    double grassWavePhase{0.0};
    std::vector<BaseXVector> cloudPositions;
    
    // Enhanced Course Features
    std::vector<BaseXVector> treePositions;
    std::vector<BaseXVector> hazardPositions;
    bool ballInWater{false};
    bool ballInSand{false};

    // Show final scorecard when course complete
    bool showScorecard{false};

    // Helpers - Enhanced
    void UpdateView();
    void UpdateCamera(float deltaTime);
    void UpdateTrails(float deltaTime);
    void UpdateEnvironment(float deltaTime);
    
    bool ProjectToScreen(const BaseXVector& vCam, POINT& outPt, const RECT& rc) const;
    bool WorldToScreen(const BaseXVector& world, POINT& outPt, const RECT& rc) const;
    BaseXVector ToCameraSpace(const BaseXVector& world) const;
    bool ScreenRayToGround(int sx, int sy, BaseXVector& outPoint) const;

    void Simulate(double dt);
    void StrikeBallFromDrag();
    void ResetHole();
    void NextHole();
    void InitializeCourse();
    GolfHole CreateHole(int holeNumber);
    
    // Enhanced Rendering
    void RenderSkybox(HDC hdc, const RECT& rc);
    void RenderCourse(HDC hdc, const RECT& rc);
    void RenderBall(HDC hdc, const RECT& rc);
    void RenderHUD(HDC hdc, const RECT& rc);
    void RenderTrajectoryPreview(HDC hdc, const RECT& rc);
    void RenderEnvironment(HDC hdc, const RECT& rc);
    void RenderScorecard(HDC hdc, const RECT& rc);
    
    // Utility functions
    COLORREF GetSkyColor() const;
    COLORREF GetGrassColor(const BaseXVector& pos) const;
    double GetTerrainHeight(const BaseXVector& pos) const;
    bool IsInHazard(const BaseXVector& pos) const;
    void DrawGlow(HDC hdc, int x, int y, int radius, COLORREF color, int intensity = 3);
};
