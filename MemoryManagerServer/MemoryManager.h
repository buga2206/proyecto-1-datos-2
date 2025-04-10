#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstddef>
#include <map>
#include <mutex>
#include <vector>
#include <string>
#include <windows.h>
#include <atomic>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

// Usamos namespace std
using namespace std;

class MemoryManager {
public:
    static MemoryManager& getInstance();

    // Inicializa el bloque principal de memoria
    void init(size_t totalSize);

    // Crea un bloque de 'size' bytes con un tipo 'type' (ej: "int", "double", "string", etc.)
    int createBlock(size_t size, const string& type);

    // Escribe 'value' en el bloque identificado por blockID
    void setValue(int blockID, const string& value);

    // Lee el contenido del bloque identificado por blockID
    string getValue(int blockID) const;

    // Incrementa el contador de referencias del bloque
    void increaseRefCount(int blockID);

    // Decrementa el contador de referencias del bloque y libera si llega a 0
    void decreaseRefCount(int blockID);

    // Devuelve un resumen global de la memoria (tama�o total, usado, etc.)
    string getStatus() const;

    // Devuelve un "mapa de memoria" detallado (ID, tipo, direcci�n, refCount, etc.)
    string getMemoryMap() const;

    // Establece la carpeta para los dumps
    void setDumpFolder(const string& folder);

    ~MemoryManager();

private:
    MemoryManager();
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Estructura que describe un bloque ocupado
    struct BlockInfo {
        size_t offset;        // Desplazamiento dentro del bloque principal
        size_t size;          // Tama�o en bytes del bloque
        string type;          // Tipo (por ejemplo, "int", "double", "string", etc.)
        int refCount = 1;     // Contador de referencias
    };

    // Estructura que describe un bloque libre
    struct FreeBlock {
        size_t offset;
        size_t size;
    };

    // Bloque principal reservado con malloc
    void* memoryBlock_;
    // Tama�o total del bloque
    size_t totalSize_;
    // Tama�o en uso
    size_t usedSize_;
    // Generador de IDs �nicos
    int nextID_;

    // Mapa de IDs a info de bloque
    map<int, BlockInfo> blocks_;
    // Lista de bloques libres
    vector<FreeBlock> freeBlocks_;

    // Mutex recursivo para sincronizaci�n
    mutable recursive_mutex mtx_;

    // Carpeta donde se guardan los dumps
    string dumpFolder_;

    // Genera un volcado (dump) de la memoria en un archivo, registrando la acci�n realizada
    void dumpMemory(const string& action) const;

    // Fusiona bloques libres adyacentes
    void mergeFreeBlocks();

    // Genera un timestamp con fecha y hora
    string getCurrentTimestamp() const;

    // Para obtener la direcci�n real en memoria de un offset
    uintptr_t computeRealAddress(size_t offset) const;

    // Determina el tama�o m�nimo requerido para un tipo dado
    static size_t getMinSizeForType(const string& type);
};

#endif // MEMORY_MANAGER_H
