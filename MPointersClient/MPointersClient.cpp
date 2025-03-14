#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib")

// Función para enviar un comando y recibir la respuesta
void sendCommand(const std::string& command) {
    WSADATA wsaData;
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[1024] = { 0 };

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Error al crear el socket" << std::endl;
        WSACleanup();
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Dirección no válida" << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        std::cerr << "Error al conectar con el servidor" << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    send(sock, command.c_str(), command.length(), 0);
    char reply[1024] = { 0 };
    int bytesReceived = recv(sock, reply, sizeof(reply) - 1, 0);
    std::cout << "Comando: " << command << "\nRespuesta: " << reply << std::endl;
    closesocket(sock);
    WSACleanup();
}

// Función que ejecuta la secuencia de comandos para probar las 5 funciones
void runClient() {
    // Consultar estado inicial
    sendCommand("status");

    // Crear un bloque de 5 bytes
    sendCommand("create 5");

    // Asignar el valor "5" al bloque 1
    sendCommand("set 1 5");

    // Obtener el valor del bloque 1
    sendCommand("get 1");

    // Consultar nuevamente el estado
    sendCommand("status");
}

int main() {
    runClient();
    return 0;
}
