#include "input_recorder.h"
#include <stdio.h>

static InputRecorder *g_rec = NULL;

static int keyIsDown[256] = {0};

#define MOUSE_MOVE_MIN_DT 100
#define MOUSE_MOVE_MIN_DIST 50

static POINT lastMousePos = {0};
static ULONGLONG lastMouseMoveTime = 0;


// ================= INTERNAL =================

static void IR_RecordEvent(IR_Event e) {
    if (!g_rec || g_rec->count >= IR_MAX_EVENTS) return;
    g_rec->events[g_rec->count++] = e;
}

static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!g_rec || !g_rec->recording) return CallNextHookEx(NULL, nCode, wParam, lParam);
    

    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT*)lParam;

        // IGNORE injected input
        if (p->flags & LLKHF_INJECTED) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }
        if (p->vkCode == VK_F9 || p->vkCode == VK_F10 || p->vkCode == VK_F11) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }


        if (wParam == WM_KEYDOWN || wParam == WM_KEYUP) {
            ULONGLONG now = GetTickCount64();

            if (wParam == WM_KEYDOWN) {
                if (keyIsDown[p->vkCode]) return CallNextHookEx(NULL, nCode, wParam, lParam);
                keyIsDown[p->vkCode] = 1;
            }

            if (wParam == WM_KEYUP) {
                keyIsDown[p->vkCode] = 0;
            }

            IR_Event e = {0};
            e.type = 0;
            e.event = wParam;
            e.time = (DWORD)(now - g_rec->startTime);
            e.delta = (DWORD)(now - g_rec->lastEventTime);

            e.vkCode = p->vkCode;
            e.scanCode = p->scanCode;
            e.flags = p->flags;

            g_rec->lastEventTime = now;

            printf("Key %s: VK=%d : deltaTime=%lu \n", wParam == WM_KEYDOWN ? "DOWN" : "UP", p->vkCode, e.delta);

            IR_RecordEvent(e);
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!g_rec || !g_rec->recording) return CallNextHookEx(NULL, nCode, wParam, lParam);

    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT*)lParam;

        // IGNORE injected input
        if (p->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) {
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }

        ULONGLONG now = GetTickCount64();

        if (wParam == WM_MOUSEMOVE) {
            int dx = abs(p->pt.x - lastMousePos.x);
            int dy = abs(p->pt.y - lastMousePos.y);
            ULONGLONG dt = now - lastMouseMoveTime;

            if (dx < MOUSE_MOVE_MIN_DIST &&
                dy < MOUSE_MOVE_MIN_DIST &&
                dt < MOUSE_MOVE_MIN_DT) {
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            lastMousePos = p->pt;
            lastMouseMoveTime = now;
        }

        IR_Event e = {0};
        e.type = 1;
        e.event = wParam;
        e.time = (DWORD)(now - g_rec->startTime);
        e.delta = (DWORD)(now - g_rec->lastEventTime);

        e.pt = p->pt;
        e.mouseData = p->mouseData;

        g_rec->lastEventTime = now;

        printf("Mouse event %d at (%d,%d) : deltaTime=%lu\n", wParam, p->pt.x, p->pt.y, e.delta);

        IR_RecordEvent(e);
    }
    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}


// ================= API =================

int IR_Init(InputRecorder *rec) {
    if (!rec) return 0;

    ZeroMemory(rec, sizeof(*rec));
    g_rec = rec;

    rec->keyHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    rec->mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);

    return (rec->keyHook && rec->mouseHook);
}

void IR_Shutdown(InputRecorder *rec) {
    if (!rec) return;

    UnhookWindowsHookEx(rec->keyHook);
    UnhookWindowsHookEx(rec->mouseHook);

    g_rec = NULL;
}

void IR_Start(InputRecorder *rec) {
    rec->recording = 1;
    rec->count = 0;

    rec->startTime = GetTickCount64();
    rec->lastEventTime = rec->startTime;

    ZeroMemory(keyIsDown, sizeof(keyIsDown));
}

void IR_Stop(InputRecorder *rec) {
    rec->recording = 0;
}

int IR_SaveToFile(InputRecorder *rec, const char *filename) {
    if (!rec || rec->count <= 0) return 0;

    FILE *fp = fopen(filename, "wb");
    if (!fp) return 0;

    // Worst case: each event may expand into TWO INPUTs
    INPUT *inputs = (INPUT *)malloc(sizeof(INPUT) * rec->count * 2);
    if (!inputs) {
        fclose(fp);
        return 0;
    }

    int outCount = 0;

    // Virtual screen metrics (multi-monitor safe)
    int screenWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int screenLeft   = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenTop    = GetSystemMetrics(SM_YVIRTUALSCREEN);

    for (int i = 0; i < rec->count; i++) {
        IR_Event *e = &rec->events[i];

        // =========================
        // KEYBOARD
        // =========================
        if (e->type == 0) {
            INPUT *in = &inputs[outCount++];
            ZeroMemory(in, sizeof(INPUT));

            in->type = INPUT_KEYBOARD;
            in->ki.wVk = (WORD)e->vkCode;
            in->ki.wScan = (WORD)e->scanCode;
            in->ki.dwFlags = 0;

            if (e->event == WM_KEYUP)
                in->ki.dwFlags |= KEYEVENTF_KEYUP;

            in->ki.time = e->time;
            continue;
        }

        // =========================
        // MOUSE
        // =========================

        // Normalize to 0–65535 range (Windows absolute coords)
        LONG absX = (LONG)((double)(e->pt.x - screenLeft) * 65535.0 / (screenWidth  - 1));
        LONG absY = (LONG)((double)(e->pt.y - screenTop)  * 65535.0 / (screenHeight - 1));
        if (absX < 0) absX = 0;
        if (absX > 65535) absX = 65535;

        if (absY < 0) absY = 0;
        if (absY > 65535) absY = 65535;

        INPUT *in = &inputs[outCount++];
        ZeroMemory(in, sizeof(INPUT));

        in->type = INPUT_MOUSE;
        in->mi.time = e->time;

        switch (e->event) {
            case WM_MOUSEMOVE:
                in->mi.dwFlags = MOUSEEVENTF_MOVE |
                                 MOUSEEVENTF_ABSOLUTE;
                in->mi.dx = absX;
                in->mi.dy = absY;
                break;

            case WM_LBUTTONDOWN:
                in->mi.dwFlags = MOUSEEVENTF_LEFTDOWN |
                                 MOUSEEVENTF_ABSOLUTE;
                in->mi.dx = absX;
                in->mi.dy = absY;
                break;

            case WM_LBUTTONUP:
                in->mi.dwFlags = MOUSEEVENTF_LEFTUP |
                                 MOUSEEVENTF_ABSOLUTE;
                in->mi.dx = absX;
                in->mi.dy = absY;
                break;

            case WM_RBUTTONDOWN:
                in->mi.dwFlags = MOUSEEVENTF_RIGHTDOWN |
                                 MOUSEEVENTF_ABSOLUTE; 
                in->mi.dx = absX;
                in->mi.dy = absY;
                break;

            case WM_RBUTTONUP:
                in->mi.dwFlags = MOUSEEVENTF_RIGHTUP |
                                 MOUSEEVENTF_ABSOLUTE; 
                in->mi.dx = absX;
                in->mi.dy = absY;
                break;

            case WM_MBUTTONDOWN:
                in->mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN |
                                 MOUSEEVENTF_ABSOLUTE; 
                in->mi.dx = absX;
                in->mi.dy = absY;
                break;

            case WM_MBUTTONUP:
                in->mi.dwFlags = MOUSEEVENTF_MIDDLEUP |
                                 MOUSEEVENTF_ABSOLUTE |
                                 MOUSEEVENTF_MOVE;
                in->mi.dx = absX;
                in->mi.dy = absY;
                break;

            case WM_MOUSEWHEEL:
                in->mi.dwFlags = MOUSEEVENTF_WHEEL;
                in->mi.mouseData = (DWORD)(short)HIWORD(e->mouseData);
                break;

            default:
                // Skip unsupported events safely
                outCount--;
                continue;
        }
    }

    // Write to file
    fwrite(&outCount, sizeof(int), 1, fp);
    fwrite(inputs, sizeof(INPUT), outCount, fp);

    fclose(fp);
    free(inputs);

    return 1;
}