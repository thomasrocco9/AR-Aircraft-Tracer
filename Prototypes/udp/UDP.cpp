#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>

// Link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#define BROADCAST_PORT 21221

int main() {
    WSADATA wsaData;
    struct sockaddr_in outgoing;
    // Initialize winsock, version 2.2
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        exit(0);
    }

    // Create UDP socket
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to open socket" << std::endl;
        WSACleanup();
        return 1;
    }

    // Set socket options for broadcasting
    BOOL bOptVal = TRUE;
    setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(int));
    outgoing.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    outgoing.sin_port = htons(BROADCAST_PORT);
    outgoing.sin_family = AF_INET;
    //bind(udpSocket, (struct sockaddr*)&outgoing, sizeof(struct sockaddr_in));

    const char* message = "UDP Test Completed";

    // Send simple message
    sendto(udpSocket, message, strlen(message) + 1, 0, (struct sockaddr*)&outgoing, sizeof(struct sockaddr_in));

    closesocket(udpSocket);
    WSACleanup();
    return 0;
}
