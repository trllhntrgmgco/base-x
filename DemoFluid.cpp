#define NOMINMAX
#include "DemoFluid.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Kernel helpers (2D)
static inline double poly6(double r2, double h){
    if (r2 >= 0 && r2 <= h*h) {
        double x = (h*h - r2);
        return (4.0/(M_PI*pow(h,8))) * x*x*x; // normalized 2D poly6
    }
    return 0.0;
}
static inline double spikyGradCoeff(double r, double h){
    if (r>0 && r <= h) return -30.0/(M_PI*pow(h,5)) * (h - r)*(h - r);
    return 0.0;
}
static inline double viscLaplacian(double r, double h){
    if (r>=0 && r<=h) return 40.0/(M_PI*pow(h,5)) * (h - r);
    return 0.0;
}

void DemoFluid::RefitBounds(){
    if (!hwnd) return;
    RECT rc; GetClientRect(hwnd, &rc);
    bounds = {10,130, rc.right-10, rc.bottom-10};
    gridW = (std::max)(1, (int)((bounds.right - bounds.left)/h));
    gridH = (std::max)(1, (int)((bounds.bottom - bounds.top)/h));
    gridHead.assign(gridW*gridH, -1);
}

void DemoFluid::BuildGrid(){
    std::fill(gridHead.begin(), gridHead.end(), -1);
    gridNext.assign(particles.size(), -1);
    for (int i=0;i<(int)particles.size();++i){
        int cx = (int)((particles[i].x - bounds.left)/h);
        int cy = (int)((particles[i].y - bounds.top)/h);
        if (cx < 0) cx = 0; else if (cx > gridW-1) cx = gridW-1;
        if (cy < 0) cy = 0; else if (cy > gridH-1) cy = gridH-1;
        int ci = CellIndex(cx,cy);
        gridNext[i] = gridHead[ci];
        gridHead[ci] = i;
    }
}

void DemoFluid::Init(HWND hWnd) {
    hwnd = hWnd;
    RefitBounds();

    particles.clear();
    particles.resize(particleCount);

    // Spawn a blob
    int cols = (int)std::sqrt((double)particleCount);
    int rows = (particleCount + cols - 1)/cols;
    double startX = bounds.left + (bounds.right-bounds.left)*0.25;
    double startY = bounds.top + (bounds.bottom-bounds.top)*0.25;
    double spacing = h*0.9;
    int idx=0;
    for(int r=0;r<rows && idx<particleCount;++r){
        for(int c=0;c<cols && idx<particleCount;++c){
            particles[idx].x = startX + c*spacing;
            particles[idx].y = startY + r*spacing;
            particles[idx].vx = particles[idx].vy = 0;
            ++idx;
        }
    }
}

void DemoFluid::Update(float deltaTime) {
    if (particles.empty()) return;
    float dt = (std::min)(0.033f, deltaTime/1000.0f);

    // keep bounds updated in case of resize
    RefitBounds();

    // 1) Build grid
    BuildGrid();

    // 2) Compute density and pressure
    for (auto &p: particles){ p.density = 0; }
    for (int cy=0; cy<gridH; ++cy){
        for (int cx=0; cx<gridW; ++cx){
            // visit this cell and neighbors
            for (int ny=-1; ny<=1; ++ny){
                for (int nx=-1; nx<=1; ++nx){
                    int gx=cx+nx, gy=cy+ny; if (gx<0||gy<0||gx>=gridW||gy>=gridH) continue;
                    int head = gridHead[CellIndex(gx,gy)];
                    for (int i = gridHead[CellIndex(cx,cy)]; i!=-1; i = gridNext[i]){
                        for (int j = head; j!=-1; j = gridNext[j]){
                            double dx = particles[j].x - particles[i].x;
                            double dy = particles[j].y - particles[i].y;
                            double r2 = dx*dx+dy*dy;
                            particles[i].density += mass * poly6(r2,h);
                        }
                    }
                }
            }
        }
    }
    for (auto &p: particles){ p.pressure = gasK * (p.density - restDensity); }

    // 3) Compute forces
    for (auto &p: particles){ p.ax = 0; p.ay = gravity; }

    for (int cy=0; cy<gridH; ++cy){
        for (int cx=0; cx<gridW; ++cx){
            for (int ny=-1; ny<=1; ++ny){
                for (int nx=-1; nx<=1; ++nx){
                    int gx=cx+nx, gy=cy+ny; if (gx<0||gy<0||gx>=gridW||gy>=gridH) continue;
                    int head = gridHead[CellIndex(gx,gy)];
                    for (int i = gridHead[CellIndex(cx,cy)]; i!=-1; i = gridNext[i]){
                        for (int j = head; j!=-1; j = gridNext[j]){
                            if (i==j) continue;
                            double dx = particles[j].x - particles[i].x;
                            double dy = particles[j].y - particles[i].y;
                            double r2 = dx*dx+dy*dy; if (r2>h*h || r2<=1e-12) continue;
                            double r = std::sqrt(r2);

                            // Pressure force
                            double pij = (particles[i].pressure + particles[j].pressure)/(2.0*particles[j].density);
                            double grad = spikyGradCoeff(r,h);
                            double fx = -mass * pij * grad * (dx/r);
                            double fy = -mass * pij * grad * (dy/r);
                            particles[i].ax += fx;
                            particles[i].ay += fy;

                            // Viscosity
                            double vijx = particles[j].vx - particles[i].vx;
                            double vijy = particles[j].vy - particles[i].vy;
                            double lap = viscLaplacian(r,h);
                            particles[i].ax += viscosity * vijx * lap / particles[j].density * mass;
                            particles[i].ay += viscosity * vijy * lap / particles[j].density * mass;
                        }
                    }
                }
            }
        }
    }

    // 4) Integrate and handle boundaries
    for (auto &p: particles){
        p.vx += p.ax * dt; p.vy += p.ay * dt;
        p.vx *= (1.0 - dt*0.5*(1.0-damping));
        p.vy *= (1.0 - dt*0.5*(1.0-damping));
        p.x += p.vx * dt; p.y += p.vy * dt;

        // bounds
        double rad = h*0.4;
        if (p.x < bounds.left+rad){ p.x = bounds.left+rad; p.vx *= -damping; }
        if (p.x > bounds.right-rad){ p.x = bounds.right-rad; p.vx *= -damping; }
        if (p.y < bounds.top+rad){ p.y = bounds.top+rad; p.vy *= -damping; }
        if (p.y > bounds.bottom-rad){ p.y = bounds.bottom-rad; p.vy *= -damping; }
    }
}

void DemoFluid::Render(HDC hdc) {
    // draw boundary
    HPEN pen = CreatePen(PS_SOLID,1,RGB(80,120,160));
    HPEN oldP = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, bounds.left, bounds.top, bounds.right, bounds.bottom);

    // draw particles
    HBRUSH pb = CreateSolidBrush(RGB(100,160,255));
    HBRUSH old = (HBRUSH)SelectObject(hdc, pb);
    int r = (int)(h*0.4);
    for (const auto &p : particles){
        Ellipse(hdc, (int)(p.x-r),(int)(p.y-r),(int)(p.x+r),(int)(p.y+r));
    }

    SelectObject(hdc, old);
    DeleteObject(pb);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(pen);
}

void DemoFluid::Shutdown() {
    particles.clear();
    gridHead.clear();
    gridNext.clear();
}
