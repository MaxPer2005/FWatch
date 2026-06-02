#ifdef _WIN32

#include "input.h"

#include <stdio.h>
#include <windows.h>

// Метка наших эмулированных событий: SendInput кладёт её в dwExtraInfo,
// а хук по ней отличает эхо от реального нажатия (антицикл без флагов/гонок).
#define SYNTHETIC_TAG ((ULONG_PTR)0x53504143) // "SPAC"

static on_space_pressed_fn g_callback = NULL;
static HHOOK g_hook = NULL;

// Колбэк низкоуровневого хука клавиатуры.
static LRESULT CALLBACK keyboard_hook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
        // Пропускаем собственные эмулированные пробелы.
        if (kb->vkCode == VK_SPACE && kb->dwExtraInfo != SYNTHETIC_TAG && g_callback) {
            g_callback();
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

int input_init(on_space_pressed_fn callback) {
    g_callback = callback;
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook, NULL, 0);
    if (g_hook == NULL) {
        fprintf(stderr, "не удалось установить хук клавиатуры (код %lu)\n",
                GetLastError());
        return 1;
    }
    return 0;
}

void input_simulate_space(void) {
    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_SPACE;
    inputs[0].ki.dwFlags = 0; // key down
    inputs[0].ki.dwExtraInfo = SYNTHETIC_TAG;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_SPACE;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP; // key up
    inputs[1].ki.dwExtraInfo = SYNTHETIC_TAG;

    SendInput(2, inputs, sizeof(INPUT));
}

void input_run_loop(void) {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

#endif // _WIN32
