#include "grid.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// Inclusive min and inclusive of max.
uint32_t rand_range(uint32_t min, uint32_t max) {
    uint32_t range = max - min;
    uint32_t error_range = RAND_MAX - RAND_MAX % range;
    uint32_t r = rand();
    while (r > error_range) {
        r = rand();
    }
    return (min + (r % (max + 1)));
}

void grid_init(struct grid_t *self, uint32_t size) {
    self->size = size;
    self->elements = (enum cell_type**)malloc(size * sizeof(enum cell_type*));
    for (uint32_t r = 0; r < size; r++) {
        self->elements[r] = (enum cell_type*)malloc(size * sizeof(enum cell_type));
        for (uint32_t c = 0; c < size; c++) {
            self->elements[r][c] = (enum cell_type)(rand_range(0, 2));
        }
    }
}

void grid_init_copy(struct grid_t *self, const struct grid_t* copy) {
    self->size = copy->size;
    self->elements = (enum cell_type**)malloc(copy->size * sizeof(enum cell_type*));
    for (uint32_t r = 0; r < copy->size; r++) {
        self->elements[r] = (enum cell_type*)malloc(copy->size * sizeof(enum cell_type));
        for (uint32_t c = 0; c < copy->size; c++) {
            self->elements[r][c] = copy->elements[r][c];
        }
    }
}

void grid_copy(struct grid_t *self, const struct grid_t* source) {
    for (uint32_t r = 0; r < self->size; r++) {
        for (uint32_t c = 0; c < self->size; c++) {
            self->elements[r][c] = source->elements[r][c];
        }
    }
}


void grid_print_line(const struct grid_t *self, uint32_t tile_size) {
    fprintf(stderr, "+");
    for (uint32_t i = 0; i < self->size; i++) {
        if (i < self->size - 1) {
            if ((i % tile_size) == tile_size - 1) {
                fprintf(stderr, "-+");
            } else {
                fprintf(stderr, "--");
            }
        } else {
            fprintf(stderr, "-");
        }
    }
    fprintf(stderr, "+");
    fprintf(stderr, "\n");
}

void grid_print(const struct grid_t *self, uint32_t tile_size) {
    grid_print_line(self, tile_size);

    for (uint32_t r = 0; r < self->size; r++) {
        for (uint32_t c = 0; c < self->size; c++) {
            if (c == 0) {
                fprintf(stderr, "|");
            }

            switch (self->elements[r][c]) {
            case RED:
                fprintf(stderr, ">");
                break;
            case BLUE:
                fprintf(stderr, "v");
                break;
            case WHITE:
                fprintf(stderr, "-");
                break;
            }

            if (c < self->size - 1) {
                if ((c % tile_size) == tile_size - 1) {
                    fprintf(stderr, "|");
                } else {
                    fprintf(stderr, " ");
                }
            }

        }
        fprintf(stderr, "|\n");
        if (r < (self->size - 1) && (r % tile_size) == tile_size - 1) {
            grid_print_line(self, tile_size);
        }
    }

    grid_print_line(self, tile_size);
    fprintf(stderr, "\n");
}

bool grid_check_tiles(
    const struct grid_t *self,
    uint32_t tile_size,
    uint32_t threshold
) {
    assert(self->size % tile_size == 0);

    double delta = threshold / 100.0;

    uint32_t t_len = self->size / tile_size;
    uint32_t b_tiles[t_len][t_len];
    uint32_t r_tiles[t_len][t_len];

    for (uint32_t r = 0; r < t_len; r++) {
        for (uint32_t c = 0; c < t_len; c++) {
            b_tiles[r][c] = 0;
            r_tiles[r][c] = 0;
        }
    }

    double ratio = 0.0;

    bool completed = false;
    for (uint32_t r = 0; r < self->size; r++) {
        for (uint32_t c = 0; c < self->size; c++) {
            uint32_t tr = (int)(r / tile_size);
            uint32_t tc = (int)(c / tile_size);

            switch (self->elements[r][c]) {
            case BLUE:
                b_tiles[tr][tc]++;
                ratio = b_tiles[tr][tc] / (double)(tile_size * tile_size);
                if (ratio >= delta) {
                    fprintf(
                        stderr,
                        "Tile (c=%d, r=%d) has %f%% BLUE\n",
                        tc,
                        tr,
                        ratio * 100.0,
                        threshold);
                    completed = true;
                }
                break;
            case RED:
                r_tiles[tr][tc]++;
                ratio = r_tiles[tr][tc] / (double)(tile_size * tile_size);
                if (ratio >= delta) {
                    fprintf(
                        stderr,
                        "Tile (c=%d, r=%d) has %f%% RED\n",
                        tc,
                        tr,
                        ratio * 100.0,
                        threshold);
                    completed = true;
                }
                break;
            case WHITE:
                break;
            }
        }
    }

    return completed;
}
