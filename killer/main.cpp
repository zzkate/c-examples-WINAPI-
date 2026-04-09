#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <map>
#include <regex>

std::map<std::string, std::string> parseArgs(int argc, char** argv){
    std::map<std::string, std::string> params;
    std::string key, value;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "--") {
            key = arg.substr(2); // Ключ без --
            if (i + 1 < argc) {
                value = argv[++i]; // Следующий аргумент - значение
                params[key] = value;
            }
        }
    }
    return params;
}

bool wildcardMatch(const std::wstring& text, const std::wstring& pattern) {
    // 1. Экранируем спецсимволы regex, кроме '*' и '?'
    std::wstring regexPattern;
    for (wchar_t c : pattern) {
        if (c == L'*') regexPattern += L".*";
        else if (c == L'?') regexPattern += L".";
        else if (wcschr(L".+^$|()[]{}", c)) regexPattern += L"\\", regexPattern += c;
        else regexPattern += c;
    }

    // 2. Создаем регулярное выражение
    std::wregex re(L"^" + regexPattern + L"$");
    return std::regex_match(text, re);
}

// Функция для завершения процессов
void killProcessesByMaskAndTime(const std::wstring& mask, ULONGLONG maxAgeSeconds) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            // Проверка маски имени
            if (wildcardMatch(pe.szExeFile, mask)) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    FILETIME ftCreate, ftExit, ftKernel, ftUser;
                    if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                        // Получаем текущее время
                        FILETIME ftNow;
                        GetSystemTimeAsFileTime(&ftNow);

                        // Конвертируем FILETIME в ULONGLONG (100-нс интервалы)
                        ULARGE_INTEGER now, create;
                        now.LowPart = ftNow.dwLowDateTime;
                        now.HighPart = ftNow.dwHighDateTime;
                        create.LowPart = ftCreate.dwLowDateTime;
                        create.HighPart = ftCreate.dwHighDateTime;

                        // Разница в секундах
                        ULONGLONG ageSeconds = (now.QuadPart - create.QuadPart) / 10000000;

                        if (ageSeconds > maxAgeSeconds) {
                            std::wcout << L"Killing: " << pe.szExeFile << L" (Age: " << ageSeconds << L"s)\n";
                            TerminateProcess(hProcess, 0);
                        }
                    }
                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
}

int main(int argc, char** argv) {
    std::map<std::string, std::string> params = parseArgs(argc, argv);
    if (params.size() != 2 || params.find("procmask") == params.end() || params.find("time") == params.end()){
        std::cout << "invalid args";
        return -1;
    }
    std::wstring procmask =  std::wstring(params["procmask"].begin(), params["procmask"].end());
    int t = 0;
    try {
        t = atoi(params["time"].c_str());
    }
    catch(std::exception const & e)
    {
        std::cout<< "error : " << e.what() << std::endl;
    }

    std::cout << "Kill all processes by mask " << params["procmask"] << " working more than " << t << std::endl;

    killProcessesByMaskAndTime(procmask, t);
    return 0;
}