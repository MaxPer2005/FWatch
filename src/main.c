#include "relay.h"
#include "client.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
#endif

static void usage(const char *prog) {
    fprintf(stderr,
            "использование:\n"
            "  %s relay <port>              запустить relay-сервер\n"
            "  %s client <server_ip> <port> подключиться к relay и синхронизировать пробел\n"
            "  %s test                      локальный тест платформенного слоя\n",
            prog, prog, prog);
}

int main(int argc, char **argv) {
#ifdef _WIN32
    // Консоль Windows по умолчанию не UTF-8 — иначе русские строки в кракозябрах.
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "relay") == 0) {
        if (argc != 3) {
            usage(argv[0]);
            return 2;
        }
        return relay_run(argv[2]);
    }

    if (strcmp(argv[1], "client") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 2;
        }
        return client_run(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "test") == 0) {
        if (argc != 2) {
            usage(argv[0]);
            return 2;
        }
        return client_run_test();
    }

    usage(argv[0]);
    return 2;
}
