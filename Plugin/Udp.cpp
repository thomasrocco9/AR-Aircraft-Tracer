#include "Udp.h"

#pragma comment(lib, "Ws2_32.lib")

void udp::Init()
{
	WSADATA WsaData;
	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0) {
		return;
	}

	UdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (UdpSocket == INVALID_SOCKET) {
		WSACleanup();
		return;
	}

	BOOL OptVal = TRUE;

	setsockopt(UdpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&OptVal, sizeof(int));
	DestAddr.sin_family = AF_INET;
	DestAddr.sin_port = htons(BroadcastPort);
	DestAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
}

void udp::Send(const char* Buf)
{
	sendto(UdpSocket, Buf, strlen(Buf) + 1, 0, (struct sockaddr*)&DestAddr, sizeof(DestAddr));
}

void udp::Cleanup()
{
	closesocket(UdpSocket);
	WSACleanup();
}