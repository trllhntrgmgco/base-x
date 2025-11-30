#include "DemoIntegration.h"
#include "DemoBase.h"
#include "physics.h"
#include <vector>
#include <string>
#include <cmath>

using namespace BaseXEngine;

enum IntegratorType {
    INTEGRATOR_EULER = 0,
    INTEGRATOR_VERLET = 1,
    INTEGRATOR_RK4 = 2
};

struct Particle {
    BaseXVector pos;
    BaseXVector vel;
    BaseXVector prevPos; // for Verlet
    double mass;
};

class DemoIntegrationImpl {
public:
    HWND hwnd = nullptr;
    Particle p;
    BaseXVector anchor;
    double gravity = -980.0; // units per s^2 (consistent with other demos)
    double springK = 50.0;
    double damping = 0.02;
    IntegratorType integrator = INTEGRATOR_EULER;
    std::vector<POINT> trail;
    int maxTrail = 300;

    void Reset() {
        p.pos = BaseXVector(0.0, 0.0, 0.0);
        p.vel = BaseXVector(0.0, 0.0, 0.0);
        p.prevPos = p.pos;
        p.mass = 1.0;
        anchor = BaseXVector(0.0, 200.0, 0.0);
        trail.clear();
    }

    BaseXVector acceleration(const BaseXVector& pos, const BaseXVector& vel) {
        // gravity + spring to anchor + damping
        BaseXVector a(0.0, gravity, 0.0);
        BaseXVector diff = pos - anchor;
        // spring force toward anchor: F = -k * x
        BaseXVector spring = diff * (-springK / p.mass);
        // damping proportional to velocity
        BaseXVector damp = vel * (-damping);
        return a + spring + damp;
    }

    void StepEuler(double dt) {
        BaseXVector a = acceleration(p.pos, p.vel);
        p.vel = p.vel + a * dt;
        p.pos = p.pos + p.vel * dt;
    }

    void StepVerlet(double dt) {
        // p_new = 2p - p_prev + a*dt*dt
        BaseXVector a = acceleration(p.pos, p.vel);
        BaseXVector newPos = p.pos * 2.0 - p.prevPos + a * (dt * dt);
        // estimate velocity
        p.vel = (newPos - p.prevPos) / (2.0 * dt);
        p.prevPos = p.pos;
        p.pos = newPos;
    }

    // RK4 for second-order system by turning into first-order system
    void StepRK4(double dt) {
        // state: [pos, vel]
        // derivative: dpos = vel, dvel = a(pos, vel)
        struct State { BaseXVector x; BaseXVector v; };
        auto deriv = [&](const State& s)->State {
            State out;
            out.x = s.v;
            out.v = acceleration(s.x, s.v);
            return out;
        };

        State s0{p.pos, p.vel};
        State k1 = deriv(s0);
        State s1{p.pos + k1.x * (dt * 0.5), p.vel + k1.v * (dt * 0.5)};
        State k2 = deriv(s1);
        State s2{p.pos + k2.x * (dt * 0.5), p.vel + k2.v * (dt * 0.5)};
        State k3 = deriv(s2);
        State s3{p.pos + k3.x * dt, p.vel + k3.v * dt};
        State k4 = deriv(s3);

        BaseXVector dp = (k1.x + k2.x * 2.0 + k3.x * 2.0 + k4.x) * (1.0/6.0);
        BaseXVector dv = (k1.v + k2.v * 2.0 + k3.v * 2.0 + k4.v) * (1.0/6.0);

        p.pos = p.pos + dp * dt;
        p.vel = p.vel + dv * dt;
    }

    void Update(double dtSeconds) {
        // integrate depending on integrator
        if (integrator == INTEGRATOR_EULER) {
            StepEuler(dtSeconds);
        } else if (integrator == INTEGRATOR_VERLET) {
            // Verlet requires prevPos to be initialized properly
            // if prevPos equals pos (first frame) approximate prevPos = pos - vel*dt
            if (p.prevPos.x() == p.pos.x() && p.prevPos.y() == p.pos.y() && p.prevPos.z() == p.pos.z()) {
                p.prevPos = p.pos - p.vel * dtSeconds;
            }
            StepVerlet(dtSeconds);
        } else if (integrator == INTEGRATOR_RK4) {
            StepRK4(dtSeconds);
        }

        // record trail
        if (hwnd) {
            RECT rc; GetClientRect(hwnd, &rc);
            POINT pt;
            pt.x = (int)(rc.left + (rc.right - rc.left) * 0.5 + p.pos.x());
            pt.y = (int)(rc.top + (rc.bottom - rc.top) * 0.5 - p.pos.y());
            trail.push_back(pt);
            if ((int)trail.size() > maxTrail) trail.erase(trail.begin());
        }
    }

    void Render(HDC hdc) {
        if (!hwnd) return;
        RECT rc; GetClientRect(hwnd, &rc);
        // clear background
        HBRUSH bg = CreateSolidBrush(RGB(18,20,30));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        // draw anchor
        POINT aPt;
        aPt.x = (int)(rc.left + (rc.right - rc.left) * 0.5 + anchor.x());
        aPt.y = (int)(rc.top + (rc.bottom - rc.top) * 0.5 - anchor.y());
        HBRUSH anchorBrush = CreateSolidBrush(RGB(255,180,0));
        HBRUSH oldB = (HBRUSH)SelectObject(hdc, anchorBrush);
        Ellipse(hdc, aPt.x-6, aPt.y-6, aPt.x+6, aPt.y+6);
        SelectObject(hdc, oldB);
        DeleteObject(anchorBrush);

        // draw trail
        HPEN trailPen = CreatePen(PS_SOLID, 1, RGB(100,200,255));
        HPEN oldP = (HPEN)SelectObject(hdc, trailPen);
        for (size_t i = 1; i < trail.size(); ++i) {
            MoveToEx(hdc, trail[i-1].x, trail[i-1].y, nullptr);
            LineTo(hdc, trail[i].x, trail[i].y);
        }
        SelectObject(hdc, oldP);
        DeleteObject(trailPen);

        // draw particle
        POINT pPt;
        pPt.x = (int)(rc.left + (rc.right - rc.left) * 0.5 + p.pos.x());
        pPt.y = (int)(rc.top + (rc.bottom - rc.top) * 0.5 - p.pos.y());
        HBRUSH pBrush = CreateSolidBrush(RGB(200,220,255));
        HBRUSH oldPB = (HBRUSH)SelectObject(hdc, pBrush);
        Ellipse(hdc, pPt.x-8, pPt.y-8, pPt.x+8, pPt.y+8);
        SelectObject(hdc, oldPB);
        DeleteObject(pBrush);

        // draw spring line
        HPEN springPen = CreatePen(PS_DOT, 2, RGB(255,200,120));
        HPEN oldSP = (HPEN)SelectObject(hdc, springPen);
        MoveToEx(hdc, aPt.x, aPt.y, nullptr);
        LineTo(hdc, pPt.x, pPt.y);
        SelectObject(hdc, oldSP);
        DeleteObject(springPen);

        // UI text
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(220,220,220));
        wchar_t buf[256];
        const wchar_t* integratorName = (integrator == INTEGRATOR_EULER) ? L"Euler" : (integrator == INTEGRATOR_VERLET) ? L"Verlet" : L"RK4";
        swprintf_s(buf, L"Integrator: %s  (Press 1=Euler 2=Verlet 3=RK4)  R=Reset  Space=Impulse", integratorName);
        TextOut(hdc, rc.left + 10, rc.top + 10, buf, lstrlenW(buf));

        // particle state
        swprintf_s(buf, L"Pos: (%.2f, %.2f)  Vel: (%.2f, %.2f)", p.pos.x(), p.pos.y(), p.vel.x(), p.vel.y());
        TextOut(hdc, rc.left + 10, rc.top + 30, buf, lstrlenW(buf));
    }
};

static DemoIntegrationImpl g_demo;

void DemoIntegration::Init(HWND hWnd) {
    g_demo.hwnd = hWnd;
    g_demo.Reset();
}

void DemoIntegration::Update(float deltaTime) {
    // avoid Windows' max macro collision by using an explicit conditional
    double dt = (deltaTime < 0.0f ? 0.0 : deltaTime) / 1000.0; // seconds
    g_demo.Update(dt);
}

void DemoIntegration::Render(HDC hdc) {
    g_demo.Render(hdc);
}

void DemoIntegration::Shutdown() {
    g_demo.hwnd = nullptr;
}

// Input helpers exposed via dynamic_cast in main WndProc will call these if needed
void HandleIntegrationKeyDown(int key) {
    switch (key) {
    case '1': g_demo.integrator = INTEGRATOR_EULER; break;
    case '2': g_demo.integrator = INTEGRATOR_VERLET; break;
    case '3': g_demo.integrator = INTEGRATOR_RK4; break;
    case 'R': case 'r': g_demo.Reset(); break;
    case VK_SPACE:
        // apply upward impulse
        g_demo.p.vel = g_demo.p.vel + BaseXVector(0.0, 300.0, 0.0);
        break;
    }
}

void HandleIntegrationMouseDown(int x, int y) {
    if (!g_demo.hwnd) return;
    RECT rc; GetClientRect(g_demo.hwnd, &rc);
    // convert screen to world relative to center
    double wx = (double)(x - (rc.left + (rc.right - rc.left)/2));
    double wy = (double)((rc.top + (rc.bottom - rc.top)/2) - y);
    // apply impulse toward mouse
    BaseXVector dir(wx - g_demo.p.pos.x(), wy - g_demo.p.pos.y(), 0.0);
    double mag = dir.magnitude();
    if (mag > 1e-6) dir = dir / mag;
    g_demo.p.vel = g_demo.p.vel + dir * 400.0;
}
