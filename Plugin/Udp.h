#pragma once

#include <winsock2.h>
#include <WS2tcpip.h>

class udp
{
public:
	void Init();
	void Send(const char* Buf);
	void Cleanup();

private:
	const int BroadcastPort = 21221;
	SOCKET UdpSocket;
	struct sockaddr_in DestAddr;
};