#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static constexpr int M = 1536;
static int grid[M][M];


typedef struct {
    int x, y;
} point;


int main(int argc, const char* argv[]) {
    FILE* fp = NULL;
    if (argc > 1) {
        const char* fn = argv[1];
        if (strcmp(fn, "-") == 0) {
            fp = stdin;
        }
        else {
            fp = fopen(fn, "r");
            if (fp == NULL) {
                perror(fn);
                return 1;
            }
        }
    }

    size_t width = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) { break; }
        if (c == '\n') {
            break;
        }
        else {
            width += 1;
        }
    }
    if (width == 0) {
        fprintf(stderr, "! problem not found\n");
        return 1;
    }
    else {
        fseek(fp, -(int)(width+1), SEEK_CUR);
    }

    size_t pois_size = width * width;
    point* pois = (point*) calloc(pois_size, sizeof(point));
    size_t poi_count = 0;

    point pos = { 0, 0 };
    point start = pos;

    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) { break; }
        switch (c) {
            case 'L':
                start = pos;
                if (poi_count + 1 >= pois_size) {
                    fprintf(stderr, "! poi limit\n");
                    return 1;
                }
                else {
                    pois[poi_count++] = pos;
                }
                pos.x += 1;
                break;
            case '.':
                if (poi_count + 1 >= pois_size) {
                    fprintf(stderr, "! poi limit\n");
                    return 1;
                }
                else {
                    pois[poi_count++] = pos;
                }
                pos.x += 1;
                break;
            case '\n':
                pos.y += 1;
                pos.x = 0;
                break;
            default:
                pos.x += 1;
                break;
        }
    }

    if (poi_count == 0) {
        fprintf(stderr, "! empty problem\n");
        return 1;
    }
    else {
        fprintf(stderr, "%zu pills + L\n", poi_count-1);
    }

    size_t adj_size = poi_count * poi_count;
    int* adj = (int*) calloc(adj_size, sizeof(int));

    return 0;
}
