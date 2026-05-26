#include "serial_reader.h"
#include <cstdio>
#include <initguid.h>
#include <setupapi.h>
#include <devguid.h>

SerialReader::~SerialReader() {
    close();
}

bool SerialReader::open(const char* portName, DWORD baudRate) {
    close();

    char path[64];
    std::snprintf(path, sizeof(path), "\\\\.\\%s", portName);

    m_hCom = CreateFileA(path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (m_hCom == INVALID_HANDLE_VALUE) return false;

    SetupComm(m_hCom, 4096, 4096);

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(m_hCom, &dcb)) { close(); return false; }
    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    if (!SetCommState(m_hCom, &dcb)) { close(); return false; }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    if (!SetCommTimeouts(m_hCom, &timeouts)) { close(); return false; }

    PurgeComm(m_hCom, PURGE_RXCLEAR | PURGE_TXCLEAR);
    m_buffer.clear();
    return true;
}

void SerialReader::close() {
    if (m_hCom != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hCom);
        m_hCom = INVALID_HANDLE_VALUE;
    }
    m_buffer.clear();
}

bool SerialReader::tryReadLine(std::string& outLine) {
    if (m_hCom == INVALID_HANDLE_VALUE) return false;

    char buf[256];
    DWORD read = 0;
    if (ReadFile(m_hCom, buf, sizeof(buf), &read, nullptr) && read > 0) {
        m_buffer.append(buf, read);
    }

    if (m_buffer.size() > 4096) {
        m_buffer.clear();
        return false;
    }

    size_t pos = m_buffer.find('\n');
    if (pos == std::string::npos) return false;

    outLine = m_buffer.substr(0, pos);
    if (!outLine.empty() && outLine.back() == '\r') {
        outLine.pop_back();
    }
    m_buffer.erase(0, pos + 1);
    return true;
}

bool SerialReader::send(const void* data, size_t len) {
    if (m_hCom == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(m_hCom, data, (DWORD)len, &written, nullptr) != FALSE
           && written == len;
}

bool SerialReader::sendString(const std::string& str) {
    return send(str.data(), str.size());
}

std::vector<std::string> SerialReader::scanPorts() {
    std::vector<std::string> ports;

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_PORTS,
        nullptr,
        nullptr,
        DIGCF_PRESENT);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        // Fallback: try COM1-COM16
        for (int i = 1; i <= 16; i++) {
            char path[16];
            std::snprintf(path, sizeof(path), "\\\\.\\COM%d", i);
            HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                   0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
                char name[8];
                std::snprintf(name, sizeof(name), "COM%d", i);
                ports.push_back(name);
            }
        }
        return ports;
    }

    SP_DEVINFO_DATA devInfo = {};
    devInfo.cbSize = sizeof(devInfo);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfo); i++) {
        HKEY hKey = SetupDiOpenDevRegKey(hDevInfo, &devInfo,
                                          DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (hKey == INVALID_HANDLE_VALUE) continue;

        char portName[32] = {};
        DWORD size = sizeof(portName);
        DWORD type = 0;
        if (RegQueryValueExA(hKey, "PortName", nullptr, &type,
                             (LPBYTE)portName, &size) == ERROR_SUCCESS
            && type == REG_SZ) {
            ports.push_back(portName);
        }
        RegCloseKey(hKey);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return ports;
}
