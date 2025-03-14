#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <cstddef>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstring>

class MemoryManager {
public:
    // Patrón Singleton: retorna la única instancia.
    static MemoryManager& getInstance();

    // Reserva el bloque de memoria en el heap (usando malloc) de totalSize bytes.
    void init(size_t totalSize);

    // Crea un sub-bloque de 'size' bytes y retorna un ID único (o -1 si falla).
    int createBlock(size_t size);

    // Asigna un valor (en forma de cadena) al bloque identificado por blockID.
    void setValue(int blockID, const std::string& value);

    // Obtiene el valor almacenado en el bloque identificado por blockID.
    std::string getValue(int blockID) const;

    // Retorna un string con el estado interno (totalSize, usedSize, nextID, cantidad de bloques).
    std::string getStatus() const;

    // Establece la carpeta donde se guardarán los dumps.
    void setDumpFolder(const std::string& folder);

private:
    MemoryManager();
    ~MemoryManager();

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Método privado para generar un dump de la memoria (se escribe el estado actual).
    void dumpMemory() const;

    // Función auxiliar que retorna un timestamp para nombrar los archivos de dump.
    std::string getCurrentTimestamp() const;

    // Estructura que describe un sub-bloque.
    struct BlockInfo {
        size_t offset;
        size_t size;
    };

    void* memoryBlock_;              // Bloque principal reservado.
    size_t totalSize_;               // Tamaño total del bloque.
    size_t usedSize_;                // Espacio ya asignado.
    int nextID_;                     // Para generar IDs únicos.
    std::map<int, BlockInfo> blocks_; // Mapea blockID a BlockInfo.

    mutable std::mutex mtx_;
    std::string dumpFolder_;         // Carpeta para guardar los dumps.
};

#endif // MEMORY_MANAGER_H
