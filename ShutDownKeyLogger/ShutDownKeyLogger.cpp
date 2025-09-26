#include <windows.h>
#include <tlhelp32.h>

class SilentProcessKiller {
public:
    static bool KillProcessSilently(const wchar_t* processName) {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);

        bool killed = false;

        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, processName) == 0) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess != NULL) {
                        TerminateProcess(hProcess, 0);
                        CloseHandle(hProcess);
                        killed = true;
                    }
                }
            } while (Process32NextW(hSnapshot, &pe));
        }

        CloseHandle(hSnapshot);
        return killed;
    }
};

// Точка входа без консоли
int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    const wchar_t* targetProcess = L"keylogger.exe"; // Укажите ваше имя файла
    const wchar_t* targetProcess2 = L"Linekeylogger.exe"; // Укажите ваше имя файла
    (SilentProcessKiller::KillProcessSilently(targetProcess));
    (SilentProcessKiller::KillProcessSilently(targetProcess2));


    return 0;
}