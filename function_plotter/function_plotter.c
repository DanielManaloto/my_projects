#include <SDL2/SDL.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "tinyexpr.c"

static int WIDTH = 900;
static int HEIGHT = 600;
static int SCALE_X = 20;
static int SCALE_Y = 10;

void draw_in_plane(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int thickness, uint32_t color) {
    // Convert to screen coordinates (center origin, invert Y)
    x1 = x1 + WIDTH / 2;
    y1 = HEIGHT / 2 - y1;
    x2 = x2 + WIDTH / 2;
    y2 = HEIGHT / 2 - y2;

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color) & 0xFF;

    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    // Draw thick line using simple offset
    for (int t = -thickness/2; t <= thickness/2; t++) {
        SDL_RenderDrawLine(renderer, x1 + t, y1, x2 + t, y2);
        SDL_RenderDrawLine(renderer, x1, y1 + t, x2, y2 + t);
    }
}

void draw_grid(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Black
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255); //Gray

    int gridSize = 50;
    for (int x = 0; x < WIDTH; x += gridSize) {
        SDL_RenderDrawLine(renderer, x, 0, x, HEIGHT);
    }
    for (int y = 0; y < HEIGHT; y += gridSize) {
        SDL_RenderDrawLine(renderer, 0, y, WIDTH, y);
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // White
    SDL_RenderDrawLine(renderer, WIDTH/2, 0, WIDTH/2, HEIGHT);
    SDL_RenderDrawLine(renderer, 0, HEIGHT/2, WIDTH, HEIGHT/2);

}


int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    char buffer[100];

    printf("Input String: ");
    scanf("%s", buffer);

    double x, y;
    int err;
    /* Store variable names and pointers. */
    te_variable vars[] = {{"x", &x}};
    te_expr *expr = te_compile(buffer, vars, 1, &err);

    if (!expr) {
    printf("Parse error at position %d\n", err);
    return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Line Plotter", SDL_WINDOWPOS_CENTERED,
         SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    int thickness = 1;
    int offset = thickness/2;

    SDL_Event event;
    int running = 1;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }
        SDL_Delay(10);
        draw_grid(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red
        SDL_Rect rect = {WIDTH/2 - offset, HEIGHT/2 - offset, thickness, thickness};
        SDL_RenderFillRect(renderer, &rect);
        
        int prev_y = 0;
        int first = 1;

        for (int i = -WIDTH/2; i < WIDTH/2; i++) {
            x = (double)i / SCALE_X;
            int y = (int)(te_eval(expr) * SCALE_Y);

            if (!first) {
                draw_in_plane(renderer, i-1, prev_y, i, y, thickness, 0x303030);
            }

            prev_y = y;
            first = 0;
        }

        SDL_RenderPresent(renderer); 
        SDL_Delay(100);

    }

    te_free(expr);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}