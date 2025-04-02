#include <iostream>
#include <sstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

// Función auxiliar para enviar un comando y recibir la respuesta
string sendRequest(const string& serverIP, int serverPort, const string& command) {
    // Crear socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "[CLIENTE] Error al crear el socket." << endl;
        return "";
    }

    // Configurar dirección del servidor
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    // Conectar con el servidor
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "[CLIENTE] Error al conectar con el servidor." << endl;
        closesocket(sock);
        return "";
    }

    // Enviar el comando
    int sendResult = send(sock, command.c_str(), (int)command.size(), 0);
    if (sendResult == SOCKET_ERROR) {
        cerr << "[CLIENTE] Error al enviar el comando." << endl;
        closesocket(sock);
        return "";
    }
    cout << "[CLIENTE] Comando enviado: " << command << endl;

    // Recibir la respuesta
    char buffer[4096];
    int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived <= 0) {
        cerr << "[CLIENTE] Error al recibir la respuesta o conexión cerrada." << endl;
        closesocket(sock);
        return "";
    }
    buffer[bytesReceived] = '\0';
    string response(buffer);

    closesocket(sock);
    return response;
}

// Función runClient, fuera de main, con solicitudes predefinidas
void runClient() {
    string serverIP = "127.0.0.1";
    int serverPort = 8080;

    cout << "=== Inicio de pruebas del cliente ===\n" << endl;

    // 1) create 4 int
    {
        string cmd = "create 4 int";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    // 2) create 8 double
    {
        string cmd = "create 8 double";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    // 3) set 1 42  (se asume que el primer bloque tiene ID=1)
    {
        string cmd = "set 1 42";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    // 4) get 1
    {
        string cmd = "get 1";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    // 5) increase 1
    {
        string cmd = "increase 1";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    // 6) decrease 1
    {
        string cmd = "decrease 1";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    // 7) status
    {
        string cmd = "status";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    // 8) map
    {
        string cmd = "map";
        string resp = sendRequest(serverIP, serverPort, cmd);
        cout << "[SERVIDOR] " << resp << endl;
    }

    cout << "\n=== Fin de pruebas del cliente ===" << endl;
}

int main() {
    // Inicializa Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "[CLIENTE] WSAStartup falló." << endl;
        return 1;
    }

    runClient();

    WSACleanup();
    return 0;
}