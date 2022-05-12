/* Copyright (C) 2016-2020 Thomas Hauck - All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

   The author would be happy if changes and
   improvements were reported back to him.

   Author:  Thomas Hauck
   Email:   Thomas@fam-hauck.de
*/

#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <signal.h>

#include "Service.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <VersionHelpers.h>
#include <conio.h>
#include "BaseSrv.h"
#include "SrvCtrl.h"
#include "Psapi.h"
#pragma comment(lib, "Psapi.lib")
#else
#include <fcntl.h>
#include <memory>
#include <thread>
#include <condition_variable>
#include <syslog.h>
#include <termios.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
class CBaseSrv
{
public:
    explicit CBaseSrv(const wchar_t*) {}
    virtual int Run(void) { Start(); return 0; }
    virtual void Start(void) = 0;
    virtual void Stop(void) = 0;
};
#endif

using namespace std;

class Service : public CBaseSrv
{
public:
    static Service& GetInstance(const SrvParam* SrvPara = nullptr)
    {
        if (s_pInstance == 0)
            s_pInstance = Service::factory(SrvPara);
        return *s_pInstance;
    }

    void Start(void) override
    {
        m_bIsStopped = false;

        if (fnStartCallBack != nullptr)
            fnStartCallBack();

        unique_lock<mutex> lock(m_mxStop);
        m_cvStop.wait(lock, [&]() { return m_bStop; });

        if (fnStopCallBack != nullptr)
            fnStopCallBack();

        m_bIsStopped = true;
    }

    void Stop(void) noexcept override
    {
        m_bStop = true;
        m_cvStop.notify_all();
    }

    void CallSignalCallback()
    {
        if (fnSignalCallBack != nullptr)
            fnSignalCallBack();
    }

    bool IsStopped(void) noexcept { return m_bIsStopped; }

    static void SignalHandler(int iSignal)
    {
        signal(iSignal, Service::SignalHandler);

        if (iSignal == SIGTERM)
            Service::GetInstance().Stop();
#if defined(_WIN32) || defined(_WIN64)
        else if (iSignal == SIGINT)
            Service::GetInstance().CallSignalCallback();
#else
        else if (iSignal == SIGHUP)
            Service::GetInstance().CallSignalCallback();
#endif
    }

    static unique_ptr<Service> factory(const SrvParam* SrvPara = nullptr)
    {
        struct EnableMaker : public Service
        {
            EnableMaker(const SrvParam* SrvPara = nullptr) : Service(SrvPara) {}
            using Service::Service;
        };
        return make_unique<EnableMaker>(SrvPara);
    }

private:
    explicit Service(const SrvParam* SrvPara) : CBaseSrv(SrvPara->szSrvName), m_bStop(false), m_bIsStopped(true),
        fnStartCallBack(SrvPara->fnStartCallBack), fnStopCallBack(SrvPara->fnStopCallBack), fnSignalCallBack(SrvPara->fnSignalCallBack) { }

private:
    static unique_ptr<Service> s_pInstance;
    bool m_bStop;
    bool m_bIsStopped;
    mutex              m_mxStop;
    condition_variable m_cvStop;
    function<void()> fnStartCallBack;
    function<void()> fnStopCallBack;
    function<void()> fnSignalCallBack;

};

unique_ptr<Service> Service::s_pInstance;


#if defined(_WIN32) || defined(_WIN64)
DWORD WINAPI RemoteThreadProc(LPVOID/* lpParameter*/) noexcept
{
    return raise(SIGINT);
}
#endif

int ServiceMain(int argc, const char* argv[], const SrvParam& SrvPara)
{
#if defined(_WIN32) || defined(_WIN64)
    signal(SIGINT, Service::SignalHandler);
#else
    signal(SIGHUP, Service::SignalHandler);
    signal(SIGTERM, Service::SignalHandler);

    auto _kbhit = []() -> int
    {
        struct termios oldt, newt;
        int ch;
        int oldf;

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);

        if (ch != EOF)
        {
            ungetc(ch, stdin);
            return 1;
        }

        return 0;
    };
#endif

    int iRet = 0;

    if (argc > 1)
    {
        while (++argv, --argc)
        {
            if (argv[0][0] == '-')
            {
                switch ((argv[0][1] & 0xdf))
                {
#if defined(_WIN32) || defined(_WIN64)
                case 'I':
                    iRet = CSvrCtrl().Install(SrvPara.szSrvName, SrvPara.szDspName, SrvPara.szDescrip);
                    break;
                case 'R':
                    iRet = CSvrCtrl().Remove(SrvPara.szSrvName);
                    break;
                case 'S':
                    iRet = CSvrCtrl().Start(SrvPara.szSrvName);
                    break;
                case 'E':
                    iRet = CSvrCtrl().Stop(SrvPara.szSrvName);
                    break;
                case 'P':
                    iRet = CSvrCtrl().Pause(SrvPara.szSrvName);
                    break;
                case 'C':
                    iRet = CSvrCtrl().Continue(SrvPara.szSrvName);
                    break;
#endif
                case 'F':
                {
                    wcout << SrvPara.szSrvName << L" started" << endl;

                    Service::GetInstance(&SrvPara);

                    thread th([&]() {
                        Service::GetInstance().Start();
                    });

                    const wchar_t caZeichen[] = L"\\|/-";
                    int iIndex = 0;
                    while (_kbhit() == 0)
                    {
                        wcout << L'\r' << caZeichen[iIndex++] << flush;
                        if (iIndex > 3) iIndex = 0;
                        this_thread::sleep_for(chrono::milliseconds(100));
                    }

                    wcout << SrvPara.szSrvName << L" stopped" << endl;
                    Service::GetInstance().Stop();
                    if (th.joinable() == true)
                        th.join();
                }
                break;
                case 'K':
                {
#if defined(_WIN32) || defined(_WIN64)
                    function <bool()> IsEleavate = []() -> bool
                    {
                        if (!IsWindowsVistaOrGreater())
                            return false;

                        // check if elevated on Vista and 7
                        HANDLE Token = nullptr;
                        TOKEN_ELEVATION Elevation = { 0 };  // Token type only available with Vista/7
                        DWORD ReturnSize = 0;

                        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &Token) ||
                            !GetTokenInformation(Token, TokenElevation, &Elevation, sizeof(Elevation), &ReturnSize))
                            return false;

                        if (Elevation.TokenIsElevated)  // process is elevated
                            return false;

                        wstring FileName(MAX_PATH, 0);
                        GetModuleFileName(nullptr, &FileName[0], MAX_PATH);
                        FileName.erase(FileName.find_first_of(L'\0'));
                        wstring pStrCmdLine = GetCommandLine();

                        size_t nPos = pStrCmdLine.find(FileName);
                        if (nPos != string::npos)
                        {
                            nPos += pStrCmdLine.substr(nPos + FileName.size()).find_first_of(L' ') + 1 + FileName.size();
                            pStrCmdLine.erase(0, nPos);
                        }

                        SHELLEXECUTEINFO Info;
                        Info.hwnd = nullptr;
                        Info.cbSize = sizeof(Info);
                        Info.fMask = 0;
                        Info.hwnd = nullptr;
                        Info.lpVerb = L"runas";
                        Info.lpFile = &FileName[0];
                        Info.lpParameters = &pStrCmdLine[0];
                        Info.lpDirectory = nullptr;
                        Info.nShow = 0;// SW_HIDE;
                        Info.hInstApp = nullptr;
                        while (!ShellExecuteEx(&Info));

                        return true;
                    };
                    if (IsEleavate() == true)
                        break;

                    function <bool()> AdjustTocken = []() -> bool
                    {
                        TOKEN_PRIVILEGES tp = { 0 };
                        LUID luid;

                        if (!LookupPrivilegeValue( nullptr,             // lookup privilege on local system
                                                   L"SeDebugPrivilege", // privilege to lookup
                                                   &luid))              // receives LUID of privilege
                        {
                            wcout << L"LookupPrivilegeValue error: " << GetLastError() << L"\r\n";
                            return false;
                        }

                        tp.PrivilegeCount = 1;
                        tp.Privileges[0].Luid = luid;
                        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

                        // Enable the privilege or disable all privileges.

                        HANDLE ProcessHandle = GetCurrentProcess();
                        HANDLE TokenHandle;
                        OpenProcessToken(ProcessHandle, TOKEN_ALL_ACCESS, &TokenHandle);
                        const BOOL bRes = AdjustTokenPrivileges(TokenHandle, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), static_cast<PTOKEN_PRIVILEGES>(nullptr), static_cast<PDWORD>(nullptr));
                        CloseHandle(TokenHandle);

                        if (!bRes)
                        {
                            wcout << L"AdjustTokenPrivileges error: " << GetLastError() << L"\r\n";
                            return false;
                        }

                        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
                        {
                            wcout << L"The token does not have the specified privilege.\r\n";
                            return false;
                        }
                        return true;
                    };
                    if (AdjustTocken() == false)
                        break;

                    wstring strPath(MAX_PATH, 0);
                    GetModuleFileName(nullptr, &strPath[0], MAX_PATH);
                    strPath.erase(strPath.find_first_of(L'\0'));
                    strPath.erase(0, strPath.find_last_of(L'\\') + 1);

                    if (strPath.empty() == false)
                    {
                        DWORD dwInitSize = 1024;
                        DWORD dwIdReturned = 0;
                        unique_ptr<DWORD[]> pBuffer = make_unique<DWORD[]>(dwInitSize);
                        while (dwInitSize < 16384 && EnumProcesses(pBuffer.get(), sizeof(DWORD) * dwInitSize, &dwIdReturned) != 0)
                        {
                            if (dwIdReturned == sizeof(DWORD) * dwInitSize) // Buffer to small
                            {
                                dwInitSize *= 2;
                                pBuffer = make_unique<DWORD[]>(dwInitSize);
                                continue;
                            }
                            dwIdReturned /= sizeof(DWORD);

                            for (DWORD n = 0; n < dwIdReturned; ++n)
                            {
                                HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pBuffer[n]);
                                if (hProcess != nullptr)
                                {
                                    wstring strEnumPath(MAX_PATH, 0);
                                    if (GetModuleBaseName(hProcess, nullptr, &strEnumPath[0], MAX_PATH) > 0)
                                        strEnumPath.erase(strEnumPath.find_first_of(L'\0'));
                                    else
                                    {
                                        strEnumPath.erase(GetProcessImageFileName(hProcess, &strEnumPath[0], MAX_PATH));
                                        strEnumPath.erase(0, strEnumPath.find_last_of('\\') + 1);
                                    }

                                    if (strEnumPath.empty() == false && strEnumPath == strPath) // Same Name
                                    {
                                        if (GetCurrentProcessId() != pBuffer[n])                // but other process
                                        {
                                            HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, RemoteThreadProc, nullptr, 0, nullptr);
                                            CloseHandle(hProcess);
                                            DWORD dwExitCode;
                                            while (GetExitCodeThread(hThread, &dwExitCode) && dwExitCode == STILL_ACTIVE)
                                                this_thread::sleep_for(chrono::milliseconds(10));
                                            CloseHandle(hThread);
                                            break;
                                        }
                                    }
                                    CloseHandle(hProcess);
                                }
                            }
                            break;
                        }
                    }
#else
                    pid_t nMyId = getpid();
                    string strMyName(64, 0);
                    FILE* fp = fopen("/proc/self/comm", "r");
                    if (fp)
                    {
                        if (fgets(&strMyName[0], strMyName.size(), fp) != NULL)
                        {
                            strMyName.erase(strMyName.find_last_not_of('\0') + 1);
                            strMyName.erase(strMyName.find_last_not_of('\n') + 1);
                        }
                        fclose(fp);
                    }

                    DIR* dir = opendir("/proc");
                    if (dir != nullptr)
                    {
                        struct dirent* ent;
                        char* endptr;

                        while ((ent = readdir(dir)) != NULL)
                        {
                            // if endptr is not a null character, the directory is not entirely numeric, so ignore it
                            long lpid = strtol(ent->d_name, &endptr, 10);
                            if (*endptr != '\0')
                                continue;

                            // if the number is our own pid we ignore it
                            if ((pid_t)lpid == nMyId)
                                continue;

                            // try to open the cmdline file
                            FILE* fp = fopen(string("/proc/" + to_string(lpid) + "/comm").c_str(), "r");
                            if (fp != nullptr)
                            {
                                string strName(64, 0);
                                if (fgets(&strName[0], strName.size(), fp) != NULL)
                                {
                                    strName.erase(strName.find_last_not_of('\0') + 1);
                                    strName.erase(strName.find_last_not_of('\n') + 1);
                                    if (strName == strMyName)
                                    {
                                        //wcout << strName.c_str() << L" = " << (pid_t)lpid << endl;
                                        kill((pid_t)lpid, SIGHUP);
                                        break;
                                    }
                                }
                                fclose(fp);
                            }
                        }
                        closedir(dir);
                    }
#endif
                }
                break;
                case 'H':
                case '?':
                    wcout << L"\r\n";
#if defined(_WIN32) || defined(_WIN64)
                    wcout << L"-i   Install the system service\r\n";
                    wcout << L"-r   Removes the system service\r\n";
                    wcout << L"-s   Starts the system service\r\n";
                    wcout << L"-e   Shuts down the system service\r\n";
                    wcout << L"-p   System service is paused (pause)\r\n";
                    wcout << L"-c   System service will continue (Continue)\r\n";
#endif
                    wcout << L"-f   Start the application as a console application\r\n";
                    wcout << L"-k   Reload configuration\r\n";
                    wcout << L"-h   Show this help\r\n";
                    return iRet;
                }
            }
        }
    }
    else
    {
        Service::GetInstance(&SrvPara);

#if !defined(_WIN32) && !defined(_WIN64)
        function<string(const wstring&)> fnWS2S = [](const wstring& src) -> string
        {
            string strDst(src.size() * 4, 0);
            size_t nWritten = wcstombs(&strDst[0], src.c_str(), src.size() * 4);
            strDst.resize(nWritten > 0 && nWritten < src.size() * 4 ? nWritten : 0);
            return strDst;
        };
        string strSrvName = fnWS2S(SrvPara.szSrvName);
        //Set our Logging Mask and open the Log
        setlogmask(LOG_UPTO(LOG_NOTICE));
        openlog(strSrvName.c_str(), LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

        syslog(LOG_NOTICE, "%s", string("Starting " + strSrvName).c_str());

        //Fork the Parent Process
        pid_t pid = fork();

        if (pid < 0)
            exit(EXIT_FAILURE);

        //We got a good pid, Close the Parent Process
        if (pid > 0)
            exit(EXIT_SUCCESS);

        //Create a new Signature Id for our child
        pid_t sid = setsid();
        if (sid < 0)
            exit(EXIT_FAILURE);

        //Fork second time the Process
        pid = fork();

        if (pid < 0)
            exit(EXIT_FAILURE);

        //We got a good pid, Close the Parent Process
        if (pid > 0)
            exit(EXIT_SUCCESS);

        //Change File Mask
        umask(0);

        //Close Standard File Descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);


#endif
        iRet = Service::GetInstance().Run();

        syslog(LOG_NOTICE, "%s", string(strSrvName + " gestoppt").c_str());
    }

    return iRet;
}
