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

class MemoryManager {
public:
    static MemoryManager& getInstance();

    // Inicializa el bloque principal de memoria
    void init(size_t totalSize);

    // Crea un bloque de 'size' bytes con un tipo 'type' (ej: "int", "double", "string", etc.)
    int createBlock(size_t size, const std::string& type);

    // Escribe 'value' en el bloque identificado por blockID
    void setValue(int blockID, const std::string& value);

    // Lee el contenido del bloque identificado por blockID
    std::string getValue(int blockID) const;

    // Incrementa el contador de referencias del bloque
    void increaseRefCount(int blockID);

    // Decrementa el contador de referencias del bloque y libera si llega a 0
    void decreaseRefCount(int blockID);

    // Devuelve un resumen global de la memoria (tamaño total, usado, etc.)
    std::string getStatus() const;

    // Devuelve un "mapa de memoria" detallado (ID, tipo, dirección, refCount, etc.)
    std::string getMemoryMap() const;

    // Establece la carpeta para los dumps
    void setDumpFolder(const std::string& folder);

    ~MemoryManager();

private:
    MemoryManager();
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Genera un volcado (dump) de la memoria en un archivo, registrando la acción realizada
    void dumpMemory(const std::string& action) const;

    // Fusiona bloques libres adyacentes
    void mergeFreeBlocks();

    // Genera un timestamp con fecha y hora
    std::string getCurrentTimestamp() const;

    // Para obtener la dirección real en memoria de un offset
    uintptr_t computeRealAddress(size_t offset) const;

    // Estructura que describe un bloque ocupado
    struct BlockInfo {
        size_t offset;        // Desplazamiento dentro del bloque principal
        size_t size;          // Tamaño en bytes del bloque
        std::string type;     // Tipo (por ejemplo, "int", "double", "string", etc.)
        int refCount = 1;     // Contador de referencias
    };

    // Estructura que describe un bloque libre
    struct FreeBlock {
        size_t offset;
        size_t size;
    };

    void* memoryBlock_;               // Bloque principal reservado con malloc
    size_t totalSize_;                // Tamaño total del bloque
    size_t usedSize_;                 // Tamaño en uso
    int nextID_;                      // Generador de IDs únicos
    std::map<int, BlockInfo> blocks_; // Mapa de IDs a info de bloque
    std::vector<FreeBlock> freeBlocks_;
    mutable std::recursive_mutex mtx_;          // Mutex para sincronización
    std::string dumpFolder_;          // Carpeta donde se guardan los dumps
};

#endif // MEMORY_MANAGER_H
