/* Copyright (C) 2016-2020 Thomas Hauck - All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT

   The author would be happy if changes and
   improvements were reported back to him.

   Author:  Thomas Hauck
   Email:   Thomas@fam-hauck.de
*/

#pragma once

#include "windows.h"
#include <string>

using namespace std;

class CBaseSrv
{
public:
    explicit CBaseSrv(const wchar_t* szSrvName);
	~CBaseSrv(void);
	int Run(void);

	static void WINAPI ServiceStartCB(DWORD argc, LPTSTR *argv);
	static void WINAPI ServiceCtrlHandler(DWORD Opcode);

	virtual int  Init(void)  { return 1;}
	virtual void Start(void) = 0;
	virtual void Stop(void)  = 0;
	virtual void Pause(void) {;}
	virtual void Continue(void) {;}

private:
	static basic_string<wchar_t> s_strSrvName;
	static SERVICE_STATUS        s_stSrvStatus;
	static SERVICE_STATUS_HANDLE s_hSrvStatus;
	static CBaseSrv*             s_This;
};
