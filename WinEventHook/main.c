#include <windows.h>
#include <stdio.h>
#include "input_recorder.h"

#define HOTKEY_TOGGLE VK_F9
#define HOTKEY_EXIT   VK_F10

void ClearConsoleInput() {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hInput);
}

void Replay(const char *filename){

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Failed to open file: %s\n", filename);
        return;
    }

    int count = 0;

    if (fread(&count, sizeof(int), 1, fp) != 1) {
        printf("Failed to read event count\n");
        fclose(fp);
        return;
    }

    printf("Event count: %d\n\n", count);
    INPUT in;
    DWORD lastTime = 0;
    DWORD delay = 10;
    for (int i = 0; i < count; i++) {
        in = (INPUT){0};
        if (fread(&in, sizeof(INPUT), 1, fp) != 1) {
            printf("Failed to read event %d\n", i);
            break;
        }
        
        if (in.type == INPUT_KEYBOARD) {
            delay = in.ki.time - lastTime;
            lastTime = in.ki.time;
            in.ki.time = 0;
        }

        else if (in.type == INPUT_MOUSE) {
            delay = in.mi.time - lastTime;
            lastTime = in.mi.time;
            in.mi.time = 0;
        }

        Sleep(delay * 0.05f);
        SendInput(1, &in, sizeof(INPUT));
    }
    fclose(fp);
}

int main() {
    InputRecorder rec;
    MSG msg;

    if (!IR_Init(&rec)) {
        printf("Failed to init recorder\n");
        return 1;
    }

    RegisterHotKey(NULL, 1, 0, VK_F9);
    RegisterHotKey(NULL, 2, 0, VK_F10);
    RegisterHotKey(NULL, 3, 0, VK_F11);

    printf("F9 = Start/Stop\nF10 = Exit\nF11 = Replay\n");

    while (GetMessage(&msg, NULL, 0, 0)) {

        if (msg.message == WM_HOTKEY) {

            if (msg.wParam == 1) { // F9
                if (!rec.recording) {
                    printf("Recording...\n");
                    IR_Start(&rec);
                } else {
                    ClearConsoleInput();
                    IR_Stop(&rec);
                    IR_SaveToFile(&rec, "events.bin");
                    printf("Saved %d events\n", rec.count);
                }
            }

            if (msg.wParam == 2) { // F10
                break;
            }
            if (msg.wParam == 3) { // F11
                Replay("events.bin");
                break;
            }

        }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
    }   

    UnregisterHotKey(NULL, 1);
    UnregisterHotKey(NULL, 2);

    IR_Shutdown(&rec);
    return 0;
}