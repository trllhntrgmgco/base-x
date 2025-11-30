#include "DemoCloth.h"
#include <algorithm>
#include <cmath>

#ifndef NOMINMAX
#define NOMINMAX
#endif

void DemoCloth::BuildCloth(double sx, double sy){
    nodes.clear(); springs.clear();
    nodes.resize(cols*rows);
    for(int r=0;r<rows;++r){
        for(int c=0;c<cols;++c){
            int i=r*cols+c; double x=sx+c*spacing, y=sy+r*spacing;
            nodes[i].x=x; nodes[i].y=y; nodes[i].px=x; nodes[i].py=y;
            nodes[i].fixed = (r==0 && (c%4==0)); // pin every 4th on top
            if (c>0){ springs.push_back({i-1,i,spacing,1.0}); }
            if (r>0){ springs.push_back({i-cols,i,spacing,1.0}); }
        }
    }
}

void DemoCloth::Init(HWND hWnd){
    RECT rc; GetClientRect(hWnd,&rc); bounds = {10,130, rc.right-10, rc.bottom-10};
    BuildCloth(bounds.left+60, bounds.top+40);
}

void DemoCloth::SatisfyConstraints(int iterations){
    for(int it=0; it<iterations; ++it){
        // spring constraints
        for(const auto&s: springs){
            auto &A = nodes[s.a]; auto &B = nodes[s.b];
            double dx=B.x-A.x, dy=B.y-A.y; double d2=dx*dx+dy*dy; if (d2<=1e-9) continue;
            double d=std::sqrt(d2); double diff=(d - s.rest)/d; double corr=0.5*s.k*diff;
            if (!A.fixed){ A.x += dx*corr; A.y += dy*corr; }
            if (!B.fixed){ B.x -= dx*corr; B.y -= dy*corr; }
        }
        // bounds
        for(auto &n:nodes){
            double rad=4.0;
            if (n.x < bounds.left+rad) n.x=bounds.left+rad;
            if (n.x > bounds.right-rad) n.x=bounds.right-rad;
            if (n.y < bounds.top+rad) n.y=bounds.top+rad;
            if (n.y > bounds.bottom-rad) n.y=bounds.bottom-rad;
        }
    }
}

void DemoCloth::Update(float deltaTime){
    double dt = (std::min)(0.033, (double)deltaTime/1000.0);
    // integrate (Verlet)
    for(auto &n:nodes){
        if (n.fixed) continue;
        double vx = (n.x - n.px)*damping; double vy = (n.y - n.py)*damping;
        n.px = n.x; n.py = n.y;
        vy += gravity*dt;
        n.x += vx; n.y += vy;
    }
    SatisfyConstraints(5);
}

void DemoCloth::Render(HDC hdc){
    HPEN pen = CreatePen(PS_SOLID,1,RGB(180,180,220)); HPEN old=(HPEN)SelectObject(hdc,pen);
    for(const auto&s: springs){
        MoveToEx(hdc,(int)nodes[s.a].x,(int)nodes[s.a].y,nullptr);
        LineTo(hdc,(int)nodes[s.b].x,(int)nodes[s.b].y);
    }
    SelectObject(hdc,old); DeleteObject(pen);
}

void DemoCloth::Shutdown(){ nodes.clear(); springs.clear(); }
