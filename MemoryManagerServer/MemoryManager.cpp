#include "MemoryManager.h"
#include <iostream>
#include <cstdlib>
#include <fstream>

// Usamos namespace std
using namespace std;

// ----------------------------------------------------------------------------------
// Función auxiliar para obtener la ruta de la carpeta del ejecutable.
// Se utiliza para concatenar con el dumpFolder.
// ----------------------------------------------------------------------------------
static string getProjectDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    string fullPath(path);
    size_t lastSlash = fullPath.find_last_of("\\/");
    return fullPath.substr(0, lastSlash + 1);
}

// ----------------------------------------------------------------------------------
// Constructor y destructor
// ----------------------------------------------------------------------------------
MemoryManager::MemoryManager()
    : memoryBlock_(nullptr), totalSize_(0), usedSize_(0), nextID_(1) {
}

MemoryManager::~MemoryManager() {
    if (memoryBlock_) {
        free(memoryBlock_);
        memoryBlock_ = nullptr;
    }
}

// ----------------------------------------------------------------------------------
// Singleton
// ----------------------------------------------------------------------------------
MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

// ----------------------------------------------------------------------------------
// Inicializa el bloque principal de memoria
// ----------------------------------------------------------------------------------
void MemoryManager::init(size_t totalSize) {
    lock_guard<recursive_mutex> lock(mtx_);
    if (!memoryBlock_) {
        memoryBlock_ = malloc(totalSize);
        if (!memoryBlock_) {
            cerr << "Error: No se pudo asignar memoria de "
                << totalSize << " bytes." << endl;
            return;
        }
        totalSize_ = totalSize;
        usedSize_ = 0;

        // Al inicio, toda la memoria está libre
        FreeBlock initialFree{ 0, totalSize };
        freeBlocks_.push_back(initialFree);

        cout << "MemoryManager: Se ha reservado "
            << totalSize << " bytes." << endl;
    }
}

// ----------------------------------------------------------------------------------
// Determina el tamaño mínimo requerido para un tipo dado
// ----------------------------------------------------------------------------------
size_t MemoryManager::getMinSizeForType(const string& type) {
    if (type == "int")    return sizeof(int);
    if (type == "double") return sizeof(double);
    if (type == "float")  return sizeof(float);
    if (type == "long")   return sizeof(long);
    if (type == "bool")   return sizeof(bool);
    if (type == "char")   return sizeof(char);
    // Para un string, requerimos al menos 1 byte
    if (type == "string") return 1;
    // "raw" u otro, permitimos 0
    return 0;
}

// ----------------------------------------------------------------------------------
// Crea un bloque de 'size' bytes con tipo 'type'
// ----------------------------------------------------------------------------------
int MemoryManager::createBlock(size_t size, const string& type) {
    lock_guard<recursive_mutex> lock(mtx_);

    // Verificar tamaño mínimo según el tipo
    size_t minSize = getMinSizeForType(type);
    if (minSize > size) {
        cerr << "Error: Se solicitó un bloque de tipo '" << type
            << "' con tamaño " << size << " bytes, pero se requiere al menos "
            << minSize << " bytes para almacenar ese tipo de dato." << endl;
        return -1;
    }

    // Buscar en la lista de bloques libres
    for (auto it = freeBlocks_.begin(); it != freeBlocks_.end(); ++it) {
        if (it->size >= size) {
            // Se puede usar este bloque libre
            BlockInfo newBlock;
            newBlock.offset = it->offset;
            newBlock.size = size;
            newBlock.type = type;
            newBlock.refCount = 1; // Al crear, inicia con 1

            int blockID = nextID_++;
            blocks_[blockID] = newBlock;

            // Ajustar el bloque libre
            it->offset += size;
            it->size -= size;

            // Si el bloque libre quedó en tamaño 0, se elimina
            if (it->size == 0) {
                freeBlocks_.erase(it);
            }

            usedSize_ += size;

            // Generar dump
            ostringstream action;
            action << "CREATE -> ID=" << blockID
                << ", size=" << size
                << ", type=" << type;
            dumpMemory(action.str());
            return blockID;
        }
    }

    // Si no se encontró un bloque suficientemente grande
    cerr << "Espacio insuficiente para crear un bloque de "
        << size << " bytes." << endl;
    return -1;
}

// ----------------------------------------------------------------------------------
// setValue: Escribe 'value' en el bloque 'blockID'
// ----------------------------------------------------------------------------------
void MemoryManager::setValue(int blockID, const string& value) {
    lock_guard<recursive_mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it == blocks_.end()) {
        cerr << "setValue: Bloque " << blockID << " no encontrado." << endl;
        return;
    }

    const string& type = it->second.type;
    size_t offset = it->second.offset;
    size_t blockSize = it->second.size;

    // Verificar si el bloque es suficiente para escribir el tipo
    size_t minSize = getMinSizeForType(type);
    if (blockSize < minSize) {
        cerr << "Error: El bloque " << blockID << " es de " << blockSize
            << " bytes, insuficiente para escribir un '" << type
            << "' que requiere "
            << minSize << " bytes." << endl;
        return;
    }

    // Dependiendo del tipo, convertir y escribir
    try {
        if (type == "int") {
            int num = stoi(value);
            memcpy(static_cast<char*>(memoryBlock_) + offset, &num, sizeof(int));
        }
        else if (type == "double") {
            double num = stod(value);
            memcpy(static_cast<char*>(memoryBlock_) + offset, &num, sizeof(double));
        }
        else if (type == "float") {
            float num = stof(value);
            memcpy(static_cast<char*>(memoryBlock_) + offset, &num, sizeof(float));
        }
        else if (type == "long") {
            long num = stol(value);
            memcpy(static_cast<char*>(memoryBlock_) + offset, &num, sizeof(long));
        }
        else if (type == "bool") {
            bool b = (value == "true" || value == "1");
            memcpy(static_cast<char*>(memoryBlock_) + offset, &b, sizeof(bool));
        }
        else if (type == "char") {
            char c = (value.empty() ? '\0' : value[0]);
            memcpy(static_cast<char*>(memoryBlock_) + offset, &c, sizeof(char));
        }
        else if (type == "string") {
            // Copiamos la cadena, asegurando dejar espacio para el terminador nulo si cabe
            size_t maxCopy = (blockSize > 0 ? blockSize - 1 : 0);
            size_t copySize = min(maxCopy, value.size());
            memcpy(static_cast<char*>(memoryBlock_) + offset, value.c_str(), copySize);

            if (maxCopy > 0) {
                // Terminador nulo si cabe
                static_cast<char*>(memoryBlock_)[offset + copySize] = '\0';
            }
            if (copySize < value.size()) {
                cerr << "Advertencia: La cadena '" << value
                    << "' fue truncada al escribir en un bloque de "
                    << blockSize << " bytes." << endl;
            }
        }
        else {
            // Para tipos no reconocidos, se trata como buffer de string crudo
            size_t copySize = min(blockSize, value.size());
            memcpy(static_cast<char*>(memoryBlock_) + offset, value.c_str(), copySize);
            if (copySize < value.size()) {
                cerr << "Advertencia: Datos truncados al escribir en un bloque de "
                    << blockSize << " bytes." << endl;
            }
        }
    }
    catch (const exception& e) {
        cerr << "Error: No se pudo convertir '" << value
            << "' al tipo '" << type << "'. Excepción: "
            << e.what() << endl;
        return;
    }

    // Generar dump
    ostringstream action;
    action << "SET -> ID=" << blockID << ", newValue=" << value;
    dumpMemory(action.str());
}

// ----------------------------------------------------------------------------------
// getValue: Lee el contenido del bloque 'blockID' y lo retorna como string
// ----------------------------------------------------------------------------------
string MemoryManager::getValue(int blockID) const {
    lock_guard<recursive_mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it == blocks_.end()) {
        cerr << "getValue: Bloque " << blockID << " no encontrado." << endl;
        return "";
    }

    const string& type = it->second.type;
    size_t offset = it->second.offset;
    size_t blockSize = it->second.size;

    ostringstream oss;

    if (type == "int") {
        if (blockSize >= sizeof(int)) {
            int val = 0;
            memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(int));
            oss << val;
        }
        else {
            oss << "[Error: bloque muy pequeño para int]";
        }
    }
    else if (type == "double") {
        if (blockSize >= sizeof(double)) {
            double val = 0.0;
            memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(double));
            oss << val;
        }
        else {
            oss << "[Error: bloque muy pequeño para double]";
        }
    }
    else if (type == "float") {
        if (blockSize >= sizeof(float)) {
            float val = 0.0f;
            memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(float));
            oss << val;
        }
        else {
            oss << "[Error: bloque muy pequeño para float]";
        }
    }
    else if (type == "long") {
        if (blockSize >= sizeof(long)) {
            long val = 0;
            memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(long));
            oss << val;
        }
        else {
            oss << "[Error: bloque muy pequeño para long]";
        }
    }
    else if (type == "bool") {
        if (blockSize >= sizeof(bool)) {
            bool b = false;
            memcpy(&b, static_cast<char*>(memoryBlock_) + offset, sizeof(bool));
            oss << (b ? "true" : "false");
        }
        else {
            oss << "[Error: bloque muy pequeño para bool]";
        }
    }
    else if (type == "char") {
        if (blockSize >= sizeof(char)) {
            char val = 0;
            memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(char));
            oss << val;
        }
        else {
            oss << "[Error: bloque muy pequeño para char]";
        }
    }
    else if (type == "string") {
        // Para string: se asume que se guardó la cadena con terminador nulo si cabía
        const char* start = static_cast<const char*>(memoryBlock_) + offset;
        size_t len = strnlen(start, blockSize);
        oss << string(start, len);
    }
    else {
        // Para tipos no reconocidos, mostramos en hexadecimal
        unsigned char* data = static_cast<unsigned char*>(memoryBlock_) + offset;
        for (size_t i = 0; i < blockSize; i++) {
            oss << hex << setw(2) << setfill('0') << (int)data[i] << " ";
        }
    }
    return oss.str();
}

// ----------------------------------------------------------------------------------
// Aumenta el contador de referencias
// ----------------------------------------------------------------------------------
void MemoryManager::increaseRefCount(int blockID) {
    lock_guard<recursive_mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it != blocks_.end()) {
        it->second.refCount++;
        ostringstream action;
        action << "INCREASE -> ID=" << blockID
            << ", newRefCount=" << it->second.refCount;
        dumpMemory(action.str());
    }
    else {
        cerr << "increaseRefCount: Bloque " << blockID << " no encontrado." << endl;
    }
}

// ----------------------------------------------------------------------------------
// Disminuye el contador de referencias (libera si llega a 0)
// ----------------------------------------------------------------------------------
void MemoryManager::decreaseRefCount(int blockID) {
    lock_guard<recursive_mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it == blocks_.end()) {
        cerr << "decreaseRefCount: Bloque " << blockID << " no encontrado." << endl;
        return;
    }
    if (it->second.refCount > 0) {
        it->second.refCount--;
        ostringstream action;
        action << "DECREASE -> ID=" << blockID
            << ", newRefCount=" << it->second.refCount;
        if (it->second.refCount == 0) {
            // Lo marcamos como bloque libre
            freeBlocks_.push_back({ it->second.offset, it->second.size });
            usedSize_ -= it->second.size;
            blocks_.erase(it);
            mergeFreeBlocks();
            action << " (LIBERATED)";
        }
        dumpMemory(action.str());
    }
}

// ----------------------------------------------------------------------------------
// Retorna un resumen global de la memoria
// ----------------------------------------------------------------------------------
string MemoryManager::getStatus() const {
    lock_guard<recursive_mutex> lock(mtx_);
    ostringstream oss;
    oss << "Memory Status => [Total: " << totalSize_
        << " bytes, Used: " << usedSize_
        << " bytes, Free: " << (totalSize_ - usedSize_)
        << " bytes, BlockCount: " << blocks_.size() << "]";
    return oss.str();
}

// ----------------------------------------------------------------------------------
// Devuelve un "mapa" de la memoria con información detallada
// ----------------------------------------------------------------------------------
string MemoryManager::getMemoryMap() const {
    lock_guard<recursive_mutex> lock(mtx_);
    ostringstream oss;
    oss << "\n=== Memory Map ===\n";
    for (auto& kv : blocks_) {
        int bID = kv.first;
        const BlockInfo& info = kv.second;
        uintptr_t realAddr = computeRealAddress(info.offset);
        oss << "ID=" << bID
            << ", Offset=" << info.offset
            << ", Address=0x" << hex << realAddr << dec
            << ", Size=" << info.size
            << ", Type=" << info.type
            << ", RefCount=" << info.refCount
            << ", Value=" << getValue(bID) << "\n";
    }

    if (!freeBlocks_.empty()) {
        oss << "\n--- Free Blocks ---\n";
        for (auto& fb : freeBlocks_) {
            oss << "Offset=" << fb.offset << ", Size=" << fb.size << "\n";
        }
    }
    oss << "==================\n";
    return oss.str();
}

// ----------------------------------------------------------------------------------
// Establece la carpeta de dumps
// ----------------------------------------------------------------------------------
void MemoryManager::setDumpFolder(const string& folder) {
    lock_guard<recursive_mutex> lock(mtx_);
    dumpFolder_ = getProjectDirectory() + folder;

    DWORD attrib = GetFileAttributesA(dumpFolder_.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(dumpFolder_.c_str(), NULL);
    }
}

// ----------------------------------------------------------------------------------
// Genera un volcado de la memoria en un archivo "memory_dump.txt"
// ----------------------------------------------------------------------------------
void MemoryManager::dumpMemory(const string& action) const {
    if (dumpFolder_.empty()) return;

    string filename = dumpFolder_ + "\\memory_dump.txt";
    HANDLE hFile = CreateFileA(filename.c_str(),
        FILE_APPEND_DATA,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        cerr << "dumpMemory: No se pudo abrir/crear el archivo de dump." << endl;
        return;
    }

    ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] " << action << "\n"
        << getStatus() << "\n"
        << getMemoryMap() << "\n";

    string entry = oss.str();

    DWORD bytesWritten;
    WriteFile(hFile, entry.c_str(), static_cast<DWORD>(entry.size()), &bytesWritten, NULL);
    CloseHandle(hFile);
}

// ----------------------------------------------------------------------------------
// Fusiona bloques libres adyacentes
// ----------------------------------------------------------------------------------
void MemoryManager::mergeFreeBlocks() {
    if (freeBlocks_.empty()) return;

    sort(freeBlocks_.begin(), freeBlocks_.end(), [](const FreeBlock& a, const FreeBlock& b) {
        return a.offset < b.offset;
        });

    for (size_t i = 0; i < freeBlocks_.size() - 1;) {
        if (freeBlocks_[i].offset + freeBlocks_[i].size == freeBlocks_[i + 1].offset) {
            freeBlocks_[i].size += freeBlocks_[i + 1].size;
            freeBlocks_.erase(freeBlocks_.begin() + i + 1);
        }
        else {
            i++;
        }
    }
}

// ----------------------------------------------------------------------------------
// Genera un timestamp con fecha y hora (ms incluidos)
// ----------------------------------------------------------------------------------
string MemoryManager::getCurrentTimestamp() const {
    auto now = chrono::system_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    time_t t = chrono::system_clock::to_time_t(now);

    tm localTm;
    localtime_s(&localTm, &t);

    ostringstream oss;
    oss << put_time(&localTm, "%Y-%m-%d %H:%M:%S")
        << "." << setw(3) << setfill('0') << ms.count();
    return oss.str();
}

// ----------------------------------------------------------------------------------
// Computa la dirección real en memoria sumando offset al inicio del bloque principal
// ----------------------------------------------------------------------------------
uintptr_t MemoryManager::computeRealAddress(size_t offset) const {
    return reinterpret_cast<uintptr_t>(static_cast<char*>(memoryBlock_) + offset);
}
