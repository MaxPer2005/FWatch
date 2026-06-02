#include "net.h"

#include <errno.h>

#ifndef _WIN32
  #include <signal.h>
  #include <time.h>
#endif

int net_startup(void) {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    // Обрыв соединения при send() иначе шлёт SIGPIPE и убивает процесс.
    signal(SIGPIPE, SIG_IGN);
    return 0;
#endif
}

void net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

void net_configure_socket(socket_t s) {
    int yes = 1;
    // Отключаем алгоритм Нейгла: один байт уходит немедленно.
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(yes));
    // Периодические keepalive-пробы выявляют молча умершего пира.
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (const char *)&yes, sizeof(yes));
#ifdef SO_NOSIGPIPE
    // macOS: запрещаем SIGPIPE на уровне сокета (дополнительно к игнору сигнала).
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&yes, sizeof(yes));
#endif
}

int net_send_all(socket_t s, const void *buf, int len) {
    const char *p = (const char *)buf;
    int left = len;
    while (left > 0) {
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL; // Linux: не генерировать SIGPIPE при отправке.
#endif
        int n = (int)send(s, p, left, flags);
        if (n <= 0) {
#ifndef _WIN32
            if (n < 0 && errno == EINTR) {
                continue;
            }
#endif
            return -1;
        }
        p += n;
        left -= n;
    }
    return 0;
}

void net_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}
