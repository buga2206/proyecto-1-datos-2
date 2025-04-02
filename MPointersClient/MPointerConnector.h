#ifndef MPOINTER_CONNECTOR_H
#define MPOINTER_CONNECTOR_H

#include <winsock2.h>
#include <string>
#include <mutex>

class MPointerConnector {
public:
    static void Init(const std::string& host, int port);
    static std::string sendCommand(const std::string& cmd);
    static void Shutdown();

private:
    static SOCKET sock;
    static sockaddr_in serv_addr;
    static std::mutex mtx;
    static bool isInitialized;
};

#endif