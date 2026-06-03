# sync — синхронизация нажатия пробела между двумя компьютерами

Кроссплатформенная CLI-утилита на чистом C23. Когда один пользователь нажимает
пробел — у второго эмулируется нажатие пробела, и наоборот. Связь идёт через
простой TCP-relay сервер.

## Быстрая установка (одной командой)

Скачивает готовый бинарь и сразу подключается к relay-серверу.

**macOS** (Terminal):
```sh
curl -fsSL https://raw.githubusercontent.com/MaxPer2005/FWatch/main/install.sh | bash
```

**Windows** (PowerShell):
```powershell
irm https://raw.githubusercontent.com/MaxPer2005/FWatch/main/install.ps1 | iex
```

> **macOS, первый запуск:** система попросит разрешение Accessibility. Зайди в
> *System Settings → Privacy & Security → Accessibility*, включи свой терминал
> и запусти снова: `~/.local/bin/sync client 91.228.153.31 9000`.

Запусти команду на обоих компьютерах — и пробел синхронизируется между ними.

## Режимы

```
sync relay <port>               # на Linux-сервере: пересылает байты между двумя клиентами
sync client <server_ip> <port>  # на macOS/Windows: перехват + эмуляция пробела
sync test                       # локальный тест платформенного слоя (без сети)
```

## Протокол

Один байт `0x01` = «нажат пробел». Больше ничего. Без шифрования, без
автообнаружения — IP и порт задаются явно.

## Сборка

### CMake (рекомендуется)

```sh
cmake -S . -B build
cmake --build build
# исполняемый файл: build/sync
```

CMake сам определяет платформу: на macOS линкует `-framework ApplicationServices`,
на Windows — `ws2_32`, на Linux — только сокеты и pthread.

### Без CMake

macOS:
```sh
clang -std=c23 -o sync src/main.c src/relay.c src/client.c src/net.c \
  src/input_macos.c -framework ApplicationServices
```

Linux (только relay):
```sh
# gcc 14+/clang: -std=c23 ; gcc 13 и старше: -std=c2x
gcc -std=c2x -O2 -o sync src/main.c src/relay.c src/client.c src/net.c \
  src/input_stub.c -lpthread
```

> На glibc строгий режим C23 прячет POSIX-сокеты (`getaddrinfo`, `select`);
> `net.h` сам определяет `_GNU_SOURCE`, поэтому отдельный флаг не нужен.
> Проверено на Ubuntu 24.04, gcc 13.3.

Windows (MinGW):
```sh
gcc -std=c23 -o sync.exe src/main.c src/relay.c src/client.c src/net.c \
  src/input_windows.c -lws2_32
```

## Разрешения

- **macOS**: требуется разрешение Accessibility (System Settings → Privacy &
  Security → Accessibility) для приложения, из которого запускается `sync`
  (терминал/iTerm). Без него `CGEventTapCreate` вернёт ошибку.
- **Windows**: низкоуровневый хук работает без особых прав; для эмуляции в
  программах с повышенными правами может потребоваться запуск от администратора.

## Пример

На сервере (Linux):
```sh
./sync relay 9000
```

На двух машинах (macOS/Windows):
```sh
./sync client 203.0.113.5 9000
```

Теперь пробел на одной машине нажимает пробел на другой.

## Структура

```
CMakeLists.txt        кроссплатформенная сборка
src/
  main.c              парсинг аргументов, точка входа
  relay.c / relay.h   relay-сервер (select по двум клиентам)
  client.c / client.h клиент: сеть (отдельный поток) + платформенный event loop
  net.c / net.h       обёртка над сокетами (Winsock / POSIX)
  input.h             общий интерфейс платформенного слоя
  input_macos.c       перехват/эмуляция через CGEventTap (#ifdef __APPLE__)
  input_windows.c     перехват/эмуляция через WH_KEYBOARD_LL / SendInput (#ifdef _WIN32)
  input_stub.c        заглушка для Linux (client/test не поддерживаются)
```

## Детали реализации

- **Антицикл (без гонок)**: эмулированные события помечаются — на macOS через
  источник с `CGEventSourceSetUserData`, на Windows через `dwExtraInfo`.
  Платформенный слой отбрасывает свои же события по метке ещё до колбэка,
  поэтому эхо не уходит обратно в сеть. Общих флагов между потоками нет.
- **Потоки**: приём из сети (`recv` в цикле) — в отдельном потоке (pthread на
  macOS, CreateThread на Windows); main thread занят event loop'ом. Сокет под
  защитой мьютекса, т.к. меняется при переподключении.
- **Relay**: `select` по двум сокетам; обслуживает пары **по кругу** — после
  обрыва одного клиента второму шлёт `peer disconnected` и принимает новую пару
  (процесс не завершается).
- **Клиент — автопереподключение**: при обрыве сетевой поток переподключается с
  backoff (1→2→3→5→10 с); event loop продолжает работать. Переживает рестарт
  relay без ручного вмешательства.
- **Надёжность сети**: `SIGPIPE` игнорируется (обрыв пира не убивает процесс),
  `TCP_NODELAY` (минимальная задержка на нажатие), `SO_KEEPALIVE`,
  безопасная дозапись `send` и повтор при `EINTR`.

## Развёртывание relay (Linux + systemd)

`deploy.sh` собирает бинарь и ставит systemd-юнит `fwatch-relay` с
`Restart=always` (автоподъём после сбоя) на порту 9000:

```sh
# на сервере, из /root/fwatch (исходники + deploy.sh)
bash deploy.sh
# управление:
systemctl status fwatch-relay
journalctl -u fwatch-relay -f
```

Проверено на боевом сервере: relay обслуживает пары по кругу без падений,
а после `kill -9` systemd поднимает его заново (`NRestarts` растёт, порт снова
слушается). Клиент при этом сам переподключается.
