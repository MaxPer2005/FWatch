#include "relay.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Создаёт слушающий сокет на указанном порту.
static socket_t make_listener(const char *port) {
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &res) != 0 || res == NULL) {
        fprintf(stderr, "[relay] не удалось разобрать порт %s\n", port);
        return INVALID_SOCK;
    }

    socket_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCK) {
        fprintf(stderr, "[relay] не удалось создать сокет\n");
        freeaddrinfo(res);
        return INVALID_SOCK;
    }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    if (bind(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        fprintf(stderr, "[relay] не удалось привязаться к порту %s\n", port);
        close_socket(s);
        freeaddrinfo(res);
        return INVALID_SOCK;
    }
    freeaddrinfo(res);

    if (listen(s, 8) != 0) {
        fprintf(stderr, "[relay] listen() не удался\n");
        close_socket(s);
        return INVALID_SOCK;
    }
    return s;
}

// Принимает одно входящее соединение, печатает адрес клиента.
// EINTR не считается ошибкой. Возвращает INVALID_SOCK при реальной ошибке.
static socket_t accept_client(socket_t listener, int index) {
    for (;;) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        socket_t c = accept(listener, (struct sockaddr *)&addr, &len);
        if (c == INVALID_SOCK) {
#ifndef _WIN32
            if (errno == EINTR) {
                continue;
            }
#endif
            fprintf(stderr, "[relay] accept() не удался\n");
            return INVALID_SOCK;
        }
        char ip[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        printf("[relay] клиент %c подключился: %s:%d\n",
               index == 0 ? 'A' : 'B', ip, (int)ntohs(addr.sin_port));
        fflush(stdout);
        return c;
    }
}

// Обслуживает одну пару: пересылает байты между a и b до отключения любого.
// Возвращается (не завершает процесс), чтобы можно было принять новую пару.
static void serve_pair(socket_t a, socket_t b) {
    socket_t peers[2] = { a, b };
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(a, &rfds);
        FD_SET(b, &rfds);
        socket_t maxfd = (a > b) ? a : b;

        int ready = select((int)maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ready < 0) {
#ifndef _WIN32
            if (errno == EINTR) {
                continue;
            }
#endif
            fprintf(stderr, "[relay] select() не удался\n");
            return;
        }

        for (int i = 0; i < 2; i++) {
            socket_t src = peers[i];
            socket_t dst = peers[i ^ 1];
            if (!FD_ISSET(src, &rfds)) {
                continue;
            }
            unsigned char buf[256];
            int n = (int)recv(src, (char *)buf, sizeof(buf), 0);
            if (n < 0) {
#ifndef _WIN32
                if (errno == EINTR) {
                    continue;
                }
#endif
                n = 0; // прочие ошибки трактуем как отключение
            }
            if (n == 0) {
                printf("[relay] клиент %c отключился\n", i == 0 ? 'A' : 'B');
                fflush(stdout);
                // Сообщаем второму клиенту, что пара распалась.
                const char *msg = "peer disconnected";
                net_send_all(dst, msg, (int)strlen(msg));
                return;
            }
            if (net_send_all(dst, buf, n) != 0) {
                printf("[relay] клиент %c недоступен (ошибка отправки)\n",
                       i == 0 ? 'B' : 'A');
                fflush(stdout);
                return;
            }
            printf("[relay] %c -> %c (%d байт)\n",
                   i == 0 ? 'A' : 'B', i == 0 ? 'B' : 'A', n);
            fflush(stdout);
        }
    }
}

int relay_run(const char *port) {
    if (net_startup() != 0) {
        fprintf(stderr, "[relay] не удалось инициализировать сеть\n");
        return 1;
    }

    socket_t listener = make_listener(port);
    if (listener == INVALID_SOCK) {
        net_cleanup();
        return 1;
    }
    printf("[relay] слушаю порт %s\n", port);
    fflush(stdout);

    // Обслуживаем пары по кругу: одна пара ушла — принимаем следующую.
    for (;;) {
        printf("[relay] жду двух клиентов...\n");
        fflush(stdout);

        socket_t a = accept_client(listener, 0);
        if (a == INVALID_SOCK) {
            net_sleep_ms(100); // не крутимся вхолостую при ошибке accept
            continue;
        }
        socket_t b = accept_client(listener, 1);
        if (b == INVALID_SOCK) {
            close_socket(a);
            net_sleep_ms(100);
            continue;
        }

        net_configure_socket(a);
        net_configure_socket(b);
        printf("[relay] оба клиента на связи, пересылаю байты\n");
        fflush(stdout);

        serve_pair(a, b);

        close_socket(a);
        close_socket(b);
        printf("[relay] пара завершена, жду новую\n");
        fflush(stdout);
    }

    // недостижимо
}
