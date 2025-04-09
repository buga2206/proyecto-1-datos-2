#ifndef MPOINTER_H
#define MPOINTER_H

#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <type_traits>
#include <sstream>
#include <mutex>
#include <iomanip>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib")
using namespace std;

/*
  MPointer es una clase template que actúa como puntero remoto.
  Internamente, guarda únicamente el identificador (blockID) del bloque asignado por el Memory Manager.

  Se comunican comandos (create, set, get, increase, decrease) con el servidor mediante sockets.

  Se sobrecargan los siguientes operadores:
    *  – Se usa un objeto Proxy para que *p sirva tanto para lectura (convertido a T) como para asignación.
    =  – Permite asignar un valor a un MPointer (o copiar otro MPointer, copiando el blockID y ajustando el refCount).
    &  – Está sobrecargado como miembro para retornar el blockID, _simulando_ la dirección remota.

  Nota: Debido a que sobrecargar operator& implica que &pInt ya no retorna la dirección de pInt en memoria local,
  se debe usar únicamente para obtener el identificador del bloque en el servidor (no para compararlo con nullptr).
*/

template <typename T>
class MPointer {
public:
    // Constructor por defecto: sin bloque asignado (blockID = -1)
    MPointer();

    // Constructor de copia: incrementa el refCount en el servidor
    MPointer(const MPointer<T>& other);

    // Operador de asignación de otro MPointer (copia el blockID e incrementa refCount)
    MPointer<T>& operator=(const MPointer<T>& other);

    // Operador de asignación desde un valor T (permite "p = valor")
    MPointer<T>& operator=(const T& val);

    // Destructor: decrementa el refCount en el servidor
    ~MPointer();

    // Clase Proxy para simular la desreferenciación
    class Proxy {
    public:
        Proxy(MPointer<T>& mp) : mp(mp) {}
        // Conversión a T: permite obtener el valor remoto mediante getValue()
        operator T() const { return mp.getValue(); }
        // Asignación: permite "*p = valor" invocando setValue()
        Proxy& operator=(const T& val) { mp.setValue(val); return *this; }
    private:
        MPointer<T>& mp;
    };

    // Sobrecarga de operator* (no const) retorna un objeto Proxy
    Proxy operator*() { return Proxy(*this); }
    // Sobrecarga const de operator* para lectura directa
    T operator*() const { return getValue(); }

    // Sobrecarga del operador &: retorna el blockID (la "dirección remota")
    int operator&() const { return blockID; }

    // Método para obtener el ID del bloque
    int getID() const;

    // Retorna true si no apunta a ningún bloque (blockID == -1)
    bool isNull() const { return blockID < 0; }

    // ------------------ Métodos estáticos de configuración ------------------
    // Inicializa la conexión con el Memory Manager (IP y puerto)
    static void Init(const string& ip, int port);

    // Crea un nuevo bloque remoto y retorna un MPointer para ese bloque
    static MPointer<T> New();

private:
    int blockID; // Identificador del bloque en el servidor Memory Manager

    // Envía un comando al servidor y retorna la respuesta en forma de string
    static string sendRequest(const string& command);

    // Función auxiliar para mapear el tipo T a un string (para el comando "create <size> <type>")
    static string typeName();

    // Métodos helper para asignar y obtener el valor remoto:
    void setValue(const T& val) const;
    T getValue() const;

    // Métodos para incrementar o decrementar el contador de referencias en el servidor
    static void increaseRef(int id);
    static void decreaseRef(int id);

    // Variables estáticas para la conexión con el servidor Memory Manager
    static string serverIP;
    static int serverPort;
};

// Definición de las variables estáticas
template <typename T> string MPointer<T>::serverIP = "127.0.0.1";
template <typename T> int MPointer<T>::serverPort = 8080;

// ----------------------------------------------------------------------
// Implementación de MPointer (en el header, al ser template)
// ----------------------------------------------------------------------

// Constructor por defecto
template <typename T>
MPointer<T>::MPointer() : blockID(-1) {}

// Constructor de copia
template <typename T>
MPointer<T>::MPointer(const MPointer<T>& other) : blockID(other.blockID) {
    if (blockID >= 0)
        increaseRef(blockID);
}

// Operador de asignación de otro MPointer
template <typename T>
MPointer<T>& MPointer<T>::operator=(const MPointer<T>& other) {
    if (this != addressof(other)) {  // Usar std::addressof para obtener la dirección real
        if (blockID >= 0)
            decreaseRef(blockID);
        blockID = other.blockID;
        if (blockID >= 0)
            increaseRef(blockID);
    }
    return *this;
}

// Operador de asignación desde un valor T
template <typename T>
MPointer<T>& MPointer<T>::operator=(const T& val) {
    if (blockID >= 0)
        setValue(val);
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

// getID: devuelve el blockID
template <typename T>
int MPointer<T>::getID() const {
    return blockID;
}

// Init: configura la dirección IP y el puerto del Memory Manager
template <typename T>
void MPointer<T>::Init(const string& ip, int port) {
    serverIP = ip;
    serverPort = port;
}

// New: crea un nuevo bloque remoto y retorna un MPointer para ese bloque
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

// setValue: envía el comando "set <blockID> <valor>"
// Se asume que el servidor tiene ramas específicas, por ejemplo, para "char" se copia un solo byte.
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
        // Eliminar espacios al inicio y final
        size_t start = valStr.find_first_not_of(" \t\r\n");
        size_t end = valStr.find_last_not_of(" \t\r\n");
        if (start != string::npos && end != string::npos)
            valStr = valStr.substr(start, end - start + 1);
        else
            valStr = "";

        if constexpr (is_same_v<T, string>) {
            return valStr;
        }
        else if constexpr (is_same_v<T, char>) {
            return valStr.empty() ? '\0' : valStr[0];
        }
        else if constexpr (is_same_v<T, bool>) {
            return (valStr == "true" || valStr == "1");
        }
        else if constexpr (is_arithmetic_v<T>) {
            istringstream iss(valStr);
            T result{};
            iss >> result;
            return result;
        }
        else {
            istringstream iss(valStr);
            T result{};
            iss >> result;
            return result;
        }
    }
    return T();
}

// increaseRef: envía "increase <id>" al servidor
template <typename T>
void MPointer<T>::increaseRef(int id) {
    if (id < 0) return;
    ostringstream oss;
    oss << "increase " << id;
    sendRequest(oss.str());
}

// decreaseRef: envía "decrease <id>" al servidor
template <typename T>
void MPointer<T>::decreaseRef(int id) {
    if (id < 0) return;
    ostringstream oss;
    oss << "decrease " << id;
    sendRequest(oss.str());
}

// typeName: mapea el tipo T a un string para el comando "create"
// Soporta int, double, float, bool, string, long, char y unsigned char (como "byte")
template <typename T>
string MPointer<T>::typeName() {
    if constexpr (is_same_v<T, int>)             return "int";
    if constexpr (is_same_v<T, double>)          return "double";
    if constexpr (is_same_v<T, float>)           return "float";
    if constexpr (is_same_v<T, bool>)            return "bool";
    if constexpr (is_same_v<T, string>)          return "string";
    if constexpr (is_same_v<T, long>)            return "long";
    if constexpr (is_same_v<T, char>)            return "char";
    if constexpr (is_same_v<T, unsigned char>)   return "byte";
    return "raw";
}

// sendRequest: se conecta al servidor, envía el comando y retorna la respuesta
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
