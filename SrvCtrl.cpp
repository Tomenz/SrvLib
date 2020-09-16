/* Copyright (C) 2016-2020 Thomas Hauck - All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

   The author would be happy if changes and
   improvements were reported back to him.

   Author:  Thomas Hauck
   Email:   Thomas@fam-hauck.de
*/

#include <string>
#include ".\srvctrl.h"
#include <VersionHelpers.h>

using namespace std;

CSvrCtrl::CSvrCtrl(void) : m_hSCManager(nullptr)
{
	m_hSCManager = OpenSCManager(
    NULL,                    // local machine
    NULL,                    // ServicesActive database
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

    if(GetModuleFileName(NULL, szPath, MAX_PATH) == 0)
    {
//        printf("GetModuleFileName failed (%d)\n", GetLastError());
        return -2;
    }

    SC_HANDLE schService = CreateService(
        m_hSCManager,              // SCManager database
        szSvrName,				   // name of service
        szDisplayName,             // service name to display
        SERVICE_ALL_ACCESS,        // desired access
        SERVICE_WIN32_OWN_PROCESS, // service type
        SERVICE_AUTO_START,        // start type
        SERVICE_ERROR_NORMAL,      // error control type
        szPath,                    // path to service's binary
        NULL,                      // no load ordering group
        NULL,                      // no tag identifier
        NULL,                      // no dependencies
        NULL,                      // LocalSystem account
        NULL);                     // no password

    if (schService == NULL)
    {
//        printf("CreateService failed (%d)\n", GetLastError());
        return -1;
    }
    else
    {
        CloseServiceHandle(schService);

        if (szDescription != nullptr)
            SetServiceDescription(szSvrName, const_cast<wchar_t*>(szDescription));

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

    if (schService == NULL)
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

	SC_HANDLE schService;
    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwOldCheckPoint;
    DWORD dwStartTickCount;
    DWORD dwBytesNeeded;

    schService = OpenService(m_hSCManager, szSvrName, SERVICE_ALL_ACCESS);

    if (schService == NULL)
        return -5;

    if (StartService(schService, 0, NULL) == 0)
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

    if (schService == NULL)
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

    if (schService == NULL)
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

    if (schService == NULL)
        return -5;

	DWORD dwError = 0;
	SERVICE_STATUS ssStatus;
	if (ControlService(schService, SERVICE_CONTROL_CONTINUE, &ssStatus) == 0)
		dwError = GetLastError();

    CloseServiceHandle(schService);
    return dwError;
}

bool CSvrCtrl::SetServiceDescription(const wchar_t* szSvrName, wchar_t* szDescription)
{
    // Need to acquire database lock before reconfiguring.
    SC_LOCK sclLock = LockServiceDatabase(m_hSCManager);

    // If the database cannot be locked, report the details.
    if (sclLock == NULL)
    {
        // Exit if the database is not locked by another process.

        if (GetLastError() != ERROR_SERVICE_DATABASE_LOCKED)
        {
            return false;
        }

        // Allocate a buffer to get details about the lock.

        LPQUERY_SERVICE_LOCK_STATUS lpqslsBuf = (LPQUERY_SERVICE_LOCK_STATUS) LocalAlloc(LPTR, sizeof(QUERY_SERVICE_LOCK_STATUS) + 256);
        if (lpqslsBuf == NULL)
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
	SC_HANDLE schService = OpenService(m_hSCManager, szSvrName, SERVICE_CHANGE_CONFIG);	// need CHANGE access
    if (schService == NULL)
    {
		// Release the database lock.
		UnlockServiceDatabase(sclLock);
        return false;
    }

    SERVICE_DESCRIPTION sdBuf;
    sdBuf.lpDescription = szDescription;

    if( !ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sdBuf))	// value: new description
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
}
