/* Copyright (C) 2016-2020 Thomas Hauck - All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

   The author would be happy if changes and
   improvements were reported back to him.

   Author:  Thomas Hauck
   Email:   Thomas@fam-hauck.de
*/

#include <iostream>
#include <mutex>
#include <signal.h>
#include <fcntl.h>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <VersionHelpers.h>
#include <conio.h>
#include <io.h>
#else
#include <memory>
#include <thread>
#include <condition_variable>
#include <syslog.h>
#include <termios.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include "BaseSrv.h"
#include "Srvctrl.h"
#include "Psapi.h"
#pragma comment(lib, "Psapi.lib")
#else
class CBaseSrv
{
public:
    explicit CBaseSrv(const wchar_t*) {}
    virtual int Run(void) { Start(); return 0; }
    virtual void Start(void) = 0;
};
#endif

#include "Service.h"

using namespace std;

class Service : public CBaseSrv
{
public:
    static Service& GetInstance(const SrvParam* SrvPara = nullptr)
    {
        if (s_pInstance == 0)
            s_pInstance.reset(new Service(SrvPara));
        return *s_pInstance.get();
    }

    virtual void Start(void)
    {
        m_bIsStopped = false;

        if (fnStartCallBack != nullptr)
            fnStartCallBack();

        unique_lock<mutex> lock(m_mxStop);
        m_cvStop.wait(lock, [&]() { return m_bStop; });

        if (fnStopCallBack != nullptr)
            fnStopCallBack();

        m_bIsStopped = true;
    };

    virtual void Stop(void)
    {
        m_bStop = true;
        m_cvStop.notify_all();
    }

    void CallSignalCallback()
    {
        if (fnSignalCallBack != nullptr)
            fnSignalCallBack();
    }

    bool IsStopped(void) { return m_bIsStopped; }

    static void SignalHandler(int iSignal)
    {
        signal(iSignal, Service::SignalHandler);

        if (iSignal == SIGTERM)
            Service::GetInstance().Stop();
        else if (iSignal == SIGINT)
            Service::GetInstance().CallSignalCallback();

#if defined(_WIN32) || defined(_WIN64)
        OutputDebugString(L"STRG+C-Signal empfangen\r\n");
#endif
    }

private:
    explicit Service(const SrvParam* SrvPara) : CBaseSrv(SrvPara->szSrvName), m_bStop(false), m_bIsStopped(true), 
        fnStartCallBack(SrvPara->fnStartCallBack), fnStopCallBack(SrvPara->fnStopCallBack), fnSignalCallBack(SrvPara->fnSignalCallBack) { }

private:
    static shared_ptr<Service> s_pInstance;
    bool m_bStop;
    bool m_bIsStopped;
    mutex              m_mxStop;
    condition_variable m_cvStop;
    function<void()> fnStartCallBack;
    function<void()> fnStopCallBack;
    function<void()> fnSignalCallBack;

};

shared_ptr<Service> Service::s_pInstance = nullptr;


#if defined(_WIN32) || defined(_WIN64)
DWORD WINAPI RemoteThreadProc(LPVOID/* lpParameter*/)
{
    return raise(SIGINT);
}
#endif

int ServiceMain(int argc, const char* argv[], SrvParam SrvPara)
{
#if defined(_WIN32) || defined(_WIN64)
    // Detect Memory Leaks
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
    _setmode(_fileno(stdout), _O_U16TEXT);

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
                    //CSvrCtrl().SetServiceDescription(szSrvName, szDescrip);
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
                        wcout << L'\r' << caZeichen[iIndex++] /*<< L"  Sockets:" << setw(3) << BaseSocket::s_atRefCount << L"  SSL-Pumpen:" << setw(3) << SslTcpSocket::s_atAnzahlPumps << L"  HTTP-Connections:" << setw(3) << nHttpCon*/ << flush;
                        if (iIndex > 3) iIndex = 0;
                        this_thread::sleep_for(chrono::milliseconds(100));
                    }

                    wcout << SrvPara.szSrvName << L" stopped" << endl;
                    Service::GetInstance().Stop();
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
                        HANDLE Token;
                        TOKEN_ELEVATION Elevation;      // Token type only available with Vista/7
                        DWORD ReturnSize;

                        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ, &Token) ||
                            !GetTokenInformation(Token, TokenElevation, &Elevation, sizeof(Elevation), &ReturnSize))
                            return false;

                        if (Elevation.TokenIsElevated)  // process is elevated
                            return false;

                        wstring FileName(MAX_PATH, 0);
                        GetModuleFileName(NULL, &FileName[0], MAX_PATH);
                        FileName.erase(FileName.find_first_of(L'\0'));
                        wstring pStrCmdLine = GetCommandLine();

                        size_t nPos = pStrCmdLine.find(FileName);
                        if (nPos != string::npos)
                        {
                            nPos += pStrCmdLine.substr(nPos + FileName.size()).find_first_of(L' ') + 1 + FileName.size();
                            pStrCmdLine.erase(0, nPos);
                        }

                        SHELLEXECUTEINFO Info;
                        Info.hwnd = NULL;
                        Info.cbSize = sizeof(Info);
                        Info.fMask = NULL;
                        Info.hwnd = NULL;
                        Info.lpVerb = L"runas";
                        Info.lpFile = &FileName[0];
                        Info.lpParameters = &pStrCmdLine[0];
                        Info.lpDirectory = NULL;
                        Info.nShow = 0;// SW_HIDE;
                        Info.hInstApp = NULL;
                        while (!ShellExecuteEx(&Info));

                        return true;
                    };
                    if (IsEleavate() == true)
                        break;

                    function <bool()> AdjustTocken = []() -> bool
                    {
                        TOKEN_PRIVILEGES tp;
                        LUID luid;

                        if (!LookupPrivilegeValue( NULL,            // lookup privilege on local system
                                                   L"SeDebugPrivilege",   // privilege to lookup 
                                                   &luid))        // receives LUID of privilege
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
                        BOOL bRes = AdjustTokenPrivileges(TokenHandle, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES)nullptr, (PDWORD)nullptr);
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
                    GetModuleFileName(NULL, &strPath[0], MAX_PATH);
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
                                HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pBuffer.get()[n]);
                                if (hProcess != NULL)
                                {
                                    wstring strEnumPath(MAX_PATH, 0);
                                    if (GetModuleBaseName(hProcess, NULL, &strEnumPath[0], MAX_PATH) > 0)
                                        strEnumPath.erase(strEnumPath.find_first_of(L'\0'));
                                    else
                                    {
                                        strEnumPath.erase(GetProcessImageFileName(hProcess, &strEnumPath[0], MAX_PATH));
                                        strEnumPath.erase(0, strEnumPath.find_last_of('\\') + 1);
                                    }

                                    if (strEnumPath.empty() == false && strEnumPath == strPath) // Same Name
                                    {
                                        if (GetCurrentProcessId() != pBuffer.get()[n])          // but other process
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
                            //wcout << "Meine PID = " << nMyId << " = " << strMyName.c_str() << endl;
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
        //Set our Logging Mask and open the Log
        setlogmask(LOG_UPTO(LOG_NOTICE));
        openlog("websockserv", LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

        syslog(LOG_NOTICE, "Starting WebSockServ");
        pid_t pid, sid;
        //Fork the Parent Process
        pid = fork();

        if (pid < 0)
            exit(EXIT_FAILURE);

        //We got a good pid, Close the Parent Process
        if (pid > 0)
            exit(EXIT_SUCCESS);

        //Create a new Signature Id for our child
        sid = setsid();
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

        thread([&]()
        {
            while (Service::GetInstance().IsStopped() == false)
                this_thread::sleep_for(chrono::milliseconds(100));
        }).detach();

#endif
        iRet = Service::GetInstance().Run();
    }

    return iRet;
}

