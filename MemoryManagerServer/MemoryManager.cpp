#include "MemoryManager.h"
#include <iostream>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <sstream>

using namespace std;

//
// Función auxiliar para obtener la ruta de la carpeta del ejecutable.
// Se utiliza para concatenar con el dumpFolder.
//
string getProjectDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    string fullPath(path);
    size_t lastSlash = fullPath.find_last_of("\\/");
    return fullPath.substr(0, lastSlash + 1);
}

MemoryManager::MemoryManager()
    : memoryBlock_(nullptr), totalSize_(0), usedSize_(0), nextID_(1) {
}

MemoryManager::~MemoryManager() {
    if (memoryBlock_) {
        free(memoryBlock_);
        memoryBlock_ = nullptr;
    }
}

MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::init(size_t totalSize) {
    lock_guard<recursive_mutex> lock(mtx_);
    if (!memoryBlock_) {
        memoryBlock_ = malloc(totalSize);
        if (!memoryBlock_) {
            cerr << "Error: No se pudo asignar memoria de " << totalSize << " bytes." << endl;
            return;
        }
        totalSize_ = totalSize;
        usedSize_ = 0;

        // Al inicio, toda la memoria está libre
        FreeBlock initialFree{ 0, totalSize };
        freeBlocks_.push_back(initialFree);

        cout << "MemoryManager: Se ha reservado " << totalSize << " bytes." << endl;
    }
}

int MemoryManager::createBlock(size_t size, const string& type) {
    lock_guard<recursive_mutex> lock(mtx_);

    // Buscar primero en la lista de bloques libres
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

            // Generar dump con la acción
            ostringstream action;
            action << "CREATE -> ID=" << blockID
                << ", size=" << size
                << ", type=" << type;
            dumpMemory(action.str());
            return blockID;
        }
    }

    // Si no se encontró un bloque libre suficientemente grande
    cerr << "Espacio insuficiente para crear un bloque de " << size << " bytes." << endl;
    return -1;
}

void MemoryManager::setValue(int blockID, const string& value) {
    lock_guard<recursive_mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it == blocks_.end()) {
        cerr << "setValue: Bloque " << blockID << " no encontrado." << endl;
        return;
    }

    const string& type = it->second.type;
    size_t offset = it->second.offset;

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
        // Para char: guarda exactamente un carácter
        char c = (value.empty() ? '\0' : value[0]);
        memcpy(static_cast<char*>(memoryBlock_) + offset, &c, sizeof(char));
    }
    else if (type == "string") {
        // Para string: copiamos la cadena, asegurando dejar espacio para el terminador nulo.
        // Se copia un máximo de (size - 1) bytes.
        size_t maxCopy = (it->second.size > 0 ? it->second.size - 1 : 0);
        size_t copySize = min(maxCopy, value.size());
        memcpy(static_cast<char*>(memoryBlock_) + offset, value.c_str(), copySize);
        // Establecer el terminador nulo si es posible
        if (maxCopy > 0) {
            static_cast<char*>(memoryBlock_)[offset + copySize] = '\0';
        }
    }
    else {
        // Para otros tipos: se trata como buffer de string.
        strncpy_s(static_cast<char*>(memoryBlock_) + offset,
            it->second.size,
            value.c_str(),
            _TRUNCATE);
    }

    ostringstream action;
    action << "SET -> ID=" << blockID << ", newValue=" << value;
    dumpMemory(action.str());
}


string MemoryManager::getValue(int blockID) const {
    lock_guard<recursive_mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it == blocks_.end()) {
        cerr << "getValue: Bloque " << blockID << " no encontrado." << endl;
        return "";
    }

    const string& type = it->second.type;
    size_t offset = it->second.offset;
    ostringstream oss;

    if (type == "int") {
        int val = 0;
        memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(int));
        oss << val;
    }
    else if (type == "double") {
        double val = 0.0;
        memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(double));
        oss << val;
    }
    else if (type == "float") {
        float val = 0.0f;
        memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(float));
        oss << val;
    }
    else if (type == "long") {
        long val = 0;
        memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(long));
        oss << val;
    }
    else if (type == "bool") {
        bool b = false;
        memcpy(&b, static_cast<char*>(memoryBlock_) + offset, sizeof(bool));
        oss << (b ? "true" : "false");
    }
    else if (type == "char") {
        char val = 0;
        if (it->second.size >= sizeof(char))
            memcpy(&val, static_cast<char*>(memoryBlock_) + offset, sizeof(char));
        oss << val;
    }
    else if (type == "string") {
        // Para string: se asume que se guardó la cadena con terminador nulo.
        const char* start = static_cast<const char*>(memoryBlock_) + offset;
        // Leer hasta el primer '\0' o hasta info.size, lo que ocurra primero.
        size_t len = strnlen(start, it->second.size);
        oss << string(start, len);
    }
    else {
        // Para otros tipos, se imprime en hexadecimal
        unsigned char* data = static_cast<unsigned char*>(memoryBlock_) + offset;
        for (size_t i = 0; i < it->second.size; i++) {
            oss << hex << setw(2) << setfill('0') << (int)data[i] << " ";
        }
    }
    return oss.str();
}


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
}

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
        // Si llega a 0, se libera el bloque
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

string MemoryManager::getStatus() const {
    lock_guard<recursive_mutex> lock(mtx_);
    ostringstream oss;
    oss << "Memory Status => [Total: " << totalSize_
        << " bytes, Used: " << usedSize_
        << " bytes, Free: " << (totalSize_ - usedSize_)
        << " bytes, BlockCount: " << blocks_.size() << "]";
    return oss.str();
}

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
            << ", RefCount=" << info.refCount;

        // A continuación, leemos el contenido del bloque y lo mostramos.
        oss << ", Value=";
        if (info.type == "int") {
            int val = 0;
            if (info.size >= sizeof(int))
                memcpy(&val, static_cast<char*>(memoryBlock_) + info.offset, sizeof(int));
            oss << val;
        }
        else if (info.type == "double") {
            double val = 0.0;
            if (info.size >= sizeof(double))
                memcpy(&val, static_cast<char*>(memoryBlock_) + info.offset, sizeof(double));
            oss << val;
        }
        else if (info.type == "float") {
            float val = 0.0f;
            if (info.size >= sizeof(float))
                memcpy(&val, static_cast<char*>(memoryBlock_) + info.offset, sizeof(float));
            oss << val;
        }
        else if (info.type == "bool") {
            bool val = false;
            if (info.size >= sizeof(bool))
                memcpy(&val, static_cast<char*>(memoryBlock_) + info.offset, sizeof(bool));
            oss << (val ? "true" : "false");
        }
        else if (info.type == "string") {
            const char* start = static_cast<const char*>(memoryBlock_) + info.offset;
            size_t len = strnlen(start, info.size);
            string val(start, len);
            oss << val;
		}
		else if (info.type == "long") {
			long val = 0;
			if (info.size >= sizeof(long))
				memcpy(&val, static_cast<char*>(memoryBlock_) + info.offset, sizeof(long));
			oss << val;
		}
		else if (info.type == "char") {
			char val = 0;
			if (info.size >= sizeof(char))
				memcpy(&val, static_cast<char*>(memoryBlock_) + info.offset, sizeof(char));
			oss << val;
		}
        else {
            // Para tipos no reconocidos, mostramos todo el contenido en hexadecimal
            unsigned char* data = static_cast<unsigned char*>(memoryBlock_) + info.offset;
            for (size_t i = 0; i < info.size; i++) {
                oss << hex << setw(2) << setfill('0') << (int)data[i] << " ";
            }
        }
        oss << "\n";
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

void MemoryManager::setDumpFolder(const string& folder) {
    lock_guard<recursive_mutex> lock(mtx_);
    // Si no es ruta absoluta, concatenamos con la carpeta del proyecto
    dumpFolder_ = getProjectDirectory() + folder;
    // Se puede crear de una vez la carpeta
    DWORD attrib = GetFileAttributesA(dumpFolder_.c_str());
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryA(dumpFolder_.c_str(), NULL);
    }
}

void MemoryManager::dumpMemory(const string& action) const {
    if (dumpFolder_.empty()) return;

    // Creamos (o abrimos) un archivo de dump con nombre "memory_dump.txt" y agregamos
    // la entrada al final.
    string filename = dumpFolder_ + "\\memory_dump.txt";
    HANDLE hFile = CreateFileA(filename.c_str(), FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        cerr << "dumpMemory: No se pudo abrir/crear el archivo de dump." << endl;
        return;
    }

    // Construimos el contenido del volcado
    // Incluir timestamp, la acción que ocurrió y el estado completo
    ostringstream oss;
    oss << "[" << getCurrentTimestamp() << "] " << action << "\n"
        << getStatus() << "\n"
        << getMemoryMap() << "\n";

    string entry = oss.str();

    DWORD bytesWritten;
    WriteFile(hFile, entry.c_str(), static_cast<DWORD>(entry.size()), &bytesWritten, NULL);
    CloseHandle(hFile);
}

void MemoryManager::mergeFreeBlocks() {
    // Ordena los bloques libres por offset
    if (freeBlocks_.empty()) return;

    sort(freeBlocks_.begin(), freeBlocks_.end(), [](const FreeBlock& a, const FreeBlock& b) {
        return a.offset < b.offset;
        });

    // Fusiona adyacentes
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

uintptr_t MemoryManager::computeRealAddress(size_t offset) const {
    // Sumamos el offset a la dirección base del memoryBlock_
    // reinterpret_cast a uintptr_t para obtener la dirección como número
    return reinterpret_cast<uintptr_t>(static_cast<char*>(memoryBlock_) + offset);
}
