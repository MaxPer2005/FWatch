#ifndef CLIENT_H
#define CLIENT_H

// Запускает клиент: подключается к relay-серверу, поднимает сетевой поток
// и запускает платформенный event loop в main thread.
// server_ip и port задаются явно.
// Возвращает 0 при штатном завершении, ненулевое при ошибке.
int client_run(const char *server_ip, const char *port);

// Запускает локальный тест: перехват пробела сразу эмулирует пробел обратно.
// Без сети. Для проверки платформенного слоя.
int client_run_test(void);

#endif // CLIENT_H
