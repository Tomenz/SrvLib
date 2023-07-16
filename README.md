# SrvLib

[![Build Status](https://travis-ci.com/Tomenz/SrvLib.svg?branch=master)](https://travis-ci.com/Tomenz/SrvLib)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/73915c0f4be040198b0ebdd0466f34b9)](https://app.codacy.com/gh/Tomenz/SrvLib?utm_source=github.com&utm_medium=referral&utm_content=Tomenz/SrvLib&utm_campaign=Badge_Grade)
[![Build status](https://ci.appveyor.com/api/projects/status/871d8ynr5qwu589h?svg=true)](https://ci.appveyor.com/project/Tomenz/srvlib)
[![CodeQL](https://github.com/Tomenz/SrvLib/actions/workflows/codeql.yml/badge.svg)](https://github.com/Tomenz/SrvLib/actions/workflows/codeql.yml)
[![CMake](https://github.com/Tomenz/SrvLib/actions/workflows/cmake.yml/badge.svg)](https://github.com/Tomenz/SrvLib/actions/workflows/cmake.yml)

A plugin library to make your console application a windows system service, or a linux daemon service

# Windows
On Windows there are commandline option to install,remove,start and stop the service.

    -i   Install the system service
    -r   Removes the system service
    -s   Starts the system service
    -e   Shuts down the system service
    -p   System service is paused (pause)
    -c   System service will continue (Continue)
    -f   Start the application as a console application
    -k   Reload configuration
    -h   Show this help

# Linux - systemd

    Rename an copy the example.service file after editing to /etc/systemd/system/
    Reload the systemd as root with: systemctl daemon-reload
    
    IN THE NEXT COMMANDS USE THE NAME OF THE RENAMED "example" FILE!
    
    Enable the auto start run this command as root: systemctl enable example.service
    To start the service, run as root: systemctl start example.service
    To stop the service, run as root: systemctl stop example.service
    To reload the what ever, run as root: systemctl reload example.service

# Linux - init.d
Copy the file in the init.d directory to /etc/init.d/ and rename it, modify the execution rights, and change the application name and the path for the application in that file.

commanline option are:
/etc/init.d/filename start|stop|status|restart|reload|install|remove

# Example
```
#include "Service.h"

int main(int argc, const char* argv[])
{
    SrvParam svParam;
#if defined(_WIN32) || defined(_WIN64)
    svParam.szDspName = L"Example Service";                 // Servicename in Service control manager of windows
    svParam.szDescrip = L"Example Service by your name";    // Description in Service control manager of windows
#endif
    svParam.szSrvName = L"ExampleServ";                     // Service name (service id)
    svParam.fnStartCallBack = []()
    {   // Start you server here
    };
    svParam.fnStopCallBack = []() 
    {   // Stop you server here
    };
    svParam.fnSignalCallBack = []()
    {   // what ever you do with this callback, maybe reload the configuration
    };

    return ServiceMain(argc, argv, svParam);
}
```
