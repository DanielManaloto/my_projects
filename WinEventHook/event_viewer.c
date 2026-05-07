#include <windows.h>
#include <stdio.h>

void DumpEventsRaw(const char *filename) {
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

    for (int i = 0; i < count; i++) {
        if (fread(&in, sizeof(INPUT), 1, fp) != 1) {
            printf("Failed to read event %d\n", i);
            break;
        }

        if (in.type == INPUT_KEYBOARD) {
            printf(
                "[%d] KEY | vk=%u | scan=%u | flags=0x%lx | time=%lu\n",
                i,
                in.ki.wVk,
                in.ki.wScan,
                in.ki.dwFlags,
                in.ki.time
            );
        }
        else if (in.type == INPUT_MOUSE) {
            LONG x = in.mi.dx;
            LONG y = in.mi.dy;

            // If absolute, convert back to screen coords for readability
            if (in.mi.dwFlags & MOUSEEVENTF_ABSOLUTE) {
                int screenW = GetSystemMetrics(SM_CXSCREEN);
                int screenH = GetSystemMetrics(SM_CYSCREEN);

                x = (in.mi.dx * screenW) / 65535;
                y = (in.mi.dy * screenH) / 65535;
            }

            printf(
                "[%d] MOUSE | flags=0x%-10lx | x=%ld | y=%ld | data=%lu | time=%lu\n",
                i,
                in.mi.dwFlags,
                x,
                y,
                in.mi.mouseData,
                in.mi.time
            );
        }
        else {
            printf("[%d] UNKNOWN INPUT TYPE: %lu\n", i, in.type);
        }
    }

    fclose(fp);
}

int main() {
    DumpEventsRaw("events.bin");
    return 0;
}
