#include <iostream>
#include "MPointer.h"
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

// Definición del nodo para la lista enlazada
template <typename T>
struct Node {
    T data;
    MPointer<Node<T>> next;
    // Constructor por defecto
    Node() : data(), next() {}
};

// Se definen operadores para serializar y deserializar Node<int>
// Formato: "data;nextID" (donde nextID es el ID del siguiente nodo)
ostream& operator<<(ostream& os, const Node<int>& node) {
    os << node.data << ";" << node.next.getID();
    return os;
}

istream& operator>>(istream& is, Node<int>& node) {
    string s;
    getline(is, s);
    size_t pos = s.find(";");
    if (pos != string::npos) {
        node.data = stoi(s.substr(0, pos));
        int nextID = stoi(s.substr(pos + 1));
        node.next.setID(nextID);
    }
    return is;
}

// Función para imprimir la lista enlazada usando getRemoteValue()
template <typename T>
void printList(MPointer<Node<T>>& head) {
    MPointer<Node<T>> current = head;
    int index = 0;
    while (true) {
        Node<T> nodeValue = current.getRemoteValue();
        cout << "Nodo " << index << " -> data: " << nodeValue.data << endl;
        if (nodeValue.next.isNull())
            break;
        current = nodeValue.next;
        index++;
    }
}

int main() {
    // Inicializar Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed." << endl;
        return 1;
    }

    // Configurar MPointer para conectarse al Memory Manager
    MPointer<Node<int>>::Init("127.0.0.1", 8080);

    // Crear el primer nodo (cabeza de la lista)
    auto head = MPointer<Node<int>>::New();
    Node<int> n1;
    n1.data = 10;
    head = n1;  // Usa operator=(const T&) para asignar n1

    // Crear el segundo nodo
    auto second = MPointer<Node<int>>::New();
    Node<int> n2;
    n2.data = 20;
    second = n2;

    // Enlazar: head->next = second
    Node<int> temp = head.getRemoteValue();
    temp.next = second;
    head = temp;  // Actualiza head en el servidor

    // Crear el tercer nodo
    auto third = MPointer<Node<int>>::New();
    Node<int> n3;
    n3.data = 30;
    third = n3;

    // Enlazar: second->next = third
    Node<int> temp2 = second.getRemoteValue();
    temp2.next = third;
    second = temp2;  // Actualiza second en el servidor

    // Imprimir la lista enlazada usando getRemoteValue()
    cout << "Lista enlazada:" << endl;
    printList(head);

    WSACleanup();
    return 0;
}
