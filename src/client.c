#include "client.h"
#include "input.h"
#include "net.h"
#include "relay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <pthread.h>
#endif

// Байты протокола.
#define MSG_SPACE     0x01  // нажат пробел
#define MSG_HEARTBEAT 0x00  // пульс: держит NAT-маршрут живым, принимающий игнорит

// Интервал тишины, после которого шлём пульс (мс).
#define HEARTBEAT_MS 20000

// Сокет связи с relay (меняется при переподключении) — под защитой g_lock,
// т.к. читается из main thread (отправка) и пишется из сетевого потока.
static socket_t g_sock = INVALID_SOCK;
static char g_server_ip[256];
static char g_port[32];

// Кроссплатформенный мьютекс вокруг g_sock.
#ifdef _WIN32
static CRITICAL_SECTION g_lock;
  #define LOCK()   EnterCriticalSection(&g_lock)
  #define UNLOCK() LeaveCriticalSection(&g_lock)
  #define LOCK_INIT() InitializeCriticalSection(&g_lock)
#else
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
  #define LOCK()   pthread_mutex_lock(&g_lock)
  #define UNLOCK() pthread_mutex_unlock(&g_lock)
  #define LOCK_INIT() ((void)0)
#endif

// ---- callback платформенного слоя (main thread, только реальные нажатия) ----
// Антицикл обеспечивает сам платформенный слой (тегирование эмулированных
// событий), поэтому сюда попадают лишь настоящие нажатия пользователя.
static void on_local_space(void) {
    unsigned char b = MSG_SPACE;
    LOCK();
    socket_t s = g_sock;
    int ok = (s != INVALID_SOCK) && (net_send_all(s, &b, 1) == 0);
    UNLOCK();
    if (s == INVALID_SOCK) {
        printf(">> пробел не отправлен: нет связи (идёт переподключение)\n");
    } else if (ok) {
        printf(">> отправил пробел\n");
    } else {
        fprintf(stderr, ">> ошибка отправки (идёт переподключение)\n");
    }
    fflush(stdout);
}

// Тестовый режим: локальное нажатие -> сразу эмулировать обратно.
static void on_test_space(void) {
    printf("перехватил пробел, эмулирую обратно\n");
    fflush(stdout);
    input_simulate_space();
}

// Подключается к relay. Возвращает сокет или INVALID_SOCK.
static socket_t connect_to_server(const char *server_ip, const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server_ip, port, &hints, &res) != 0 || res == NULL) {
        return INVALID_SOCK;
    }
    socket_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCK) {
        freeaddrinfo(res);
        return INVALID_SOCK;
    }
    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        close_socket(s);
        freeaddrinfo(res);
        return INVALID_SOCK;
    }
    freeaddrinfo(res);
    return s;
}

// Сетевой поток: держит соединение, переподключается с backoff, принимает
// байты и эмулирует пробел. Работает вечно.
static void net_recv_loop(void) {
    static const int backoffs[] = {1, 2, 3, 5, 10};
    const int nb = (int)(sizeof(backoffs) / sizeof(backoffs[0]));
    int idx = 0;

    for (;;) {
        socket_t s = connect_to_server(g_server_ip, g_port);
        if (s == INVALID_SOCK) {
            int wait = backoffs[idx < nb ? idx : nb - 1];
            if (idx < nb) {
                idx++;
            }
            fprintf(stderr, "не удалось подключиться к %s:%s, повтор через %d c...\n",
                    g_server_ip, g_port, wait);
            net_sleep_ms(wait * 1000);
            continue;
        }

        net_configure_socket(s);
        net_set_recv_timeout(s, HEARTBEAT_MS);
        LOCK();
        g_sock = s;
        UNLOCK();
        idx = 0;
        printf("подключился к %s:%s\n", g_server_ip, g_port);
        printf("готов: нажми пробел\n");
        fflush(stdout);

        for (;;) {
            unsigned char buf[256];
            int n = (int)recv(s, (char *)buf, sizeof(buf), 0);
            if (n < 0) {
#ifndef _WIN32
                if (errno == EINTR) {
                    continue;
                }
#endif
                if (net_last_recv_was_timeout()) {
                    // Тишина: шлём пульс. Если отправка не прошла — соединение
                    // мёртво, идём на переподключение.
                    unsigned char hb = MSG_HEARTBEAT;
                    LOCK();
                    int ok = (net_send_all(s, &hb, 1) == 0);
                    UNLOCK();
                    if (ok) {
                        continue;
                    }
                }
                n = 0; // реальная ошибка
            }
            if (n == 0) {
                fprintf(stderr, "<< соединение с сервером потеряно, переподключение...\n");
                LOCK();
                g_sock = INVALID_SOCK;
                UNLOCK();
                close_socket(s);
                break;
            }
            for (int i = 0; i < n; i++) {
                if (buf[i] == MSG_SPACE) {
                    printf("<< получил пробел\n");
                    fflush(stdout);
                    input_simulate_space();
                }
                // MSG_HEARTBEAT и прочее игнорируем.
            }
        }
    }
}

#ifdef _WIN32
static DWORD WINAPI net_thread_main(LPVOID arg) {
    (void)arg;
    net_recv_loop();
    return 0;
}
static int start_net_thread(void) {
    HANDLE h = CreateThread(NULL, 0, net_thread_main, NULL, 0, NULL);
    if (h == NULL) {
        return -1;
    }
    CloseHandle(h);
    return 0;
}
#else
static void *net_thread_main(void *arg) {
    (void)arg;
    net_recv_loop();
    return NULL;
}
static int start_net_thread(void) {
    pthread_t tid;
    if (pthread_create(&tid, NULL, net_thread_main, NULL) != 0) {
        return -1;
    }
    pthread_detach(tid);
    return 0;
}
#endif

int client_run(const char *server_ip, const char *port) {
    if (net_startup() != 0) {
        fprintf(stderr, "не удалось инициализировать сеть\n");
        return 1;
    }
    LOCK_INIT();
    snprintf(g_server_ip, sizeof(g_server_ip), "%s", server_ip);
    snprintf(g_port, sizeof(g_port), "%s", port);

    if (input_init(on_local_space) != 0) {
        fprintf(stderr, "не удалось инициализировать перехват клавиатуры\n");
        net_cleanup();
        return 1;
    }
    if (start_net_thread() != 0) {
        fprintf(stderr, "не удалось запустить сетевой поток\n");
        net_cleanup();
        return 1;
    }

    printf("клиент запущен, подключаюсь к %s:%s ...\n", server_ip, port);
    fflush(stdout);
    input_run_loop(); // блокирует main thread

    net_cleanup();
    return 0;
}

int client_run_test(void) {
    if (input_init(on_test_space) != 0) {
        fprintf(stderr, "не удалось инициализировать перехват клавиатуры\n");
        return 1;
    }
    printf("тестовый режим: нажми пробел, он эмулируется обратно\n");
    printf("(Ctrl+C для выхода)\n");
    fflush(stdout);
    input_run_loop();
    return 0;
}

// Фоновый поток с relay-сервером для режима "хост".
static char g_host_port[32];

#ifdef _WIN32
static DWORD WINAPI relay_thread_main(LPVOID arg) {
    (void)arg;
    relay_run(g_host_port);
    return 0;
}
#else
static void *relay_thread_main(void *arg) {
    (void)arg;
    relay_run(g_host_port);
    return NULL;
}
#endif

int client_run_host(const char *port) {
    snprintf(g_host_port, sizeof(g_host_port), "%s", port);

#ifdef _WIN32
    HANDLE h = CreateThread(NULL, 0, relay_thread_main, NULL, 0, NULL);
    if (h == NULL) {
        fprintf(stderr, "не удалось запустить relay-поток\n");
        return 1;
    }
    CloseHandle(h);
#else
    pthread_t tid;
    if (pthread_create(&tid, NULL, relay_thread_main, NULL) != 0) {
        fprintf(stderr, "не удалось запустить relay-поток\n");
        return 1;
    }
    pthread_detach(tid);
#endif

    // Даём relay время подняться, затем подключаемся к нему локально.
    net_sleep_ms(300);
    printf("relay поднят на порту %s; второй участник пусть подключается\n", port);
    printf("к твоему адресу (например, Tailscale 100.x.y.z) на порт %s\n", port);
    fflush(stdout);
    return client_run("127.0.0.1", port);
}
