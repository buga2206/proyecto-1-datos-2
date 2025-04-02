#ifndef MPOINTER_H
#define MPOINTER_H

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <type_traits>
#include <sstream>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")
using namespace std;

// ----------------------------------------------------------------------
// Clase MPointer: encapsula un "puntero" remoto a un bloque en el Memory Manager
// ----------------------------------------------------------------------
template <typename T>
class MPointer {
public:
    // Constructor por defecto: sin bloque asignado (blockID = -1)
    MPointer();

    // Constructor de copia: aumenta el refCount en el servidor
    MPointer(const MPointer<T>& other);

    // Operador de asignación de otro MPointer
    MPointer<T>& operator=(const MPointer<T>& other);

    // Operador de asignación desde un valor T: permite asignar directamente un T
    MPointer<T>& operator=(const T& val);

    // Destructor: decrementa el refCount en el servidor
    ~MPointer();

    // Método para obtener el ID del bloque (en lugar de sobrecargar operator&)
    int getID() const;

    // Método para actualizar el ID (para deserialización)
    void setID(int id);

    // Método público para obtener el valor remoto sin usar operator*
    T getRemoteValue() const;

    // Retorna true si no apunta a ningún bloque (blockID == -1)
    bool isNull() const { return blockID < 0; }

    // ------------------ Métodos estáticos de configuración ------------------
    // Inicializa la conexión con el Memory Manager (IP y puerto)
    static void Init(const string& ip, int port);

    // Crea un nuevo bloque remoto para almacenar un T y retorna un MPointer a él
    static MPointer<T> New();

private:
    // ID del bloque en el Memory Manager
    int blockID;

    // Envia un comando al servidor y retorna la respuesta
    static string sendRequest(const string& command);

    // Auxiliar para mapear tipo T a un string (para "create <size> <type>")
    static string typeName();

    // Métodos helper para setear y obtener el valor remoto
    void setValue(const T& val) const;
    T getValue() const;

    // Métodos para incrementar o decrementar el contador de referencias en el servidor
    static void increaseRef(int id);
    static void decreaseRef(int id);

    // Variables estáticas para la conexión con el Memory Manager
    static string serverIP;
    static int serverPort;
};

// Definición de las variables estáticas
template <typename T> string MPointer<T>::serverIP = "127.0.0.1";
template <typename T> int MPointer<T>::serverPort = 8080;

// ----------------------------------------------------------------------
// Implementaciones de MPointer (al ser template, se definen en el header)
// ----------------------------------------------------------------------

// Constructor por defecto
template <typename T>
MPointer<T>::MPointer() : blockID(-1) {}

// Constructor de copia
template <typename T>
MPointer<T>::MPointer(const MPointer<T>& other) : blockID(other.blockID) {
    if (blockID >= 0) {
        increaseRef(blockID);
    }
}

// Operador de asignación de otro MPointer
template <typename T>
MPointer<T>& MPointer<T>::operator=(const MPointer<T>& other) {
    if (this != &other) {
        if (blockID >= 0) {
            decreaseRef(blockID);
        }
        blockID = other.blockID;
        if (blockID >= 0) {
            increaseRef(blockID);
        }
    }
    return *this;
}

// Operador de asignación desde un valor T
template <typename T>
MPointer<T>& MPointer<T>::operator=(const T& val) {
    if (blockID >= 0) {
        setValue(val);
    }
    return *this;
}

// Destructor
template <typename T>
MPointer<T>::~MPointer() {
    if (blockID >= 0) {
        decreaseRef(blockID);
        blockID = -1;
    }
}

// getID: devuelve el ID del bloque
template <typename T>
int MPointer<T>::getID() const {
    return blockID;
}

// setID: actualiza el blockID (usado en deserialización)
template <typename T>
void MPointer<T>::setID(int id) {
    blockID = id;
}

// getRemoteValue: obtiene el valor remoto sin usar operator*
template <typename T>
T MPointer<T>::getRemoteValue() const {
    if (blockID >= 0) {
        return getValue();
    }
    return T();
}

// Inicializa la conexión con el Memory Manager
template <typename T>
void MPointer<T>::Init(const string& ip, int port) {
    serverIP = ip;
    serverPort = port;
}

// New: crea un nuevo bloque remoto y retorna un MPointer a él
template <typename T>
MPointer<T> MPointer<T>::New() {
    size_t sizeBytes = sizeof(T);
    string tname = typeName();

    ostringstream oss;
    oss << "create " << sizeBytes << " " << tname;
    string resp = sendRequest(oss.str());

    int newID = -1;
    size_t pos = resp.find("ID=");
    if (pos != string::npos) {
        istringstream iss(resp.substr(pos + 3));
        iss >> newID;
    }

    MPointer<T> mp;
    mp.blockID = newID;
    return mp;
}

// setValue: envía "set <blockID> <valor>"
template <typename T>
void MPointer<T>::setValue(const T& val) const {
    ostringstream oss;
    oss << "set " << blockID << " " << val;
    sendRequest(oss.str());
}

// getValue: envía "get <blockID>" y procesa la respuesta
template <typename T>
T MPointer<T>::getValue() const {
    ostringstream oss;
    oss << "get " << blockID;
    string resp = sendRequest(oss.str());
    size_t arrowPos = resp.find("->");
    if (arrowPos != string::npos) {
        string valStr = resp.substr(arrowPos + 2);
        if constexpr (is_arithmetic_v<T>) {
            istringstream iss(valStr);
            T result{};
            iss >> result;
            return result;
        }
        else if constexpr (is_same_v<T, string>) {
            return valStr;
        }
        else {
            // Para otros tipos, se usa el operador >> (se espera que T tenga definido operator>>)
            istringstream iss(valStr);
            T result{};
            iss >> result;
            return result;
        }
    }
    return T{};
}

// increaseRef: envía "increase <id>"
template <typename T>
void MPointer<T>::increaseRef(int id) {
    if (id < 0) return;
    ostringstream oss;
    oss << "increase " << id;
    sendRequest(oss.str());
}

// decreaseRef: envía "decrease <id>"
template <typename T>
void MPointer<T>::decreaseRef(int id) {
    if (id < 0) return;
    ostringstream oss;
    oss << "decrease " << id;
    sendRequest(oss.str());
}

// typeName: mapea el tipo T a un string
template <typename T>
string MPointer<T>::typeName() {
    if constexpr (is_same_v<T, int>)    return "int";
    if constexpr (is_same_v<T, double>) return "double";
    if constexpr (is_same_v<T, float>)  return "float";
    if constexpr (is_same_v<T, bool>)   return "bool";
    if constexpr (is_same_v<T, string>) return "string";
    return "raw";
}

// sendRequest: envía el comando al servidor y retorna la respuesta
template <typename T>
string MPointer<T>::sendRequest(const string& command) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        return "Error: socket()";
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        return "Error: connect()";
    }
    send(sock, command.c_str(), static_cast<int>(command.size()), 0);

    char buffer[1024];
    int bytesReceived = recv(sock, buffer, 1023, 0);
    if (bytesReceived <= 0) {
        closesocket(sock);
        return "Error: recv()";
    }
    buffer[bytesReceived] = '\0';
    string response(buffer);

    closesocket(sock);
    return response;
}

#endif // MPOINTER_H
