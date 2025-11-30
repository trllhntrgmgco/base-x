#include "DemoNetworking.h"
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

void DemoNetworking::Init(HWND hWnd) {
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (res != 0) {
        initialized = false;
        return;
    }

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        initialized = false;
        return;
    }

    // make non-blocking
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    // bind to a local port (auto)
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(50010); // local port
    if (bind(sock, (sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        closesocket(sock);
        WSACleanup();
        initialized = false;
        return;
    }

    // set peer to localhost:50011 (for testing two instances on same machine)
    peerAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &peerAddr.sin_addr);
    peerAddr.sin_port = htons(50011);

    initialized = true;
}

void DemoNetworking::Update(float deltaTime) {
    if (!initialized) return;

    // send a simple position packet (here we just send a counter based on time)
    float px = (float)(sin(GetTickCount64() / 1000.0) * 100.0);
    float py = (float)(cos(GetTickCount64() / 1000.0) * 50.0);
    float buf[2] = { px, py };
    sendto(sock, (const char*)buf, sizeof(buf), 0, (sockaddr*)&peerAddr, sizeof(peerAddr));

    // receive any packet
    sockaddr_in from{};
    int fromlen = sizeof(from);
    char rbuf[256];
    int r = recvfrom(sock, rbuf, (int)sizeof(rbuf), 0, (sockaddr*)&from, &fromlen);
    if (r >= (int)sizeof(float)*2) {
        float* f = (float*)rbuf;
        syncedX = f[0];
        syncedY = f[1];
    }
}

void DemoNetworking::Render(HDC hdc) {
    RECT rc; GetClientRect(GetActiveWindow(), &rc);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220,220,220));
    wchar_t buf[256];
    if (!initialized) {
        swprintf_s(buf, L"Networking: Not initialized");
    } else {
        swprintf_s(buf, L"Networking: connected  SyncPos: (%.1f, %.1f)", syncedX, syncedY);
    }
    TextOut(hdc, rc.left + 10, rc.top + 10, buf, lstrlenW(buf));
}

void DemoNetworking::Shutdown() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    if (initialized) WSACleanup();
    initialized = false;
}
