# SrvLib
A plugin library to make your console application a windows system service, or a linux daemon service

# Windows
On Windows the are commandline option to install,remove,start and stop the service.

    -i   Install the system service
    -r   Removes the system service
    -s   Starts the system service
    -e   Shuts down the system service
    -p   System service is paused (pause)
    -c   System service will continue (Continue)
    -f   Start the application as a console application
    -k   Reload configuration
    -h   Show this help

# Linux
On linux you have to copy the file in the init.d directory to /etc/init.d/ and rename it, modify the execution rights, and change the application name and the path for the application in that file.

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
    }
    svParam.fnStopCallBack = []() 
    {   // Stop you server here
    };
    svParam.fnSignalCallBack = []()
    {   // what ever you do with this callback, maybe reload the configuration
    };

    return ServiceMain(argc, argv, svParam);
}
```
