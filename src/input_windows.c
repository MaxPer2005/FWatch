#ifdef _WIN32

#include "input.h"

#include <stdio.h>
#include <windows.h>

// Виртуальные коды Windows для синхронизируемых клавиш (индекс = sync_key_t).
static const int VKS[SYNC_KEY_COUNT] = {
    VK_SPACE, // SYNC_KEY_SPACE
    VK_LEFT,  // SYNC_KEY_LEFT
    VK_RIGHT, // SYNC_KEY_RIGHT
};

// Метка наших эмулированных событий: SendInput кладёт её в dwExtraInfo,
// а хук по ней отличает эхо от реального нажатия (антицикл без флагов/гонок).
#define SYNTHETIC_TAG ((ULONG_PTR)0x53504143) // "SPAC"

// Запасная защита от эха по времени (на случай гонки с тегом).
#define ECHO_GUARD_MS 200

static on_key_fn g_callback = NULL;
static HHOOK g_hook = NULL;
static ULONGLONG g_last_emulate_ms[SYNC_KEY_COUNT];

// Возвращает sync_key_t по виртуальному коду или -1.
static int vk_to_key(DWORD vk) {
    for (int i = 0; i < SYNC_KEY_COUNT; i++) {
        if ((DWORD)VKS[i] == vk) {
            return i;
        }
    }
    return -1;
}

// Колбэк низкоуровневого хука клавиатуры.
static LRESULT CALLBACK keyboard_hook(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT *kb = (KBDLLHOOKSTRUCT *)lParam;
        int key = vk_to_key(kb->vkCode);
        if (key >= 0 && g_callback) {
            int ours = (kb->dwExtraInfo == SYNTHETIC_TAG) ||
                       (GetTickCount64() - g_last_emulate_ms[key] < ECHO_GUARD_MS);
            if (!ours) {
                g_callback((sync_key_t)key);
            }
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

int input_init(on_key_fn callback) {
    g_callback = callback;
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook, NULL, 0);
    if (g_hook == NULL) {
        fprintf(stderr, "не удалось установить хук клавиатуры (код %lu)\n",
                GetLastError());
        return 1;
    }
    return 0;
}

void input_simulate_key(sync_key_t key) {
    if (key < 0 || key >= SYNC_KEY_COUNT) {
        return;
    }
    WORD vk = (WORD)VKS[key];
    g_last_emulate_ms[key] = GetTickCount64(); // окно для запасной защиты от эха
    // Стрелки — расширенные клавиши: ставим флаг для корректной обработки.
    DWORD ext = (key == SYNC_KEY_LEFT || key == SYNC_KEY_RIGHT)
                    ? KEYEVENTF_EXTENDEDKEY : 0;

    INPUT inputs[2];
    ZeroMemory(inputs, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[0].ki.dwFlags = ext; // key down
    inputs[0].ki.dwExtraInfo = SYNTHETIC_TAG;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = ext | KEYEVENTF_KEYUP; // key up
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
