#pragma once
#include <windows.h>
#include <string>
#include <vector>

class SerialReader {
public:
    SerialReader() = default;
    ~SerialReader();

    SerialReader(const SerialReader&) = delete;
    SerialReader& operator=(const SerialReader&) = delete;

    bool open(const char* portName, DWORD baudRate = CBR_115200);
    void close();
    bool isOpen() const { return m_hCom != INVALID_HANDLE_VALUE; }

    // Non-blocking: returns true + fills outLine if a complete line was read
    bool tryReadLine(std::string& outLine);

    // Send raw bytes
    bool send(const void* data, size_t len);
    bool sendString(const std::string& str);

    // Scan for available COM ports
    static std::vector<std::string> scanPorts();

private:
    HANDLE      m_hCom = INVALID_HANDLE_VALUE;
    std::string m_buffer;
};
