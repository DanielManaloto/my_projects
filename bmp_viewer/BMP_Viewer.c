#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h> 
#include <SDL2/SDL.h>

uint8_t* decompress_rle(uint8_t *in_buf, uint32_t in_size, int width, int height, int bpp);
uint8_t* apply_palette(uint8_t *palette, uint8_t *old_data, int width, int height, int bpp, int is_rle);
void render_bmp(uint8_t *pixels, int width, int height, int bpp, int compression);


static int count_trailing_zeros(uint32_t mask);
static int count_bits(uint32_t mask);

static uint8_t normalize_to_8bit(uint32_t value, int bits);

uint8_t* bitfields_to_bgra(uint8_t *data, int width, int height, 
                           uint32_t r_mask, uint32_t g_mask, uint32_t b_mask, uint32_t a_mask, 
                           int bpp);

// Updated header structures
#pragma pack(push, 1)
typedef struct {
    char     signature[2];
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;
} BMPFileHeader;

typedef struct {
    uint32_t size;
    // Fields vary after this point
} GenericDIBHeader;
#pragma pack(pop)

int main(int argc, char *argv[]) {
    FILE *in = NULL;
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    in = fopen(argv[1], "rb");
    if (!in) {
        printf("Error: Could not open file.\n");
        return 1;
    }

    BMPFileHeader fileHeader;
    fread(&fileHeader, sizeof(BMPFileHeader), 1, in);

    uint32_t dibSize;
    fread(&dibSize, sizeof(uint32_t), 1, in);
    fseek(in, -4, SEEK_CUR); // Rewind to start of DIB header

    int32_t width, height;
    uint16_t bpp;
    uint32_t compression = 0; // OS/2 v1 is always uncompressed
    uint32_t colors_used = 0;

    if (dibSize == 12) {
        // OS/2 BMPv1 / BITMAPCOREHEADER
        struct {
            uint32_t size;
            uint16_t width;
            uint16_t height;
            uint16_t planes;
            uint16_t bit_count;
        } core;
        fread(&core, sizeof(core), 1, in);
        width = core.width;
        height = core.height;
        bpp = core.bit_count;
        printf("Detected OS/2 BMPv1 (Core Header)\n");
    } else {
        // Windows BMPv3+ (Your existing logic)
        struct {
            uint32_t size;
            int32_t  width;
            int32_t  height;
            uint16_t planes;
            uint16_t bits_per_pixel;
            uint32_t compression;
            uint32_t image_size;
            int32_t  x_pixels_per_m;
            int32_t  y_pixels_per_m;
            uint32_t colors_used;
            uint32_t important_colors;
        } win;
        fread(&win, sizeof(win), 1, in);
        width = win.width;
        height = win.height;
        bpp = win.bits_per_pixel;
        compression = win.compression;
        colors_used = win.colors_used;
        printf("Detected Windows BMP\n");
    }

    // --- Bitfield Handling ---
    uint32_t r_mask = 0, g_mask = 0, b_mask = 0, a_mask = 0;
    if (compression == 3) {
        fseek(in, 14 + dibSize, SEEK_SET);
        fread(&r_mask, 4, 1, in);
        fread(&g_mask, 4, 1, in);
        fread(&b_mask, 4, 1, in);
        if (dibSize >= 56) fread(&a_mask, 4, 1, in);
    }

    // --- Pixel Data Loading ---
    int stride = ((width * bpp + 31) / 32) * 4;
    int data_size = stride * abs(height);
    uint8_t *data = (uint8_t*)malloc(data_size);
    fseek(in, fileHeader.dataOffset, SEEK_SET);
    fread(data, 1, data_size, in);

    // --- Bitfield conversion ---
    if (compression == 3 && (bpp == 16 || bpp == 32)) {
        data = bitfields_to_bgra(data, width, abs(height), r_mask, g_mask, b_mask, a_mask, bpp);
        bpp = 32;
        compression = 0;
    }

    uint8_t *final_raw_data = data;
    int current_bpp = bpp;

    // --- RLE handling ---
    if (compression == 1 || compression == 2) {
        final_raw_data = decompress_rle(data, data_size, width, abs(height), current_bpp);
        free(data);
        current_bpp = 8;
    }

    // --- Palette handling (The tricky part for OS/2) ---
    if (current_bpp <= 8) {
        int num_colors = (colors_used > 0) ? colors_used : (1 << bpp);
        int entry_size = (dibSize == 12) ? 3 : 4; // OS/2 uses 3-byte RGB, Windows uses 4-byte RGBA
        
        uint8_t *raw_palette = (uint8_t*)malloc(num_colors * entry_size);
        fseek(in, 14 + dibSize, SEEK_SET);
        fread(raw_palette, 1, num_colors * entry_size, in);

        // Standardize palette to 4-byte BGR0 for apply_palette
        uint8_t *std_palette = (uint8_t*)malloc(num_colors * 4);
        for(int i=0; i < num_colors; i++) {
            if (dibSize == 12) {
                std_palette[i*4 + 0] = raw_palette[i*3 + 0]; // B
                std_palette[i*4 + 1] = raw_palette[i*3 + 1]; // G
                std_palette[i*4 + 2] = raw_palette[i*3 + 2]; // R
                std_palette[i*4 + 3] = 255;                  // A
            } else {
                std_palette[i*4 + 0] = raw_palette[i*4 + 0];
                std_palette[i*4 + 1] = raw_palette[i*4 + 1];
                std_palette[i*4 + 2] = raw_palette[i*4 + 2];
                std_palette[i*4 + 3] = raw_palette[i*4 + 3];
            }
        }

        uint8_t *converted_data = apply_palette(std_palette, final_raw_data, width, abs(height), current_bpp, compression);
        
        free(final_raw_data);
        free(raw_palette);
        free(std_palette);
        
        data = converted_data;
        bpp = 32;
    }

    render_bmp(data, width, height, bpp, compression);
    
    free(data);
    fclose(in);
    return 0;
}

static int count_trailing_zeros(uint32_t mask)
{
    int shift = 0;
    if (mask == 0) return 0;

    while ((mask & 1) == 0) {
        mask >>= 1;
        shift++;
    }
    return shift;
}

static int count_bits(uint32_t mask)
{
    int count = 0;
    while (mask) {
        count += mask & 1;
        mask >>= 1;
    }
    return count;
}

static uint8_t normalize_to_8bit(uint32_t value, int bits)
{
    if (bits == 0) return 0;

    if (bits >= 8)
        return (uint8_t)(value >> (bits - 8));
    else
        return (uint8_t)((value * 255) / ((1 << bits) - 1));
}

uint8_t* bitfields_to_bgra(uint8_t *data, int width, int height, 
                           uint32_t r_mask, uint32_t g_mask, uint32_t b_mask, uint32_t a_mask, 
                           int bpp) 
{
    int r_shift = count_trailing_zeros(r_mask);
    int g_shift = count_trailing_zeros(g_mask);
    int b_shift = count_trailing_zeros(b_mask);
    int a_shift = (a_mask) ? count_trailing_zeros(a_mask) : 0;

    int r_bits = count_bits(r_mask);
    int g_bits = count_bits(g_mask);
    int b_bits = count_bits(b_mask);
    int a_bits = (a_mask) ? count_bits(a_mask) : 0;

    // Calculate the input stride (with padding)
    int in_stride = ((width * bpp + 31) / 32) * 4;
    uint8_t *out_data = (uint8_t*)malloc(width * height * 4);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t pixel = 0;

            if (bpp == 16) {
                // Read from the padded input buffer
                uint8_t *pixel_ptr = &data[y * in_stride + x * 2];
                pixel = pixel_ptr[0] | (pixel_ptr[1] << 8);
            } else {
                uint8_t *pixel_ptr = &data[y * in_stride + x * 4];
                pixel = pixel_ptr[0] | (pixel_ptr[1] << 8) | (pixel_ptr[2] << 16) | (pixel_ptr[3] << 24);
            }

            uint32_t r_raw = (pixel & r_mask) >> r_shift;
            uint32_t g_raw = (pixel & g_mask) >> g_shift;
            uint32_t b_raw = (pixel & b_mask) >> b_shift;
            uint32_t a_raw = (a_mask) ? ((pixel & a_mask) >> a_shift) : 0;

            // Pack into 32-bit output (no padding needed for standard 32-bit RGBA in SDL)
            int out_idx = (y * width + x) * 4;
            out_data[out_idx + 0] = normalize_to_8bit(b_raw, b_bits);
            out_data[out_idx + 1] = normalize_to_8bit(g_raw, g_bits);
            out_data[out_idx + 2] = normalize_to_8bit(r_raw, r_bits);
            out_data[out_idx + 3] = (a_mask) ? normalize_to_8bit(a_raw, a_bits) : 255;
        }
    }

    free(data);
    return out_data;
}

uint8_t* decompress_rle(uint8_t *in_buf, uint32_t in_size, int width, int height, int bpp) {
    uint8_t *out_buf = (uint8_t*)calloc(width * height, 1);
    if (!out_buf) return NULL;

    int x = 0;
    int y = 0; 
    uint32_t i = 0;

    while (i < in_size && y < height) {
        uint8_t count = in_buf[i++];
        if (i >= in_size) break;
        uint8_t cmd = in_buf[i++];

        if (count == 0) {
            // ESCAPE CODES
            if (cmd == 0) { 
                // End of line
                x = 0;
                y++;
            } else if (cmd == 1) { 
                // End of bitmap
                break;
            } else if (cmd == 2) { 
                // Delta: skip next X and Y pixels
                if (i + 1 >= in_size) break;
                x += in_buf[i++];
                y += in_buf[i++]; 
            } else {
                int pixels_to_read = cmd;
                
                if (bpp == 8) {
                    for (int p = 0; p < pixels_to_read && x < width; p++) {
                        out_buf[y * width + x++] = in_buf[i++];
                    }
                    if (pixels_to_read % 2 != 0) i++; 
                } 
                else if (bpp == 4) {
                    int bytes_to_read = (pixels_to_read + 1) / 2;
                    for (int p = 0; p < pixels_to_read && x < width; p++) {
                        uint8_t val = in_buf[i + (p / 2)];
                        if (p % 2 == 0) {
                            out_buf[y * width + x++] = (val >> 4) & 0x0F;
                        } else {
                            out_buf[y * width + x++] = val & 0x0F;
                        }
                    }
                    i += bytes_to_read;
                    if (bytes_to_read % 2 != 0) i++;
                }
            }
        } else {
            if (bpp == 8) {
                for (int p = 0; p < count && x < width; p++) {
                    out_buf[y * width + x++] = cmd;
                }
            } 
            else if (bpp == 4) {
                uint8_t high_nibble = (cmd >> 4) & 0x0F;
                uint8_t low_nibble = cmd & 0x0F;
                for (int p = 0; p < count && x < width; p++) {
                    out_buf[y * width + x++] = (p % 2 == 0) ? high_nibble : low_nibble;
                }
            }
        }
    }
    return out_buf;
}

uint8_t* apply_palette(uint8_t *palette, uint8_t *old_data, int width, int height, int bpp, int is_rle) {
        int old_stride;
        if (is_rle)
            old_stride = width;
        else
            old_stride = ((width * bpp + 31) / 32) * 4;

    int new_stride = width * 4; // converting to 32-bit (4 bytes per pixel)
    
    int new_data_size = new_stride * height;
    uint8_t *new_data = (uint8_t*)malloc(new_data_size);
    if (!new_data) return NULL;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t palette_index = 0;
            
            if (bpp == 8) {
                // 1 byte = 1 pixel
                palette_index = old_data[y * old_stride + x];
            } 
            else if (bpp == 4) {
                // 1 byte = 2 pixels.
                uint8_t byte = old_data[y * old_stride + (x / 2)];
                if (x % 2 == 0) {
                    palette_index = (byte >> 4) & 0x0F; // First pixel is the high 4 bits
                } else {
                    palette_index = byte & 0x0F;        // Second pixel is the low 4 bits
                }
            }
            else if (bpp == 1) {
                // 1 byte = 8 pixels
                uint8_t byte = old_data[y * old_stride + (x / 8)];

                int bit = 7 - (x % 8);            // MSB first
                palette_index = (byte >> bit) & 1;
            }

            int p_idx = palette_index * 4;
            
            // Calculate where to write in the new 32-bit buffer
            int out_idx = y * new_stride + (x * 4);
            
            new_data[out_idx + 0] = palette[p_idx + 0]; // Blue
            new_data[out_idx + 1] = palette[p_idx + 1]; // Green
            new_data[out_idx + 2] = palette[p_idx + 2]; // Red
            new_data[out_idx + 3] = 255;                // Alpha (Force opaque)
        }
    }

    return new_data;
}

void render_bmp(uint8_t *pixels, int width, int height, int bpp, int compression) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    int abs_h = abs(height);
    int stride = ((width * bpp + 31) / 32) * 4;

    Uint32 format;
    if (bpp == 24)      format = SDL_PIXELFORMAT_BGR24;
    else if (bpp == 32) format = SDL_PIXELFORMAT_BGRA32;
    else if (bpp == 16) format = SDL_PIXELFORMAT_BGR555;
    else {
        printf("Unsupported BPP: %d\n", bpp);
        SDL_Quit();
        return;
    }

    // Scale dowm large images
    int win_w = width  < 800 ? width  : 800;
    int win_h = abs_h < 600 ? abs_h : 600;

    SDL_Window *window = SDL_CreateWindow(
        "BMP Viewer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_RESIZABLE
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        format,
        SDL_TEXTUREACCESS_STATIC,
        width,
        abs_h
    );

    SDL_UpdateTexture(texture, NULL, pixels, stride);

    int running = 1;
    SDL_Event e;

    while (running) {

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
        }

        int w, h;
        SDL_GetWindowSize(window, &w, &h);

        float sx = (float)w / width;
        float sy = (float)h / abs_h;
        float scale = sx < sy ? sx : sy;

        SDL_Rect dst;
        dst.w = width * scale;
        dst.h = abs_h * scale;
        dst.x = (w - dst.w) / 2;
        dst.y = (h - dst.h) / 2;

        SDL_RenderClear(renderer);

        if (height > 0) {
            SDL_RenderCopyEx(renderer, texture, NULL, &dst, 0, NULL, SDL_FLIP_VERTICAL);
        } else {
            SDL_RenderCopy(renderer, texture, NULL, &dst);
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}