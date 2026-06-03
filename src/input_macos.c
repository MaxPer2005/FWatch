#ifdef __APPLE__

#include "input.h"

#include <stdio.h>
#include <time.h>
#include <dispatch/dispatch.h>
#include <ApplicationServices/ApplicationServices.h>

// keycode'ы macOS для синхронизируемых клавиш (индекс = sync_key_t).
static const int KEYCODES[SYNC_KEY_COUNT] = {
    49,  // SYNC_KEY_SPACE
    123, // SYNC_KEY_LEFT
    124, // SYNC_KEY_RIGHT
};

// Метка наших собственных (эмулированных) событий. Реальные нажатия имеют
// userData = 0, поэтому по этой метке мы надёжно отличаем эхо от ввода
// пользователя — без флагов и гонок между потоками.
#define SYNTHETIC_TAG 0x53504143454B4559LL // "SPACEKEY"

// Запасная защита от эха: если тег вдруг прочитался неверно (гонка при
// межпоточном CGEventPost), всё равно подавляем нажатие, пришедшее в течение
// окна сразу после нашей собственной эмуляции этой же клавиши.
#define ECHO_GUARD_MS 200.0

static on_key_fn g_callback = NULL;
static CFMachPortRef g_tap = NULL;
static CGEventSourceRef g_source = NULL;
static double g_last_emulate_ms[SYNC_KEY_COUNT];

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1.0e6;
}

// Возвращает sync_key_t по keycode или -1.
static int keycode_to_key(int64_t keycode) {
    for (int i = 0; i < SYNC_KEY_COUNT; i++) {
        if (KEYCODES[i] == keycode) {
            return i;
        }
    }
    return -1;
}

// Колбэк event tap: вызывается для каждого нажатия клавиши.
static CGEventRef tap_callback(CGEventTapProxy proxy, CGEventType type,
                               CGEventRef event, void *user_info) {
    (void)proxy;
    (void)user_info;

    // Если tap отключили из-за таймаута/ошибки — включаем обратно.
    if (type == kCGEventTapDisabledByTimeout ||
        type == kCGEventTapDisabledByUserInput) {
        if (g_tap) {
            CGEventTapEnable(g_tap, true);
        }
        return event;
    }

    if (type == kCGEventKeyDown) {
        int64_t keycode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        int64_t tag = CGEventGetIntegerValueField(event, kCGEventSourceUserData);
        int key = keycode_to_key(keycode);
        if (key >= 0 && g_callback) {
            int ours = (tag == SYNTHETIC_TAG) ||
                       (now_ms() - g_last_emulate_ms[key] < ECHO_GUARD_MS);
            if (!ours) {
                g_callback((sync_key_t)key);
            }
        }
    }
    return event;
}

int input_init(on_key_fn callback) {
    g_callback = callback;

    // Источник событий с меткой — все созданные из него события несут наш тег.
    g_source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (g_source) {
        CGEventSourceSetUserData(g_source, SYNTHETIC_TAG);
    }

    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown);
    g_tap = CGEventTapCreate(kCGHIDEventTap,
                             kCGHeadInsertEventTap,
                             kCGEventTapOptionDefault,
                             mask,
                             tap_callback,
                             NULL);
    if (g_tap == NULL) {
        fprintf(stderr,
                "не удалось создать event tap.\n"
                "Включи разрешение Accessibility в System Settings >\n"
                "Privacy & Security > Accessibility для терминала.\n");
        return 1;
    }

    CFRunLoopSourceRef source =
        CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    CFRelease(source);
    CGEventTapEnable(g_tap, true);
    return 0;
}

// Собственно эмуляция. Выполняется ВСЕГДА на главном потоке (там же, где
// крутится event tap), поэтому нет межпоточной гонки CGEventPost и проблем
// видимости g_last_emulate_ms.
static void do_emulate(sync_key_t key) {
    int keycode = KEYCODES[key];
    g_last_emulate_ms[key] = now_ms(); // окно для запасной защиты от эха
    // Из помеченного источника — событие распознаётся как наше и не уходит
    // обратно в сеть. Если источник создать не удалось, используем NULL.
    CGEventRef down = CGEventCreateKeyboardEvent(g_source, (CGKeyCode)keycode, true);
    CGEventRef up = CGEventCreateKeyboardEvent(g_source, (CGKeyCode)keycode, false);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down);
    CFRelease(up);
}

void input_simulate_key(sync_key_t key) {
    if (key < 0 || key >= SYNC_KEY_COUNT) {
        return;
    }
    // Вызывается из сетевого потока — переносим эмуляцию на главный поток
    // (run loop сам обслуживает main queue внутри CFRunLoopRun).
    dispatch_async(dispatch_get_main_queue(), ^{
        do_emulate(key);
    });
}

void input_run_loop(void) {
    CFRunLoopRun();
}

#endif // __APPLE__
