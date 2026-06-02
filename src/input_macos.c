#ifdef __APPLE__

#include "input.h"

#include <stdio.h>
#include <ApplicationServices/ApplicationServices.h>

// keycode пробела на macOS.
#define KEY_SPACE 49

// Метка наших собственных (эмулированных) событий. Реальные нажатия имеют
// userData = 0, поэтому по этой метке мы надёжно отличаем эхо от ввода
// пользователя — без флагов и гонок между потоками.
#define SYNTHETIC_TAG 0x53504143454B4559LL // "SPACEKEY"

static on_space_pressed_fn g_callback = NULL;
static CFMachPortRef g_tap = NULL;
static CGEventSourceRef g_source = NULL;

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
        // Пропускаем наши собственные эмулированные пробелы (антицикл).
        if (keycode == KEY_SPACE && tag != SYNTHETIC_TAG && g_callback) {
            g_callback();
        }
    }
    return event;
}

int input_init(on_space_pressed_fn callback) {
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

void input_simulate_space(void) {
    // Из помеченного источника — событие будет распознано как наше и не уйдёт
    // обратно в сеть. Если источник создать не удалось, используем NULL.
    CGEventRef down = CGEventCreateKeyboardEvent(g_source, KEY_SPACE, true);
    CGEventRef up = CGEventCreateKeyboardEvent(g_source, KEY_SPACE, false);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down);
    CFRelease(up);
}

void input_run_loop(void) {
    CFRunLoopRun();
}

#endif // __APPLE__
