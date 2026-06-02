#ifndef NET_H
#define NET_H

// Кроссплатформенная обёртка над сокетами: скрывает различия Winsock и POSIX.

// На glibc строгий -std=c23 (__STRICT_ANSI__) прячет POSIX-расширения
// (getaddrinfo, struct addrinfo, fd_set, select, nanosleep). Включаем их явно.
// Должно быть определено до подключения любых системных заголовков.
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
  #define _GNU_SOURCE
#endif

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET socket_t;
  #define INVALID_SOCK INVALID_SOCKET
  #define close_socket(s) closesocket(s)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  typedef int socket_t;
  #define INVALID_SOCK (-1)
  #define close_socket(s) close(s)
#endif

// Инициализация сетевой подсистемы (WSAStartup на Windows; игнор SIGPIPE на
// POSIX, чтобы обрыв пира не убивал процесс сигналом). Возвращает 0 при успехе.
int net_startup(void);

// Завершение работы сетевой подсистемы.
void net_cleanup(void);

// Настраивает соединённый сокет: TCP_NODELAY (минимальная задержка на нажатие),
// SO_KEEPALIVE (детект мёртвых соединений), SO_NOSIGPIPE на macOS.
void net_configure_socket(socket_t s);

// Полностью отправляет len байт (учитывает частичную запись и EINTR).
// Возвращает 0 при успехе, -1 при ошибке. Не генерирует SIGPIPE.
int net_send_all(socket_t s, const void *buf, int len);

// Блокирующая пауза на указанное число миллисекунд (для backoff).
void net_sleep_ms(int ms);

#endif // NET_H
