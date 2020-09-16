#pragma once
#include <functional>

typedef struct
{
#if defined(_WIN32) || defined(_WIN64)
    const wchar_t* szDspName;
    const wchar_t* szDescrip;
#endif
    const wchar_t* szSrvName;
    std::function<void()> fnStartCallBack;
    std::function<void()> fnStopCallBack;
    std::function<void()> fnSignalCallBack;
}SrvParam;

int ServiceMain(int argc, const char* argv[], SrvParam SvPara);
