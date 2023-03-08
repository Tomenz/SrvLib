
#ifndef FSNOTIFY_H
#define FSNOTIFY_H

#include <string>
#include <map>
#include <thread>
#include <functional>

#include <sys/inotify.h>
#include <poll.h>

class FileSysNotify
{

public:
    static FileSysNotify& GetInstance() {
        static FileSysNotify s_ThisClass;
        return s_ThisClass;
    }

    void SetCallBackFunction(std::function<void(std::string)> fnCallBack)
    {
        m_fnCallBack = fnCallBack;
    }

    int AddWatch(const std::string& strPath)
    {
        int iWatch = inotify_add_watch(m_fd, strPath.c_str(), IN_CREATE | IN_DELETE /*IN_OPEN | IN_CLOSE*/);
        if (iWatch == -1)
            return iWatch;
        m_mapWatches.emplace(iWatch, strPath);
        return iWatch;
    }

    int StopAllWatch()
    {
        int iRet = 0;
        for (auto& it : m_mapWatches)
        {
             iRet |= inotify_rm_watch(m_fd, it.first);
        }
        return iRet;
    }

    void PollThread()
    {
        nfds_t nfds{1};
        struct pollfd fds[1];

        fds[0].fd = m_fd;                 /* Inotify input */
        fds[0].events = POLLIN;

        while (!m_bStop) {
            int poll_num = poll(fds, nfds, 100);
            if (poll_num == 0)
                continue;
            if (poll_num == -1) {
                if (errno == EINTR)
                    continue;
                perror("poll");
                exit(EXIT_FAILURE);
            }

            if (poll_num > 0) {
                if (fds[0].revents & POLLIN) {

                    /* Inotify events are available. */

                    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
                    const struct inotify_event *event;
                    ssize_t len;

                    /* Loop while events can be read from inotify file descriptor. */

                    for (;;) {

                        /* Read some events. */

                        len = read(m_fd, buf, sizeof(buf));
                        if (len == -1 && errno != EAGAIN) {
                            perror("read");
                            exit(EXIT_FAILURE);
                        }

                        /* If the nonblocking read() found no events to read, then
                            it returns -1 with errno set to EAGAIN. In that case,
                            we exit the loop. */

                        if (len <= 0)
                            break;

                        /* Loop over all events in the buffer. */
                        for (char *ptr = buf; ptr < buf + len;
                                ptr += sizeof(struct inotify_event) + event->len) {

                            event = (const struct inotify_event *) ptr;

                            /* Print event type. */
                            std::string strOut;

                            if (event->mask & IN_OPEN)
                                 strOut = "IN_OPEN: ";
                            if (event->mask & IN_CLOSE_NOWRITE)
                                strOut = "IN_CLOSE_NOWRITE: ";
                            if (event->mask & IN_CLOSE_WRITE)
                                strOut = "IN_CLOSE_WRITE: ";
                            if (event->mask & IN_CREATE)
                                strOut = "IN_CREATE: ";
                            if (event->mask & IN_DELETE)
                                strOut = "IN_DELETE: ";

                            /* Print the name of the watched directory. */

                            for (auto& it : m_mapWatches)
                            {
                                if (it.first == event->wd) {
                                    strOut += it.second + " ";
                                    break;
                                }
                            }

                            /* Print the name of the file. */

                            if (event->len)
                                strOut += event->name;

                            /* Print type of filesystem object. */

                            if (event->mask & IN_ISDIR)
                                strOut += " [directory]";
                            else
                                strOut += " [file]";
                            m_fnCallBack(strOut);
                        }
                    }




                }
            }
        }
    }

private:
    FileSysNotify() : m_bStop(false)
    {
        m_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        m_thPoll = std::thread(std::bind(&FileSysNotify::PollThread, this));
    }
    ~FileSysNotify()
    {
        StopAllWatch();
        m_bStop = true;
        m_thPoll.join();

        if (m_fd != -1)
            close(m_fd);
    }

    FileSysNotify(const FileSysNotify&) = delete;

private:
    int m_fd;
    std::map<int, std::string> m_mapWatches;
    bool m_bStop;
    std::thread m_thPoll;
    std::function<void(std::string)> m_fnCallBack;
};


#endif  // FSNOTIFY_H
