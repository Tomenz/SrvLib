/* Copyright (C) 2016-2020 Thomas Hauck - All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

   The author would be happy if changes and
   improvements were reported back to him.

   Author:  Thomas Hauck
   Email:   Thomas@fam-hauck.de
*/

#ifndef SVRCTRL_H
#define SVRCTRL_H

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

class CSvrCtrl
{
public:
    CSvrCtrl(void) noexcept;
    ~CSvrCtrl(void);
    CSvrCtrl(const CSvrCtrl&) = delete;
    CSvrCtrl(CSvrCtrl&&) = delete;
    CSvrCtrl& operator=(const CSvrCtrl&) = delete;
    CSvrCtrl& operator=(CSvrCtrl&&) = delete;

    int Install(const wchar_t* szSvrName, const wchar_t* szDisplayName, const wchar_t* szDescription = nullptr);
    int Remove(const wchar_t* szSvrName);
    int Start(const wchar_t* szSvrName);
    int Stop(const wchar_t* szSvrName);
    int Pause(const wchar_t* szSvrName);
    int Continue(const wchar_t* szSvrName);

private:
    bool SelfElevat();
    bool SetServiceDescription(const wchar_t* szSvrName, const wchar_t* szDescription) noexcept;

private:
    SC_HANDLE m_hSCManager;
};
#endif

#endif // !SVRCTRL_H
