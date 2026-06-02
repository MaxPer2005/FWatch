#ifndef INPUT_H
#define INPUT_H

// Платформенный слой ввода: перехват и эмуляция нажатия пробела.

typedef void (*on_space_pressed_fn)(void);

// Инициализирует перехват. Вызывает callback при каждом нажатии пробела.
// Возвращает 0 при успехе, ненулевое значение при ошибке.
int input_init(on_space_pressed_fn callback);

// Эмулирует нажатие пробела (down + up).
void input_simulate_space(void);

// Запускает event loop (блокирует). Вызывать из main thread.
void input_run_loop(void);

#endif // INPUT_H
