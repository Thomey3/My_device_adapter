#include <iostream>
#include <string>
#include <windows.h>
#include <vector>
#include <sstream>
#include <iomanip> 

#define DEVICE_OK 0
#define DEVICE_ERR 1

class ETLController {
private:
    HANDLE hSerial;
    std::string portName;
    int baudRate;

public:
    ETLController(){

    }

    ~ETLController() {

    }

    bool openPort(const std::string& port, int rate) {
        std::wstring portNameString(portName.begin(), portName.end());
        std::wstring fullPortName = L"\\\\.\\" + portNameString;

        // Open the serial port
        hSerial = CreateFile(fullPortName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (hSerial == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                std::cerr << "Serial port does not exist.\n";
            }
            std::cerr << "Error opening serial port.\n";
            return false;
        }

        // Set device parameters (115200 baud, 1 start bit,
        // 1 stop bit, no parity)
        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        if (!GetCommState(hSerial, &dcbSerialParams)) {
            std::cerr << "Getting state error\n";
            return false;
        }

        dcbSerialParams.BaudRate = baudRate;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            std::cerr << "Error setting serial port state\n";
            return false;
        }

        return true;
    }

    void closePort() {
        CloseHandle(hSerial);
        std::cout << "Serial port closed." << std::endl;
    }

    int sendCommand(const std::string& command) {
        if (hSerial == INVALID_HANDLE_VALUE) {
            std::cerr << "Invalid handle. Port is not open." << std::endl;
            return DEVICE_ERR; // 确保定义了 DEVICE_ERR
        }

        std::vector<uint8_t> buffer(command.begin(), command.end());
        buffer.push_back('\r');
        buffer.push_back('\n');
        DWORD bytesWritten;
        if (!WriteFile(hSerial, buffer.data(), buffer.size(), &bytesWritten, NULL)) {
            std::cerr << "Error while writing to serial port.\n";
            return DEVICE_ERR;
        }
        else {
            std::cout << "Command sent: " << command << std::endl;
            return DEVICE_OK;
        }
    }

    std::string readResponse() {
        std::string response;
        char buffer;
        DWORD bytesRead;
        bool hasCarriageReturn = false;

        // Continue reading one byte at a time until "\r\n" is found
        while (ReadFile(hSerial, &buffer, 1, &bytesRead, NULL) && bytesRead > 0) {
            response += buffer;

            // Check for carriage return
            if (buffer == '\r') {
                hasCarriageReturn = true;
                continue;
            }

            // Check for line feed if the previous character was a carriage return
            if (hasCarriageReturn && buffer == '\n') {
                break;
            }

            // If we reach here, we didn't find "\r\n" so reset the flag
            hasCarriageReturn = false;
        }

        if (hasCarriageReturn) {
            // Remove trailing "\r\n"
            response.pop_back(); // Remove '\n'
            response.pop_back(); // Remove '\r'
        }
        else {
            // Handle the case where "\r\n" was not found
            std::cerr << "No CRLF found in the response" << std::endl;
        }

        return response;
    }

    int Start()
    {
        int err = sendCommand("START");
        if (err != DEVICE_OK)
        {
            return err;
        }
        std::string response = readResponse();
        if (response == "OK")
        {
            std::cout << "Device is ready." << std::endl;
            return DEVICE_OK;
        }
        else
        {
            std::cerr << "Unexpected response: " << response << std::endl;
            return DEVICE_ERR;
        }
    }

    int setCurrent(float current) //这里需要看一下回复是什么。
    {
        std::ostringstream ss;
        ss << "SETCURRENT=" << std::fixed << std::setprecision(2) << current << "\r\n";
        sendCommand(ss.str());
        std::string response = readResponse();
        std::cerr << "response: " << response << std::endl;
    }

    int getStatus() {
        // 发送STATUS命令
        sendCommand("STATUS");

        // 读取响应
        std::string response = readResponse();

        // 确保响应不为空，并检查是否包含有效的16进制前缀
        if (response.empty() || response.substr(0, 2) != "0x") {
            std::cerr << "Invalid or empty response received." << std::endl;
            return DEVICE_ERR; // 错误代码
        }

        // 尝试解析状态码
        try {
            // 将16进制字符串转换为整数
            unsigned long statusCode = std::stoul(response, nullptr, 16);
            return static_cast<int>(statusCode);
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing status response: " << e.what() << std::endl;
            return DEVICE_ERR; // 错误代码
        }
    }
    int reset() {
        int err = sendCommand("RESET");
        if (err != DEVICE_OK)
        {
            return err;
        }
        return DEVICE_OK;
    }
    int goToDFU() {
        int err = sendCommand("GOTODFU");
        if (err != DEVICE_OK)
        {
            return err;
        }
        return DEVICE_OK;
    }
    int goPro() {
        int err = sendCommand("GOPRO");
        if (err != DEVICE_OK)
        {
            return err;
        }
        return DEVICE_OK;
    }int goProCrc() {
        int err = sendCommand("GOPROCCRC");
        if (err != DEVICE_OK)
        {
            return err;
        }
        return DEVICE_OK;
    }
    std::string getId() {
        sendCommand("GETID");
        return readResponse();
    }
    std::string getVersion() {
        sendCommand("GETVERSION");
        return readResponse();
    }

    std::string getGitSha1() {
        sendCommand("GETGITSHA1");
        return readResponse();
    }

    std::string getSn() {
        sendCommand("GETSN");
        return readResponse();
    }

    std::string getDevices() {
        sendCommand("GETDEVICES");
        return readResponse();
    }
    float getCurrent() {
        sendCommand("GETCURRENT");
        return std::stof(readResponse());
    }
    void setFP(float power) {
        std::ostringstream ss;
        ss << "SETFP=" << power;
        sendCommand(ss.str());
    }
    float getFP() {
        sendCommand("GETFP");
        std::string response = readResponse();
        if (response == "NO")
        {
            return -10;
        }
        return std::stof(response);
    }
    std::string getFPMin() {
        sendCommand("GETFPMIN");
        return readResponse();
    }

    std::string getFPMax() {
        sendCommand("GETFPMAX");
        return readResponse();
    }

    float getTemp() {
        sendCommand("GETTEMP");
        return std::stof(readResponse());
    }

    int setTempLim(float tempLim) {
        std::ostringstream ss;
        ss << "SETTEMPLIM=" << tempLim;
        int err = sendCommand(ss.str());
        if (err != DEVICE_OK)
        {
            return err;
        }
        return DEVICE_OK;
    }
};
