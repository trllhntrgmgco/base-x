#include "DemoSoftBody.h"
#include <windows.h>
#include <cmath>

void DemoSoftBody::Init(HWND hWnd) {
    nodes.clear();
    springs.clear();
    RECT rc;
    GetClientRect(hWnd, &rc);
    bounds = {10, 130, rc.right - 10, rc.bottom - 10};
    double startX = bounds.left + 100;
    double startY = bounds.top + 20;
    BuildGrid(startX, startY);
    initialized = true;
}

void DemoSoftBody::BuildGrid(double startX, double startY) {
    nodes.clear();
    springs.clear();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            SoftNode n;
            n.x = startX + c * spacing;
            n.y = startY + r * spacing;
            n.vx = n.vy = 0.0;
            n.invMass = (r == 0 && (c == 0 || c == cols-1)) ? 0.0 : 1.0; // fix two top corners
            nodes.push_back(n);
        }
    }
    auto idx = [&](int rr, int cc){ return rr * cols + cc; };
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (c + 1 < cols) {
                SoftSpring s{idx(r,c), idx(r,c+1), spacing, 0.9};
                springs.push_back(s);
            }
            if (r + 1 < rows) {
                SoftSpring s{idx(r,c), idx(r+1,c), spacing, 0.9};
                springs.push_back(s);
            }
            // shear springs
            if (r + 1 < rows && c + 1 < cols) {
                double diag = spacing * sqrt(2.0);
                springs.push_back(SoftSpring{idx(r,c), idx(r+1,c+1), diag, 0.7});
            }
            if (r + 1 < rows && c - 1 >= 0) {
                double diag = spacing * sqrt(2.0);
                springs.push_back(SoftSpring{idx(r,c), idx(r+1,c-1), diag, 0.7});
            }
        }
    }
}

void DemoSoftBody::SolveSprings(double dt) {
    // simple linear spring correction (position-based-ish)
    for (const SoftSpring &s : springs) {
        SoftNode &a = nodes[s.a];
        SoftNode &b = nodes[s.b];
        double dx = b.x - a.x;
        double dy = b.y - a.y;
        double len = sqrt(dx*dx + dy*dy);
        if (len <= 1e-6) continue;
        double diff = (len - s.restLength) / len;
        double invMassSum = a.invMass + b.invMass;
        if (invMassSum <= 0) continue;
        double k = s.stiffness * 0.5;
        double offsetX = dx * diff * k;
        double offsetY = dy * diff * k;
        if (a.invMass > 0) {
            a.x += offsetX * (a.invMass / invMassSum);
            a.y += offsetY * (a.invMass / invMassSum);
        }
        if (b.invMass > 0) {
            b.x -= offsetX * (b.invMass / invMassSum);
            b.y -= offsetY * (b.invMass / invMassSum);
        }
    }
}

void DemoSoftBody::Integrate(double dt) {
    for (SoftNode &n : nodes) {
        if (n.invMass <= 0) continue;
        n.vy += gravity * dt;
        n.vx *= damping;
        n.vy *= damping;
        n.x += n.vx * dt;
        n.y += n.vy * dt;
        // simple bounds
        if (n.x < bounds.left) { n.x = bounds.left; n.vx *= -0.3; }
        if (n.x > bounds.right) { n.x = bounds.right; n.vx *= -0.3; }
        if (n.y < bounds.top) { n.y = bounds.top; n.vy *= -0.3; }
        if (n.y > bounds.bottom) { n.y = bounds.bottom; n.vy *= -0.3; }
    }
}

void DemoSoftBody::Update(float deltaTime) {
    if (!initialized) return;
    double dt = deltaTime / 1000.0;
    // integrate then solve constraints iteratively
    Integrate(dt);
    for (int i = 0; i < 4; ++i) SolveSprings(dt);
}

void DemoSoftBody::Render(HDC hdc) {
    if (!initialized) return;
    // draw springs
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(180,180,220));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    for (const SoftSpring &s : springs) {
        const SoftNode &a = nodes[s.a];
        const SoftNode &b = nodes[s.b];
        MoveToEx(hdc, (int)a.x, (int)a.y, nullptr);
        LineTo(hdc, (int)b.x, (int)b.y);
    }
    SelectObject(hdc, old);
    DeleteObject(pen);

    // draw nodes
    HBRUSH brush = CreateSolidBrush(RGB(220,180,180));
    HBRUSH ob = (HBRUSH)SelectObject(hdc, brush);
    for (const SoftNode &n : nodes) {
        Ellipse(hdc, (int)n.x-3, (int)n.y-3, (int)n.x+3, (int)n.y+3);
    }
    SelectObject(hdc, ob);
    DeleteObject(brush);
}

void DemoSoftBody::Shutdown() {
    nodes.clear();
    springs.clear();
    initialized = false;
}
