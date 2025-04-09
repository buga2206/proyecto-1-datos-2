#include <iostream>
#include <sstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "MemoryManager.h"
#include <exception>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

bool parseArguments(int argc, char** argv, int& port, size_t& memSizeBytes, string& dumpFolder) {
    // Lectura básica de argumentos
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = stoi(argv[++i]);
        }
        else if (arg == "--memsize" && i + 1 < argc) {
            memSizeBytes = stoul(argv[++i]) * 1024 * 1024;
        }
        else if (arg == "--dumpFolder" && i + 1 < argc) {
            dumpFolder = argv[++i];
        }
    }
    return !dumpFolder.empty() && port > 0 && memSizeBytes > 0;
}

void runServer(int port, size_t memSizeBytes, const string& dumpFolder) {
    // Inicializa Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        cerr << "[SERVIDOR] WSAStartup falló: " << wsaResult << endl;
        return;
    }

    // Crea el socket del servidor
    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        cerr << "[SERVIDOR] Error al crear el socket del servidor." << endl;
        WSACleanup();
        return;
    }

    // Permitir reusar la dirección
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Asocia la dirección y el puerto
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        cerr << "[SERVIDOR] Error en bind. Código: " << WSAGetLastError() << endl;
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    // Escucha conexiones entrantes
    if (listen(server_fd, 5) == SOCKET_ERROR) {
        cerr << "[SERVIDOR] Error en listen. Código: " << WSAGetLastError() << endl;
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    // Inicializa el MemoryManager
    MemoryManager::getInstance().init(memSizeBytes);
    MemoryManager::getInstance().setDumpFolder(dumpFolder);

    cout << "[SERVIDOR] Iniciado correctamente." << endl;
    cout << "[SERVIDOR] Escuchando en el puerto " << port << endl;
    cout << "[SERVIDOR] Carpeta de dumps: " << dumpFolder << endl;

    // Bucle principal para aceptar y procesar conexiones
    while (true) {
        try {
            sockaddr_in clientAddr;
            int clientLen = sizeof(clientAddr);
            SOCKET client_socket = accept(server_fd, (sockaddr*)&clientAddr, &clientLen);
            if (client_socket == INVALID_SOCKET) {
                cerr << "[SERVIDOR] Error en accept. Código: " << WSAGetLastError() << endl;
                continue;
            }

            // Recibir datos
            char buffer[1024] = { 0 };
            int bytesReceived = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytesReceived <= 0) {
                cerr << "[SERVIDOR] Error al recibir datos o conexión cerrada." << endl;
                closesocket(client_socket);
                continue;
            }
            buffer[bytesReceived] = '\0';
            cout << "[SERVIDOR] Comando recibido: " << buffer << endl;

            // Procesar comando
            istringstream iss(buffer);
            string cmd;
            iss >> cmd;

            string reply;
            if (cmd == "create") {
                size_t size;
                string type;
                iss >> size >> type;
                int blockID = MemoryManager::getInstance().createBlock(size, type);
                if (blockID < 0) {
                    reply = "Error al crear bloque (espacio insuficiente o inválido).";
                }
                else {
                    reply = "Bloque creado con ID=" + to_string(blockID);
                }
            }
            else if (cmd == "set") {
                int id;
                iss >> id;
                string value;
                getline(iss, value); // Lee el resto de la línea
                // Elimina espacios en blanco iniciales
                size_t start = value.find_first_not_of(" ");
                if (start != string::npos) {
                    value = value.substr(start);
                }
                else {
                    value = "";
                }
                MemoryManager::getInstance().setValue(id, value);
                reply = "Valor asignado al bloque " + to_string(id);
            }
            else if (cmd == "get") {
                int id;
                iss >> id;
                string val = MemoryManager::getInstance().getValue(id);
                reply = "Bloque " + to_string(id) + " -> " + val;
            }
            else if (cmd == "increase") {
                int id;
                iss >> id;
                MemoryManager::getInstance().increaseRefCount(id);
                reply = "RefCount incrementado en bloque " + to_string(id);
            }
            else if (cmd == "decrease") {
                int id;
                iss >> id;
                MemoryManager::getInstance().decreaseRefCount(id);
                reply = "RefCount decrementado en bloque " + to_string(id);
            }
            else if (cmd == "status") {
                reply = MemoryManager::getInstance().getStatus();
            }
            else if (cmd == "map") {
                reply = MemoryManager::getInstance().getMemoryMap();
            }
            else {
                reply = "Comando inválido";
            }

            int sendResult = send(client_socket, reply.c_str(), (int)reply.size(), 0);
            if (sendResult == SOCKET_ERROR) {
                cerr << "[SERVIDOR] Error al enviar respuesta. Código: " << WSAGetLastError() << endl;
            }
            else {
                cout << "[SERVIDOR] Respuesta enviada: " << reply << endl;
            }

            closesocket(client_socket);
        }
        catch (const exception& ex) {
            cerr << "[SERVIDOR] Excepción capturada: " << ex.what() << endl;
        }
        catch (...) {
            cerr << "[SERVIDOR] Excepción desconocida capturada." << endl;
        }
    }

    closesocket(server_fd);
    WSACleanup();
}

//
// main tradicional por línea de comandos:
//
//int main(int argc, char** argv) {
//    int port = 0;
//    size_t memSizeBytes = 0;
//    string dumpFolder;
//
//    if (!parseArguments(argc, argv, port, memSizeBytes, dumpFolder)) {
//        cerr << "Uso: " << argv[0]
//             << " --port <puerto> --memsize <MB> --dumpFolder <carpeta>" << endl;
//        return 1;
//    }
//
//    runServer(port, memSizeBytes, dumpFolder);
//    return 0;
//}

// main para Visual Studio (o sin argumentos)
int main() {
    int port = 8080;
    // 100 MB de memoria
    size_t memSizeBytes = 100 * 1024 * 1024;
    string dumpFolder = "dumps";

    runServer(port, memSizeBytes, dumpFolder);
    return 0;
}
