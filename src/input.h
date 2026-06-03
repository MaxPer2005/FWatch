#ifndef INPUT_H
#define INPUT_H

// Платформенный слой ввода: перехват и эмуляция управляющих клавиш плеера.

// Набор синхронизируемых клавиш. Значения важны: используются как индексы
// в таблицах keycode'ов и (через +1) как байты протокола.
typedef enum {
    SYNC_KEY_SPACE = 0, // play/pause
    SYNC_KEY_LEFT  = 1, // перемотка назад
    SYNC_KEY_RIGHT = 2, // перемотка вперёд
    SYNC_KEY_COUNT
} sync_key_t;

typedef void (*on_key_fn)(sync_key_t key);

// Инициализирует перехват. Вызывает callback при нажатии любой из клавиш.
// Возвращает 0 при успехе, ненулевое при ошибке.
int input_init(on_key_fn callback);

// Эмулирует нажатие клавиши (down + up).
void input_simulate_key(sync_key_t key);

// Запускает event loop (блокирует). Вызывать из main thread.
void input_run_loop(void);

#endif // INPUT_H
