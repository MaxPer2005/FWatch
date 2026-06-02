// Заглушка платформенного слоя для платформ без поддержки клавиатуры
// (например, Linux — там используется только relay-режим).
#if !defined(__APPLE__) && !defined(_WIN32)

#include "input.h"
#include <stdio.h>

int input_init(on_space_pressed_fn callback) {
    (void)callback;
    fprintf(stderr,
            "режимы client/test не поддерживаются на этой платформе\n"
            "(перехват клавиатуры реализован только для macOS и Windows).\n"
            "используй этот бинарник в режиме relay.\n");
    return 1;
}

void input_simulate_space(void) {
}

void input_run_loop(void) {
}

#endif
