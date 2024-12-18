﻿#include "socket.h"

int main() {
	WSADATA ws = initializeWinsock();

	SOCKET clientSocket = initializeSocket();

	sockaddr_in server = receiveBroadcast();

	connectToServer(clientSocket, server);

	receiveAndSend(clientSocket);

	closeConnection(clientSocket);

	return 0;
 }
