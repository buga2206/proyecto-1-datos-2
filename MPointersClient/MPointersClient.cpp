#include <iostream>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

int main() {
    WSADATA wsaData;
    int sock = 0;
    struct sockaddr_in serv_addr;
    const char* hello = "Hello from client";
    char buffer[1024] = { 0 };

    // Inicializar Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    // Crear el socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Socket creation error" << std::endl;
        WSACleanup();
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    // Convertir direcciones IPv4 e IPv6 de texto a binario
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Conectar al servidor
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        std::cerr << "Connection Failed" << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Enviar mensaje al servidor
    send(sock, hello, strlen(hello), 0);
    std::cout << "Hello message sent\n";

    // Leer respuesta del servidor
    recv(sock, buffer, 1024, 0);
    std::cout << "Message from server: " << buffer << std::endl;

    // Cerrar el socket
    closesocket(sock);
    WSACleanup();

    return 0;
}
