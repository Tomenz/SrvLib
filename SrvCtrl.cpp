/* Copyright (C) 2016-2020 Thomas Hauck - All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

   The author would be happy if changes and
   improvements were reported back to him.

   Author:  Thomas Hauck
   Email:   Thomas@fam-hauck.de
*/

#include <string>
#include "SrvCtrl.h"
#include <VersionHelpers.h>

using namespace std;

CSvrCtrl::CSvrCtrl(void) noexcept : m_hSCManager(nullptr)
{
    m_hSCManager = OpenSCManager(
    nullptr,                 // local machine
    nullptr,                 // ServicesActive database
    SC_MANAGER_ALL_ACCESS);  // full access rights
}

CSvrCtrl::~CSvrCtrl(void)
{
    if (m_hSCManager != nullptr)
        CloseServiceHandle(m_hSCManager);
}

int CSvrCtrl::Install(const wchar_t* szSvrName, const wchar_t* szDisplayName, const wchar_t* szDescription/* = nullptr*/)
{
    if (SelfElevat() == true)
        return 0;

    wchar_t szPath[MAX_PATH];

    if(GetModuleFileName(nullptr, &szPath[0], MAX_PATH) == 0)
    {
//        printf("GetModuleFileName failed (%d)\n", GetLastError());
        return -2;
    }

    SC_HANDLE schService = CreateService(
        m_hSCManager,              // SCManager database
        szSvrName,                 // name of service
        szDisplayName,             // service name to display
        SERVICE_ALL_ACCESS,        // desired access
        SERVICE_WIN32_OWN_PROCESS, // service type
        SERVICE_AUTO_START,        // start type
        SERVICE_ERROR_NORMAL,      // error control type
        &szPath[0],                // path to service's binary
        nullptr,                   // no load ordering group
        nullptr,                   // no tag identifier
        nullptr,                   // no dependencies
        nullptr,                   // LocalSystem account
        nullptr);                  // no password

    if (schService == nullptr)
    {
//        printf("CreateService failed (%d)\n", GetLastError());
        return -1;
    }
    else
    {
        CloseServiceHandle(schService);

        if (szDescription != nullptr)
            SetServiceDescription(szSvrName, szDescription);

        return 0;
    }
}

int CSvrCtrl::Remove(const wchar_t* szSvrName)
{
    if (SelfElevat() == true)
        return 0;

    SC_HANDLE schService = OpenService(
        m_hSCManager,       // SCManager database
        szSvrName,          // name of service
        DELETE);            // only need DELETE access

    if (schService == nullptr)
    {
//        printf("OpenService failed (%d)\n", GetLastError());
        return -3;
    }

    if (DeleteService(schService) == 0)
    {
//        printf("DeleteService failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        return -4;
    }

    CloseServiceHandle(schService);
    return 0;

}

int CSvrCtrl::Start(const wchar_t* szSvrName)
{
    if (SelfElevat() == true)
        return 0;

    SC_HANDLE schService = nullptr;
    SERVICE_STATUS_PROCESS ssStatus = { 0 };
    DWORD dwOldCheckPoint = 0;
    DWORD dwStartTickCount = 0;
    DWORD dwBytesNeeded = 0;

    schService = OpenService(m_hSCManager, szSvrName, SERVICE_ALL_ACCESS);

    if (schService == nullptr)
        return -5;

    if (StartService(schService, 0, nullptr) == 0)
    {
        CloseServiceHandle(schService);
        return 6;
    }

    // Check the status until the service is no longer start pending.

    if (!QueryServiceStatusEx(
            schService,             // handle to service
            SC_STATUS_PROCESS_INFO, // info level
            (LPBYTE)&ssStatus,              // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded ) )              // if buffer too small
    {
        CloseServiceHandle(schService);
        return 0;
    }

    // Save the tick count and initial checkpoint.

    dwStartTickCount = GetTickCount();
    dwOldCheckPoint = ssStatus.dwCheckPoint;

    while (ssStatus.dwCurrentState == SERVICE_START_PENDING)
    {
        // Do not wait longer than the wait hint. A good interval is
        // one tenth the wait hint, but no less than 1 second and no
        // more than 10 seconds.

        DWORD dwWaitTime = ssStatus.dwWaitHint / 10;

        if( dwWaitTime < 1000 )
            dwWaitTime = 1000;
        else if ( dwWaitTime > 10000 )
            dwWaitTime = 10000;

        Sleep( dwWaitTime );

        // Check the status again.

        if (!QueryServiceStatusEx(
            schService,             // handle to service
            SC_STATUS_PROCESS_INFO, // info level
            (LPBYTE)&ssStatus,              // address of structure
            sizeof(SERVICE_STATUS_PROCESS), // size of structure
            &dwBytesNeeded ) )              // if buffer too small
            break;

        if ( ssStatus.dwCheckPoint > dwOldCheckPoint )
        {
            // The service is making progress.

            dwStartTickCount = GetTickCount();
            dwOldCheckPoint = ssStatus.dwCheckPoint;
        }
        else
        {
            if(GetTickCount()-dwStartTickCount > ssStatus.dwWaitHint)
            {
                // No progress made within the wait hint
                break;
            }
        }
    }

    CloseServiceHandle(schService);

    if (ssStatus.dwCurrentState == SERVICE_RUNNING)
        return 1;

    return 0;
}

int CSvrCtrl::Stop(const wchar_t* szSvrName)
{
    if (SelfElevat() == true)
        return 0;

    SC_HANDLE schService = OpenService(m_hSCManager, szSvrName, SERVICE_ALL_ACCESS);

    if (schService == nullptr)
        return -5;

    DWORD dwError = 0;
    SERVICE_STATUS ssStatus;
    if (ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus) == 0)
        dwError = GetLastError();

    CloseServiceHandle(schService);
    return dwError;
}

int CSvrCtrl::Pause(const wchar_t* szSvrName)
{
    if (SelfElevat() == true)
        return 0;

    SC_HANDLE schService = OpenService(m_hSCManager, szSvrName, SERVICE_ALL_ACCESS);

    if (schService == nullptr)
        return -5;

    DWORD dwError = 0;
    SERVICE_STATUS ssStatus;
    if (ControlService(schService, SERVICE_CONTROL_PAUSE, &ssStatus) == 0)
        dwError = GetLastError();

    CloseServiceHandle(schService);
    return dwError;
}

int CSvrCtrl::Continue(const wchar_t* szSvrName)
{
    if (SelfElevat() == true)
        return 0;

    SC_HANDLE schService = OpenService(m_hSCManager, szSvrName, SERVICE_ALL_ACCESS);

    if (schService == nullptr)
        return -5;

    DWORD dwError = 0;
    SERVICE_STATUS ssStatus;
    if (ControlService(schService, SERVICE_CONTROL_CONTINUE, &ssStatus) == 0)
        dwError = GetLastError();

    CloseServiceHandle(schService);
    return dwError;
}

bool CSvrCtrl::SetServiceDescription(const wchar_t* szSvrName, const wchar_t* szDescription) noexcept
{
    // Need to acquire database lock before reconfiguring.
    SC_LOCK sclLock = LockServiceDatabase(m_hSCManager);

    // If the database cannot be locked, report the details.
    if (sclLock == nullptr)
    {
        // Exit if the database is not locked by another process.

        if (GetLastError() != ERROR_SERVICE_DATABASE_LOCKED)
        {
            return false;
        }

        // Allocate a buffer to get details about the lock.

        LPQUERY_SERVICE_LOCK_STATUS lpqslsBuf = static_cast<LPQUERY_SERVICE_LOCK_STATUS>(LocalAlloc(LPTR, sizeof(QUERY_SERVICE_LOCK_STATUS) + 256));
        if (lpqslsBuf == nullptr)
        {
            return false;
        }

        // Get and print the lock status information.
        DWORD dwBytesNeeded;
        if (!QueryServiceLockStatus(m_hSCManager, lpqslsBuf, sizeof(QUERY_SERVICE_LOCK_STATUS) + 256, &dwBytesNeeded))
        {
            return FALSE;
        }

        LocalFree(lpqslsBuf);
    }

    // The database is locked, so it is safe to make changes.
    bool bRet = true;

    // Open a handle to the service.
    SC_HANDLE schService = OpenService(m_hSCManager, szSvrName, SERVICE_CHANGE_CONFIG); // need CHANGE access
    if (schService == nullptr)
    {
        // Release the database lock.
        UnlockServiceDatabase(sclLock);
        return false;
    }

    SERVICE_DESCRIPTION sdBuf;
    sdBuf.lpDescription = const_cast<wchar_t*>(szDescription);

    if( !ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sdBuf))  // value: new description
    {
        bRet = false;
    }

    // Release the database lock.
    UnlockServiceDatabase(sclLock);

    // Close the handle to the service.
    CloseServiceHandle(schService);

    return bRet;
}

bool CSvrCtrl::SelfElevat()
{
    // check Windows version
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
}
