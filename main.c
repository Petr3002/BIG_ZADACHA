#include "lodepng.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_COMPONENTS 1000
struct pixel {
    unsigned char R;
    unsigned char G;
    unsigned char B;
    unsigned char alpha;
};

void save_png_file(const char *filename, struct pixel *pixels, int width, int height) {
    unsigned error = lodepng_encode32_file(filename, (unsigned char *) pixels, width, height);
    if (error) {
        printf("error %u: %s\\n", error, lodepng_error_text(error));
    }
}

int i, j;

char *picture_download(const char *filename, int *width, int *height) {
    unsigned char *image = NULL;
    int error = lodepng_decode32_file(&image, width, height, filename);
    if (error) {
        printf("error %u: %s\n", error, lodepng_error_text(error));
        return NULL;
    }
    return (image);
}

typedef struct Node {
    unsigned char r, g, b, a;
    struct Node *up, *down, *left, *right;
    struct Node *parent;
    int rank;
} Node;

Node *find_root(Node *nodes, Node *x) {
    if (x->parent != x) {
        x->parent = find_root(nodes, x->parent);
    }
    return x->parent;
}

void free_image_graph(Node *nodes) {
    free(nodes);
}

void sobel(const char *filename) {
    int w = 0, h = 0;
    unsigned char *picture = picture_download(filename, &w, &h);

    if (picture == NULL) {
        printf("I can't read the picture %s. \n", filename);
        return;
    }
    struct pixel *sobel_image = (struct pixel *) malloc(w * h * sizeof(struct pixel));

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            // sobel
            int gx = 0, gy = 0;
            // gradient X
            int k = 4, m = 1;
            gx += (picture[(y - 1) * w * k + (x + 1) * k] - picture[(y - 1) * w * k + (x - 1) * k]) * m;
            gx += (picture[y * w * k + (x + 1) * k] - picture[y * w * k + (x - 1) * k]) * 2 * m;
            gx += (picture[(y + 1) * w * k + (x + 1) * k] - picture[(y + 1) * w * k + (x - 1) * k]) * m;
            // gradient Y
            gy += (picture[(y + 1) * w * k + (x - 1) * k] - picture[(y - 1) * w * k + (x - 1) * k]) * m;
            gy += (picture[(y + 1) * w * k + x * k] - picture[(y - 1) * w * k + x * k]) * 2 * m;
            gy += (picture[(y + 1) * w * k + (x + 1) * k] - picture[(y - 1) * w * k + (x + 1) * k]) * m;
            int magnitude = (int) sqrt(gx * gx + gy * gy);
            magnitude = fmin(fmax(magnitude, 0), 255);
            sobel_image[y * w + x].R = magnitude;
            sobel_image[y * w + x].G = magnitude;
            sobel_image[y * w + x].B = magnitude;
            sobel_image[y * w + x].alpha = 255;
        }
    }
    save_png_file("C:\\Users\\Public\\sobel.png", sobel_image, w, h);

    free(picture);
    free(sobel_image);
}

Node *create_image_graph(const char *filename, int *width, int *height) {
    unsigned char *image = NULL;
    int error = lodepng_decode32_file(&image, width, height, filename);
    if (error) {
        printf("error %u: %s\\n", error, lodepng_error_text(error));
        return NULL;
    }

    Node *nodes = malloc(*width * *height * sizeof(Node));
    if (!nodes) {
        free(image);
        return NULL;
    }

    for (unsigned y = 0; y < *height; ++y) {
        for (unsigned x = 0; x < *width; ++x) {
            Node *node = &nodes[y * *width + x];
            unsigned char *pixel = &image[(y * *width + x) * 4];
            node->r = pixel[0];
            node->g = pixel[1];
            node->b = pixel[2];
            node->a = pixel[3];
            node->up = y > 0 ? &nodes[(y - 1) * *width + x] : NULL;
            node->down = y < *height - 1 ? &nodes[(y + 1) * *width + x] : NULL;
            node->left = x > 0 ? &nodes[y * *width + (x - 1)] : NULL;
            node->right = x < *width - 1 ? &nodes[y * *width + (x + 1)] : NULL;
            node->parent = node;
            node->rank = 0;
        }
    }

    free(image);
    return nodes;
}

void black_white(const char *filename) {
    int w = 0, h = 0;
    int i;
    unsigned char *picture = picture_download(filename, &w, &h);
    if (picture == NULL) {
        printf("I can't read the picture %s. Error.\n", filename);
        return;
    }
    struct pixel *grayscale_image = (struct pixel *) malloc(w * h * sizeof(struct pixel));
    for (i = 0; i < h * w; i++) {
        unsigned char grayscale_value = (unsigned char) ((picture[i * 4] + picture[i * 4 + 1] + picture[i * 4 + 2]) /
                                                         3);
        grayscale_image[i].R = grayscale_value;
        grayscale_image[i].G = grayscale_value;
        grayscale_image[i].B = grayscale_value;
        grayscale_image[i].alpha = picture[i * 4 + 3];
    }
    save_png_file("C:\\Users\\Public\\black_white.png", grayscale_image, w, h);

    free(picture);
    free(grayscale_image);
}

void union_sets(Node *nodes, Node *x, Node *y, double e) {
//    if (x->r < 40 && y->r < 40) {
//        return;
//    }
    Node *px = find_root(nodes, x);
    Node *py = find_root(nodes, y);

    double color_difference = sqrt(pow(x->r - y->r, 2) + pow(x->g - y->g, 2) + pow(x->b - y->b, 2));
    if (px != py && color_difference < e) {
        if (px->rank > py->rank) {
            py->parent = px;
        } else {
            px->parent = py;
            if (px->rank == py->rank) {
                py->rank++;
            }
        }
    }
}



void segments(Node *nodes, int width, int height, double e) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Node *node = &nodes[y * width + x];
            if (node->up) {
                union_sets(nodes, node, node->up, e);
            }
            if (node->down) {
                union_sets(nodes, node, node->down, e);
            }
            if (node->left) {
                union_sets(nodes, node, node->left, e);
            }
            if (node->right) {
                union_sets(nodes, node, node->right, e);


            }
        }
    }
}

void colouring_components(Node *nodes, int width, int height) {
    unsigned char *output_image = malloc(width * height * 4 * sizeof(unsigned char));
    int *component_sizes = calloc(width * height, sizeof(int));
    int total_components = 0;

    srand(time(NULL));
    for (int i = 0; i < width * height; i++) {
        Node *p = find_root(nodes, &nodes[i]);
        if (p == &nodes[i]) {
            if (p->r > 150) {
                p->r = 255;
                p->g = 255;
                p->b = 255;
            }
            else if (p->r < 5) {
                p->r = 0;
                p->g = 0;
                p->b = 0;
            }
            else if (component_sizes[i] < 4) {
                p->r = 0;
                p->g = 0;
                p->b = 0;
            } else {
                p->r = rand() % 256;
                p->g = rand() % 256;
                p->b = rand() % 256;
            }

        }
        output_image[4 * i + 0] = p->r;
        output_image[4 * i + 1] = p->g;
        output_image[4 * i + 2] = p->b;
        output_image[4 * i + 3] = 255;
        component_sizes[p - nodes]++;
    }

    char *output_filename = "C:\\Users\\Public\\result.png";
    lodepng_encode32_file(output_filename, output_image, width, height);


    free(output_image);
    free(component_sizes);
}

int main() {
    black_white("C:\\Users\\Public\\start.png");
    sobel("C:\\Users\\Public\\black_white.png");

    int width, height;
    char *filename = "C:\\Users\\Public\\sobel.png";
    Node *nodes = create_image_graph(filename, &width, &height);
    if (!nodes) {
        return 1;
    }

    double e = 17.0;
    segments(nodes, width, height, e);
    colouring_components(nodes, width, height);

    free_image_graph(nodes);

    return 0;
}