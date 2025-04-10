#include <iostream>
#include "MPointer.h"
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

int main() {
    // Inicializar Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed." << endl;
        return 1;
    }

    cout << "=== PRUEBAS CON MPointer ===" << endl;

    // Configurar el Memory Manager para todos los tipos
    MPointer<int>::Init("127.0.0.1", 8080);
    MPointer<string>::Init("127.0.0.1", 8080);
    MPointer<char>::Init("127.0.0.1", 8080);
    MPointer<long>::Init("127.0.0.1", 8080);
    MPointer<float>::Init("127.0.0.1", 8080);
    MPointer<bool>::Init("127.0.0.1", 8080);

    // --- Prueba con int ---
    MPointer<int> pInt = MPointer<int>::New();
    cout << "[CLIENTE] MPointer<int> creado con ID: " << pInt.getID() << endl;
    *pInt = 123;
    int valorInt = *pInt;
    cout << "[CLIENTE] Valor leído desde MPointer<int>: " << valorInt << endl;

    // --- Prueba con string ---
    MPointer<string> pStr = MPointer<string>::New();
    cout << "\n[CLIENTE] MPointer<string> creado con ID: " << pStr.getID() << endl;
    *pStr = "Hello World";
    string valorStr = *pStr;
    cout << "[CLIENTE] Valor leído desde MPointer<string>: " << valorStr << endl;

    // --- Prueba con char ---
    MPointer<char> pChar = MPointer<char>::New();
    cout << "\n[CLIENTE] MPointer<char> creado con ID: " << pChar.getID() << endl;
    *pChar = 'X';
    char valorChar = *pChar;
    cout << "[CLIENTE] Valor leído desde MPointer<char>: " << valorChar << endl;

    // --- Prueba con long ---
    MPointer<long> pLong = MPointer<long>::New();
    cout << "\n[CLIENTE] MPointer<long> creado con ID: " << pLong.getID() << endl;
    *pLong = 9876543210L;
    long valorLong = *pLong;
    cout << "[CLIENTE] Valor leído desde MPointer<long>: " << valorLong << endl;

    // --- Prueba con float ---
    MPointer<float> pFloat = MPointer<float>::New();
    cout << "\n[CLIENTE] MPointer<float> creado con ID: " << pFloat.getID() << endl;
    *pFloat = 3.14f;
    float valorFloat = *pFloat;
    cout << "[CLIENTE] Valor leído desde MPointer<float>: " << valorFloat << endl;

    // --- Prueba con bool ---
    MPointer<bool> pBool = MPointer<bool>::New();
    cout << "\n[CLIENTE] MPointer<bool> creado con ID: " << pBool.getID() << endl;
    *pBool = true;
    bool valorBool = *pBool;
    cout << "[CLIENTE] Valor leído desde MPointer<bool>: " << (valorBool ? "true" : "false") << endl;

    // --- Prueba del operador & ---
    cout << "\n[CLIENTE] Operador & aplicado a pInt retorna: " << &pInt << endl;

    // --- Prueba de copia de MPointers: pInt2 = pInt ---
    MPointer<int> pInt2;
    pInt2 = pInt;  // Copia el blockID y aumenta el refCount en el servidor.
    cout << "\n[CLIENTE] Tras pInt2 = pInt:" << endl;
    cout << "           pInt  ID: " << &pInt << " - Valor: " << *pInt << endl;
    cout << "           pInt2 ID: " << &pInt2 << " - Valor: " << *pInt2 << endl;

    // Instrucciones de uso:
    // - MPointer<T>::New() crea un puntero remoto (se reserva localmente solo el blockID).
    // - Para asignar un valor, se usa: *p = valor;
    // - Para leer el valor, se usa: T x = *p;
    // - Para obtener el identificador (la “dirección remota”), se usa p.getID() o el operador & (sobrecargado).
    // - Para copiar un puntero, se usa p2 = p; (esto incrementa el refCount en el servidor).

    WSACleanup();
    return 0;
}
