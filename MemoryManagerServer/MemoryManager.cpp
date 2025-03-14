#include "MemoryManager.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <direct.h>  // _mkdir
#include <errno.h>

MemoryManager::MemoryManager()
    : memoryBlock_(nullptr), totalSize_(0), usedSize_(0), nextID_(1), dumpFolder_("")
{
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
    std::lock_guard<std::mutex> lock(mtx_);
    if (!memoryBlock_) {
        memoryBlock_ = malloc(totalSize);
        if (!memoryBlock_) {
            std::cerr << "Error: No se pudo asignar memoria." << std::endl;
            return;
        }
        totalSize_ = totalSize;
        usedSize_ = 0;
        std::cout << "MemoryManager: bloque de " << totalSize << " bytes reservado." << std::endl;
    }
    else {
        std::cout << "MemoryManager: ya existe un bloque reservado." << std::endl;
    }
}

int MemoryManager::createBlock(size_t size) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (usedSize_ + size > totalSize_) {
        std::cerr << "No hay suficiente espacio para crear un bloque de " << size << " bytes." << std::endl;
        return -1;
    }
    int blockID = nextID_++;
    BlockInfo info;
    info.offset = usedSize_;
    info.size = size;
    blocks_[blockID] = info;
    usedSize_ += size;
    std::cout << "MemoryManager: bloque " << blockID
        << " creado (size=" << size << ", offset=" << info.offset << ")." << std::endl;
    //dumpMemory();
    return blockID;
}

void MemoryManager::setValue(int blockID, const std::string& value) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it == blocks_.end()) {
        std::cerr << "setValue: Bloque " << blockID << " no encontrado." << std::endl;
        return;
    }
    BlockInfo info = it->second;
    size_t copySize = std::min(info.size, value.size());
    memcpy(static_cast<char*>(memoryBlock_) + info.offset, value.c_str(), copySize);
    if (copySize < info.size)
        ((char*)memoryBlock_)[info.offset + copySize] = '\0';
    std::cout << "MemoryManager: setValue en bloque " << blockID << " con valor: " << value << std::endl;
    //dumpMemory();
}

std::string MemoryManager::getValue(int blockID) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = blocks_.find(blockID);
    if (it == blocks_.end()) {
        std::cerr << "getValue: Bloque " << blockID << " no encontrado." << std::endl;
        return "";
    }
    BlockInfo info = it->second;
    const char* data = static_cast<const char*>(memoryBlock_) + info.offset;
    std::string value(data, strnlen(data, info.size));
    std::cout << "MemoryManager: getValue en bloque " << blockID << " devuelve: " << value << std::endl;
    return value;
}

std::string MemoryManager::getStatus() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    oss << "totalSize=" << totalSize_
        << ", usedSize=" << usedSize_
        << ", nextID=" << nextID_
        << ", blocks=" << blocks_.size();
    return oss.str();
}

void MemoryManager::setDumpFolder(const std::string& folder) {
    std::lock_guard<std::mutex> lock(mtx_);
    dumpFolder_ = folder;
    std::cout << "MemoryManager: Dump folder establecido en: " << dumpFolder_ << std::endl;
    //dumpMemory();
}

std::string MemoryManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &now_time_t);
#else
    localtime_r(&now_time_t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_"
        << std::setfill('0') << std::setw(3) << now_ms.count();
    return oss.str();
}

void MemoryManager::dumpMemory() const {
    if (dumpFolder_.empty()) return;

    // Intenta crear la carpeta de dump si no existe.
    int dirErr = _mkdir(dumpFolder_.c_str());
    if (dirErr == -1 && errno != EEXIST) {
        std::cerr << "No se pudo crear la carpeta de dump: " << dumpFolder_ << std::endl;
        return;
    }

    std::string timestamp = getCurrentTimestamp();
    // Usamos "\\" para rutas en Windows.
    std::string filename = dumpFolder_ + "\\" + timestamp + ".txt";

    std::ofstream dumpFile(filename, std::ios::out);
    if (!dumpFile.is_open()) {
        std::cerr << "No se pudo abrir el archivo de dump: " << filename << std::endl;
        return;
    }

    dumpFile << "MemoryManager Dump" << std::endl;
    dumpFile << getStatus() << std::endl;
    dumpFile.flush();
    dumpFile.close();
    std::cout << "Dump generado: " << filename << std::endl;
}
