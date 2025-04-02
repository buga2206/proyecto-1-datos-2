#include "MPointerConnector.h"
#include <iostream>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

// Inicializar variables estáticas
SOCKET MPointerConnector::sock = INVALID_SOCKET;
sockaddr_in MPointerConnector::serv_addr;
std::mutex MPointerConnector::mtx;
bool MPointerConnector::isInitialized = false;

void MPointerConnector::Init(const std::string& host, int port) {
    if (!isInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Error al inicializar Winsock.");
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Error al crear el socket.");
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Dirección inválida.");
        }

        if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Error al conectar al servidor.");
        }

        isInitialized = true;
    }
}

std::string MPointerConnector::sendCommand(const std::string& cmd) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!isInitialized) return "ERROR: No inicializado.";

    if (send(sock, cmd.c_str(), cmd.size(), 0) == SOCKET_ERROR) {
        return "ERROR: Fallo al enviar comando.";
    }

    char buffer[1024];
    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) return "ERROR: Sin respuesta.";

    return std::string(buffer, bytesReceived);
}

void MPointerConnector::Shutdown() {
    if (isInitialized) {
        closesocket(sock);
        WSACleanup();
        isInitialized = false;
    }
}