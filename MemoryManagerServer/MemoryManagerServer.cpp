#include <iostream>
#include <sstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "MemoryManager.h"

#pragma comment(lib, "Ws2_32.lib")

// Función robusta para parsear los parámetros de la línea de comandos.
bool parseArguments(int argc, char** argv, int& port, size_t& memSizeBytes, std::string& dumpFolder) {
    std::string portStr, memsizeStr;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            portStr = argv[++i];
        }
        else if (arg == "--memsize" && i + 1 < argc) {
            memsizeStr = argv[++i];
        }
        else if (arg == "--dumpFolder" && i + 1 < argc) {
            dumpFolder = argv[++i];
        }
        else {
            std::cerr << "Argumento desconocido: " << arg << std::endl;
        }
    }
    if (portStr.empty() || memsizeStr.empty() || dumpFolder.empty()) {
        return false;
    }
    port = std::stoi(portStr);
    memSizeBytes = std::stoul(memsizeStr) * 1024 * 1024;
    return true;
}

// Función que inicializa y mantiene el servidor en ejecución.
void runServer(int port, size_t memSizeBytes, const std::string& dumpFolder) {
    WSADATA wsaData;
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = { 0 };

    // Inicializar Winsock.
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    // Crear el socket del servidor.
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Error al crear el socket" << std::endl;
        WSACleanup();
        return;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Error en bind" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    if (listen(server_fd, 5) == SOCKET_ERROR) {
        std::cerr << "Error en listen" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    std::cout << "Servidor MemoryManager escuchando en el puerto " << port << std::endl;

    // Inicializar el MemoryManager y establecer la carpeta de dump.
    MemoryManager::getInstance().init(memSizeBytes);
    //MemoryManager::getInstance().setDumpFolder(dumpFolder);
    std::cout << "Estado inicial del MemoryManager: " << MemoryManager::getInstance().getStatus() << std::endl;

    // Bucle principal: aceptar conexiones y procesar comandos.
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Error en accept" << std::endl;
            continue;
        }
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            closesocket(client_socket);
            continue;
        }
        std::string command(buffer);
        std::cout << "Comando recibido: " << command << std::endl;
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        std::string reply;
        if (cmd == "create") {
            int size;
            iss >> size;
            int blockID = MemoryManager::getInstance().createBlock(size);
            reply = "Block created with ID: " + std::to_string(blockID);
        }
        else if (cmd == "set") {
            int id;
            std::string value;
            iss >> id >> value;
            MemoryManager::getInstance().setValue(id, value);
            reply = "Value set for block " + std::to_string(id);
        }
        else if (cmd == "get") {
            int id;
            iss >> id;
            std::string value = MemoryManager::getInstance().getValue(id);
            reply = "Block " + std::to_string(id) + " has value: " + value;
        }
        else if (cmd == "status") {
            reply = MemoryManager::getInstance().getStatus();
        }
        else {
            reply = "Unknown command";
        }
        send(client_socket, reply.c_str(), reply.size(), 0);
        closesocket(client_socket);
    }

    closesocket(server_fd);
    WSACleanup();
}

int main(int argc, char** argv) {
    int port;
    size_t memSizeBytes;
    std::string dumpFolder;
    if (!parseArguments(argc, argv, port, memSizeBytes, dumpFolder)) {
        std::cerr << "Uso: MemoryManagerServer.exe --port <puerto> --memsize <MB> --dumpFolder <carpeta>" << std::endl;
        return 1;
    }
    runServer(port, memSizeBytes, dumpFolder);
    return 0;
}
