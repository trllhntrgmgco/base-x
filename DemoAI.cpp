#include "DemoAI.h"
#include <cmath>
#include <stdlib.h>

void DemoAI::Init(HWND hWnd) {
    agents.clear();
    agents.reserve(agentCount);
    RECT rc; GetClientRect(hWnd, &rc);
    float cx = (rc.left + rc.right) * 0.5f;
    float cy = (rc.top + rc.bottom) * 0.5f;

    for (int i = 0; i < agentCount; ++i) {
        DemoAI::Agent a;
        a.x = cx + (float)(rand() % 200 - 100);
        a.y = cy + (float)(rand() % 200 - 100);
        a.vx = (float)(rand() % 100 - 50) / 10.0f;
        a.vy = (float)(rand() % 100 - 50) / 10.0f;
        agents.push_back(a);
    }
    targetX = cx;
    targetY = cy;
    running = true;
}

void DemoAI::Update(float deltaTime) {
    if (!running) return;
    RECT rc; GetClientRect(GetActiveWindow(), &rc);
    float cx = (rc.left + rc.right) * 0.5f;
    float cy = (rc.top + rc.bottom) * 0.5f;

    // moving target
    double t = GetTickCount64() / 1000.0;
    targetX = cx + (float)(sin(t) * 150.0);
    targetY = cy + (float)(cos(t * 0.7) * 100.0);

    for (size_t i = 0; i < agents.size(); ++i) {
        Agent& a = agents[i];
        // seek force
        float dx = targetX - a.x;
        float dy = targetY - a.y;
        float dist = sqrtf(dx*dx + dy*dy);
        float sx = 0.0f, sy = 0.0f;
        if (dist > 1e-3f) {
            sx = dx / dist;
            sy = dy / dist;
        }

        // neighbor stats for alignment & cohesion
        float alignX = 0.0f, alignY = 0.0f;
        float cohX = 0.0f, cohY = 0.0f;
        int neighCount = 0;

        // avoid neighbors
        float ax = 0.0f, ay = 0.0f;
        for (size_t j = 0; j < agents.size(); ++j) {
            if (i == j) continue;
            float ox = agents[j].x - a.x;
            float oy = agents[j].y - a.y;
            float od = sqrtf(ox*ox + oy*oy);
            if (od < neighborRadius && od > 1e-3f) {
                // separation
                ax -= (ox / od) * (neighborRadius - od) / neighborRadius;
                ay -= (oy / od) * (neighborRadius - od) / neighborRadius;
                // alignment
                alignX += agents[j].vx;
                alignY += agents[j].vy;
                // cohesion (toward neighbors' center)
                cohX += agents[j].x;
                cohY += agents[j].y;
                neighCount++;
            }
        }

        if (neighCount > 0) {
            alignX /= neighCount; alignY /= neighCount;
            float an = sqrtf(alignX*alignX + alignY*alignY);
            if (an > 1e-3f) { alignX = alignX / an; alignY = alignY / an; }

            cohX /= neighCount; cohY /= neighCount;
            cohX = cohX - a.x; cohY = cohY - a.y;
            float cn = sqrtf(cohX*cohX + cohY*cohY);
            if (cn > 1e-3f) { cohX = cohX / cn; cohY = cohY / cn; }
        }

        // combine
        float fx = sx * seekWeight + ax * avoidWeight + alignX * alignWeight + cohX * cohesionWeight;
        float fy = sy * seekWeight + ay * avoidWeight + alignY * alignWeight + cohY * cohesionWeight;

        // clamp acceleration
        float accelMag = sqrtf(fx*fx + fy*fy);
        if (accelMag > maxAccel) { fx = fx / accelMag * maxAccel; fy = fy / accelMag * maxAccel; }

        // integrate
        a.vx += fx * deltaTime;
        a.vy += fy * deltaTime;
        // clamp speed
        float speed = sqrtf(a.vx*a.vx + a.vy*a.vy);
        if (speed > maxSpeed) {
            a.vx = a.vx / speed * maxSpeed;
            a.vy = a.vy / speed * maxSpeed;
        }

        a.x += a.vx * deltaTime;
        a.y += a.vy * deltaTime;

        // wrap inside window
        if (a.x < rc.left) a.x = rc.right;
        if (a.x > rc.right) a.x = rc.left;
        if (a.y < rc.top) a.y = rc.bottom;
        if (a.y > rc.bottom) a.y = rc.top;
    }
}

void DemoAI::Render(HDC hdc) {
    RECT rc; GetClientRect(GetActiveWindow(), &rc);
    // draw target
    HBRUSH tb = CreateSolidBrush(RGB(255,80,80));
    HBRUSH old = (HBRUSH)SelectObject(hdc, tb);
    Ellipse(hdc, (int)targetX-6, (int)targetY-6, (int)targetX+6, (int)targetY+6);
    SelectObject(hdc, old);
    DeleteObject(tb);

    // draw agents
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(100,200,255));
    HPEN oldp = (HPEN)SelectObject(hdc, pen);
    for (auto &a : agents) {
        Ellipse(hdc, (int)a.x-4, (int)a.y-4, (int)a.x+4, (int)a.y+4);
    }
    SelectObject(hdc, oldp);
    DeleteObject(pen);

    // info
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220,220,220));
    wchar_t buf[128];
    swprintf_s(buf, L"AI: Agents=%d  Target=(%.1f, %.1f)", (int)agents.size(), targetX, targetY);
    TextOut(hdc, rc.left + 10, rc.top + 10, buf, lstrlenW(buf));
}

void DemoAI::Shutdown() {
    agents.clear();
    running = false;
}
