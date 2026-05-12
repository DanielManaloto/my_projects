#include "raylib.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>


#define WIDTH 900
#define HEIGHT 600

#define NUM_PARTICLES 60

typedef struct Particle {
    float x, y, r, vx, vy;
} Particle;

Particle particles[NUM_PARTICLES];

void DrawParticles(Particle particles[]){
    for(int i = 0; i < NUM_PARTICLES; i++) {
        DrawCircle(particles[i].x, particles[i].y, particles[i].r, WHITE);
    }

    
}

void CreateParticles(Particle particles[], int num_particles) {
    for (int i = 0; i < num_particles; i++) {
    float r = 5 + rand() % 10; // random radius
    int overlap;
        do {
            overlap = 0;

            float x = r + rand() % (WIDTH - (int)(2*r));
            float y = r + rand() % (HEIGHT - (int)(2*r));

            for (int j = 0; j < i; j++) {
                float dx = x - particles[j].x;
                float dy = y - particles[j].y;
                float dist = sqrtf(dx*dx + dy*dy);

                if (dist < r + particles[j].r) {
                    overlap = 1;
                    break;
                }
            }

            if (!overlap) {
                particles[i].x = x;
                particles[i].y = y;
            }

        } while (overlap);

        float vx = (rand() % 5) - 2; 
        float vy = (rand() % 5) - 2;
        particles[i].r = r;
        particles[i].vx = vx;
        particles[i].vy = vy;
    }
}

void UpdateParticles(Particle particles[], int num_particles){
    for (int i = 0; i < num_particles; i++) {

        // Move particle
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;

        float x = particles[i].x;
        float y = particles[i].y;
        float r = particles[i].r;

        // Wall collision
        if (x >= (WIDTH - r)){
            particles[i].vx *= -1;
            particles[i].x = WIDTH - r - 3;
        }
        if(x <= r){
            particles[i].vx *= -1;
            particles[i].x = r + 3;
        }
        if (y >= (HEIGHT - r)) {
            particles[i].vy *= -1;
            particles[i].y = HEIGHT - r -3;
        }
        if (y <= r) {
            particles[i].vy *= -1;
            particles[i].y = r + 3;
        }

        // Particle collision
        for (int j = i + 1; j < num_particles; j++) {

            float dx = particles[j].x - particles[i].x;
            float dy = particles[j].y - particles[i].y;
            float dist = sqrtf(dx*dx + dy*dy);

            float minDist = particles[i].r + particles[j].r;

            if (dist < minDist) {
                if (dist == 0.0f) dist = 0.01f; // avoid divide by zero

                // Normal
                float nx = dx / dist;
                float ny = dy / dist;

                // Tangent
                float tx = -ny;
                float ty = nx;

                // Dot product tangent
                float dpTan1 = particles[i].vx * tx + particles[i].vy * ty;
                float dpTan2 = particles[j].vx * tx + particles[j].vy * ty;

                // Dot product normal
                float dpNorm1 = particles[i].vx * nx + particles[i].vy * ny;
                float dpNorm2 = particles[j].vx * nx + particles[j].vy * ny;

                // Swap normal velocities (equal mass elastic collision)
                float m1 = dpNorm2;
                float m2 = dpNorm1;

                // Final velocities
                particles[i].vx = tx * dpTan1 + nx * m1;
                particles[i].vy = ty * dpTan1 + ny * m1;

                particles[j].vx = tx * dpTan2 + nx * m2;
                particles[j].vy = ty * dpTan2 + ny * m2;

                // Separate overlap
                float overlap = minDist - dist;
                particles[i].x -= nx * overlap / 2;
                particles[i].y -= ny * overlap / 2;
                particles[j].x += nx * overlap / 2;
                particles[j].y += ny * overlap / 2;
            }
        }
    }
}

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(WIDTH, HEIGHT, "Collidision Simulation");
    SetTargetFPS(60); 
    srand(time(NULL)); // Use current time as the seed
    CreateParticles(particles, NUM_PARTICLES);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            
            ClearBackground(BLACK);
            DrawParticles(particles);
            UpdateParticles(particles, NUM_PARTICLES);
            DrawFPS(5,5);
            // DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
