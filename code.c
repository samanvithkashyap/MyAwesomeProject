#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define SCREEN_SIZE 800
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 2048
#define MAX_PARTICLES 300
#define BASE_RADIUS 100
#define BEAT_THRESHOLD 1.4

typedef struct {
    float x, y;
    float dx, dy;
    int lifetime;
    Uint8 alpha;
} Particle;

typedef struct {
    float amplitude;
    float beat_energy;
    Uint32 last_beat;
    Particle particles[MAX_PARTICLES];
    float hue;
} VisualizerState;

SDL_mutex *mutex;
VisualizerState state = {0};

// --------------------------
// HELPER FUNCTIONS
// --------------------------

void HSLtoRGB(float h, float s, float l, Uint8 *r, Uint8 *g, Uint8 *b) {
    h = fmod(h, 360);
    float c = (1 - fabs(2*l - 1)) * s;
    float x = c * (1 - fabs(fmod(h/60.0, 2) - 1));
    float m = l - c/2;

    if(h < 60) { *r=(c+m)*255; *g=(x+m)*255; *b=m*255; }
    else if(h < 120) { *r=(x+m)*255; *g=(c+m)*255; *b=m*255; }
    else if(h < 180) { *r=m*255; *g=(c+m)*255; *b=(x+m)*255; }
    else if(h < 240) { *r=m*255; *g=(x+m)*255; *b=(c+m)*255; }
    else { *r=(x+m)*255; *g=m*255; *b=(c+m)*255; }
}

float rand_range(float min, float max) {
    return min + (float)rand() / (RAND_MAX/(max - min));
}

// --------------------------
// AUDIO PROCESSING
// --------------------------

void audio_callback(void *userdata, Uint8 *stream, int len) {
    Sint16 *samples = (Sint16*)stream;
    int num_samples = len / sizeof(Sint16);
    float sum = 0;

    for(int i=0; i<num_samples; i++) {
        sum += fabs(samples[i]) / 32768.0f;
    }
    
    float energy = sum / num_samples;
    
    SDL_LockMutex(mutex);
    state.amplitude = energy;
    state.beat_energy = 0.9f * state.beat_energy + 0.1f * energy;
    
    if(energy > state.beat_energy * BEAT_THRESHOLD) {
        state.last_beat = SDL_GetTicks();
        // Create new particles
        for(int i=0; i<MAX_PARTICLES; i++) {
            if(state.particles[i].lifetime <= 0) {
                float angle = rand_range(0, M_PI*2);
                float speed = rand_range(1.0f, 3.0f);
                
                state.particles[i].x = SCREEN_SIZE/2;
                state.particles[i].y = SCREEN_SIZE/2;
                state.particles[i].dx = cos(angle) * speed;
                state.particles[i].dy = sin(angle) * speed;
                state.particles[i].lifetime = rand()%30 + 20;
                state.particles[i].alpha = 255;
                break;
            }
        }
    }
    
    state.hue = fmod(state.hue + 0.3f, 360);
    SDL_UnlockMutex(mutex);
}

// --------------------------
// VISUALIZATION
// --------------------------

void draw_circle(SDL_Renderer *renderer, float radius) {
    Uint8 r, g, b;
    HSLtoRGB(state.hue, 0.8f, 0.6f, &r, &g, &b);
    
    for(int i=0; i<360; i+=2) {
        float angle = i * M_PI / 180;
        int x = SCREEN_SIZE/2 + cos(angle) * radius;
        int y = SCREEN_SIZE/2 + sin(angle) * radius;
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderDrawPoint(renderer, x, y);
    }
}

void draw_particles(SDL_Renderer *renderer) {
    Uint8 r, g, b;
    HSLtoRGB(state.hue, 0.8f, 0.7f, &r, &g, &b);
    
    for(int i=0; i<MAX_PARTICLES; i++) {
        if(state.particles[i].lifetime > 0) {
            state.particles[i].x += state.particles[i].dx;
            state.particles[i].y += state.particles[i].dy;
            state.particles[i].lifetime--;
            state.particles[i].alpha -= 8;
            
            SDL_SetRenderDrawColor(renderer, r, g, b, state.particles[i].alpha);
            SDL_RenderDrawPoint(renderer, 
                state.particles[i].x, 
                state.particles[i].y);
        }
    }
}

void draw_visualization(SDL_Renderer *renderer) {
    // Dark background
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_RenderClear(renderer);

    SDL_LockMutex(mutex);
    // Calculate animated radius
    float beat_strength = (SDL_GetTicks() - state.last_beat) / 1000.0f;
    float radius = BASE_RADIUS + state.amplitude * 150 - beat_strength * 50;
    
    draw_circle(renderer, radius);
    draw_particles(renderer);
    SDL_UnlockMutex(mutex);
}

// --------------------------
// MAIN FUNCTION
// --------------------------

int main() {
    srand(time(NULL));
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    
    SDL_Window *window = SDL_CreateWindow("Audio Visualizer", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_SIZE, SCREEN_SIZE, 0);
        
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    mutex = SDL_CreateMutex();

    // Audio setup
    SDL_AudioSpec desired = {0};
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = BUFFER_SIZE;
    desired.callback = audio_callback;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 1, &desired, NULL, 0);
    SDL_PauseAudioDevice(dev, 0);

    while(1) {
        SDL_Event e;
        while(SDL_PollEvent(&e)) if(e.type == SDL_QUIT) exit(0);
        
        draw_visualization(renderer);
        SDL_RenderPresent(renderer);
    }

    return 0;
}