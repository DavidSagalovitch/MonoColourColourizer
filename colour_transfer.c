
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_write.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    float r, g, b;
} Pixel;

typedef struct {
    int width, height;
    Pixel* data;
} Image;

float clamp(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

// RGB to Lab conversion helpers
void rgb_to_xyz(float r, float g, float b, float* x, float* y, float* z) {
    r = (r > 0.04045f) ? powf((r + 0.055f) / 1.055f, 2.4f) : r / 12.92f;
    g = (g > 0.04045f) ? powf((g + 0.055f) / 1.055f, 2.4f) : g / 12.92f;
    b = (b > 0.04045f) ? powf((b + 0.055f) / 1.055f, 2.4f) : b / 12.92f;

    *x = r * 0.4124f + g * 0.3576f + b * 0.1805f;
    *y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
    *z = r * 0.0193f + g * 0.1192f + b * 0.9505f;
}

float fxyz(float t) {
    return (t > 0.008856f) ? powf(t, 1.0f / 3.0f) : (7.787f * t + 16.0f / 116.0f);
}

void xyz_to_lab(float x, float y, float z, float* l, float* a, float* b) {
    x /= 0.95047f;
    y /= 1.00000f;
    z /= 1.08883f;

    float fx = fxyz(x);
    float fy = fxyz(y);
    float fz = fxyz(z);

    if (l) *l = 116.0f * fy - 16.0f;
    if (a) *a = 500.0f * (fx - fy);
    if (b) *b = 200.0f * (fy - fz);    

}

void rgb_to_lab(Pixel p, float* l, float* a, float* b) {
    float x, y, z;
    rgb_to_xyz(p.r, p.g, p.b, &x, &y, &z);
    xyz_to_lab(x, y, z, l, a, b);
}

Pixel lab_to_rgb(float l, float a, float b) {
    float fy = (l + 16.0f) / 116.0f;
    float fx = a / 500.0f + fy;
    float fz = fy - b / 200.0f;

    float x = (powf(fx, 3.0f) > 0.008856f) ? powf(fx, 3.0f) : (fx - 16.0f / 116.0f) / 7.787f;
    float y = (powf(fy, 3.0f) > 0.008856f) ? powf(fy, 3.0f) : (fy - 16.0f / 116.0f) / 7.787f;
    float z = (powf(fz, 3.0f) > 0.008856f) ? powf(fz, 3.0f) : (fz - 16.0f / 116.0f) / 7.787f;

    x *= 0.95047f;
    y *= 1.00000f;
    z *= 1.08883f;

    float r = x *  3.2406f + y * -1.5372f + z * -0.4986f;
    float g = x * -0.9689f + y *  1.8758f + z *  0.0415f;
    float b_ = x *  0.0557f + y * -0.2040f + z *  1.0570f;

    r = (r > 0.0031308f) ? (1.055f * powf(r, 1.0f / 2.4f) - 0.055f) : 12.92f * r;
    g = (g > 0.0031308f) ? (1.055f * powf(g, 1.0f / 2.4f) - 0.055f) : 12.92f * g;
    b_ = (b_ > 0.0031308f) ? (1.055f * powf(b_, 1.0f / 2.4f) - 0.055f) : 12.92f * b_;

    Pixel p = { clamp(r, 0, 1), clamp(g, 0, 1), clamp(b_, 0, 1) };
    return p;
}

void compute_mean_std(float* values, int count, float* mean, float* stddev) {
    float sum = 0.0f;
    for (int i = 0; i < count; i++) sum += values[i];
    *mean = sum / count;

    float variance = 0.0f;
    for (int i = 0; i < count; i++) {
        float diff = values[i] - *mean;
        variance += diff * diff;
    }
    *stddev = sqrtf(variance / count);
}

int main() {
    printf("Starting program...\n");
    int w1 = 0, h1 = 0, c1 = 0;
    int w2 = 0, h2 = 0, c2 = 0;
    
    printf("Loading grayscale.jpg...\n");
    unsigned char* gray_data_raw = stbi_load("mandril_gray.jpg", &w1, &h1, &c1, 1);
    if (!gray_data_raw) {
        printf("ERROR: Failed to load grayscale.jpg\n");
        system("pause");
        return 1;
    }
    printf("Grayscale image loaded: %dx%d, %d channels\n", w1, h1, c1);
    
    // Convert grayscale to RGB manually
    unsigned char* gray_data = malloc(w1 * h1 * 3);
    for (int i = 0; i < w1 * h1; i++) {
        gray_data[i*3 + 0] = gray_data_raw[i];
        gray_data[i*3 + 1] = gray_data_raw[i];
        gray_data[i*3 + 2] = gray_data_raw[i];
    }
    stbi_image_free(gray_data_raw);

    printf("Loading reference.jpg...\n");
    unsigned char* ref_data = stbi_load("mandril_color.jpg", &w2, &h2, &c2, 3);
    if (!ref_data) {
        printf("ERROR: Failed to load reference.jpg\n");
        system("pause");
        return 1;
    }
    printf("Reference image loaded: %dx%d, %d channels\n", w2, h2, c2);
    

    int count = w1 * h1;
    float *lab_l = malloc(sizeof(float) * count);
    float *lab_a = malloc(sizeof(float) * count);
    float *lab_b = malloc(sizeof(float) * count);
    float *ref_a = malloc(sizeof(float) * count);
    float *ref_b = malloc(sizeof(float) * count);

    for (int i = 0; i < count; i++) {
        int idx = i * 3;
        Pixel p_gray = { gray_data[idx]/255.0f, gray_data[idx+1]/255.0f, gray_data[idx+2]/255.0f };
        Pixel p_ref  = { ref_data[idx]/255.0f,  ref_data[idx+1]/255.0f,  ref_data[idx+2]/255.0f };

        rgb_to_lab(p_gray, &lab_l[i], &lab_a[i], &lab_b[i]);
        rgb_to_lab(p_ref, NULL, &ref_a[i], &ref_b[i]);
    }

    float mean_a_src, std_a_src, mean_b_src, std_b_src;
    float mean_a_ref, std_a_ref, mean_b_ref, std_b_ref;

    compute_mean_std(lab_a, count, &mean_a_src, &std_a_src);
    compute_mean_std(lab_b, count, &mean_b_src, &std_b_src);
    compute_mean_std(ref_a, count, &mean_a_ref, &std_a_ref);
    compute_mean_std(ref_b, count, &mean_b_ref, &std_b_ref);

    unsigned char* result = malloc(count * 3);
    for (int i = 0; i < count; i++) {
        float new_a = (std_a_src != 0) ? ((lab_a[i] - mean_a_src) * (std_a_ref / std_a_src) + mean_a_ref) : mean_a_ref;
        float new_b = (std_b_src != 0) ? ((lab_b[i] - mean_b_src) * (std_b_ref / std_b_src) + mean_b_ref) : mean_b_ref;        
        float clamped_a = fmaxf(fminf(new_a, 127), -127);
        float clamped_b = fmaxf(fminf(new_b, 127), -127);
        Pixel p = lab_to_rgb(lab_l[i], clamped_a, clamped_b);
        result[i * 3 + 0] = (unsigned char)(p.r * 255);
        result[i * 3 + 1] = (unsigned char)(p.g * 255);
        result[i * 3 + 2] = (unsigned char)(p.b * 255);
    }

    stbi_write_jpg("colorized_output.jpg", w1, h1, 3, result, w1 * 3);
    printf("Colorized image saved as 'colorized_output.jpg'\n");

    free(gray_data);
    stbi_image_free(ref_data);
    free(lab_l); free(lab_a); free(lab_b); free(ref_a); free(ref_b); free(result);
    return 0;
}
