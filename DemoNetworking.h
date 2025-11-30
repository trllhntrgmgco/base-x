#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include "DemoBase.h"

class DemoNetworking : public DemoBase {
public:
    void Init(HWND hWnd) override;
    void Update(float deltaTime) override;
    void Render(HDC hdc) override;
    void Shutdown() override;
private:
    // UDP socket used for simple position sync
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in peerAddr{};
    bool initialized = false;

    // simple synced state
    float syncedX = 0.0f;
    float syncedY = 0.0f;
};
