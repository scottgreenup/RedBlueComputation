
#include <assert.h>
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static struct argp_option options[] = {
    {"gridsize",  'n', "grid_size", 0, "Size of the grid."},
    {"tilesize",  't', "tile_size", 0, "Size of the tile."},
    {"threshold", 'c', "threshold", 0, "The threshold."},
    {"max_iters", 'm', "max_iters", 0, "Max iterations."},
    {0}
};

struct arguments {
    uint32_t grid_size;
    uint32_t tile_size;
    uint32_t threshold;
    uint32_t max_iters;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;
    switch (key) {
        case 'n': args->grid_size = atoi(arg); break;
        case 't': args->tile_size = atoi(arg); break;
        case 'c': args->threshold = atoi(arg); break;
        case 'm': args->max_iters = atoi(arg); break;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, "", "", 0, 0, 0};

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

enum cell_type {
    BLUE,
    RED,
    WHITE
};

struct grid_t {
    enum cell_type** elements;
    uint32_t size;
};

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
    for (uint32_t r = 0; r < self->size; r++) {
        for (uint32_t c = 0; c < self->size; c++) {
            uint32_t tr = (int)(r / tile_size);
            uint32_t tc = (int)(c / tile_size);

            switch (self->elements[r][c]) {
            case BLUE:
                b_tiles[tr][tc]++;
                ratio = b_tiles[tr][tc] / (double)(tile_size * tile_size);
                if (ratio >= delta) {
                    fprintf(stderr, "Finished.\n");
                    fprintf(
                        stderr,
                        "Tile (r=%d, c=%d) has ratio %lf\% blue, above the %d\% threshold.\n",
                        tr,
                        tc,
                        ratio * 100.0,
                        threshold);
                    return true;
                }
                break;
            case RED:
                r_tiles[tr][tc]++;
                ratio = r_tiles[tr][tc] / (double)(tile_size * tile_size);
                if (ratio >= delta) {
                    fprintf(stderr, "Finished.\n");
                    fprintf(
                        stderr,
                        "Tile (r=%d, c=%d) has ratio %lf\% red, above the %d\% threshold.\n",
                        tr,
                        tc,
                        ratio * 100.0,
                        threshold);
                    return true;
                }
                break;
            }
        }
    }

    return false;
}

int main(int argc, char** argv) {

    struct arguments args;
    args.grid_size = 0;
    args.tile_size = 0;
    args.threshold = 0;
    args.max_iters = 0;
    argp_parse(&argp, argc, argv, 0, 0, &args);

    // Initialize the grid with blue, red, and white cells
    srand(time(NULL));
    struct grid_t grid_curr;
    grid_init(&grid_curr, args.grid_size);

    // Create 'immutable' copy to refer to when checking
    struct grid_t grid_prev;
    grid_init_copy(&grid_prev, &grid_curr);
    grid_print(&grid_curr, args.tile_size);

    uint32_t iterations = 0;
    bool finished = false;
    while (iterations < args.max_iters && !finished) {

        // RED movement -- red can move right
        for (uint32_t r = 0; r < args.grid_size; r++) {
            for (uint32_t c = 0; c < args.grid_size; c++) {
                if (grid_prev.elements[r][c] != RED) {
                    continue;
                }

                uint32_t next = (c+1) % args.grid_size;
                if (grid_prev.elements[r][next] != WHITE) {
                    continue;
                }

                grid_curr.elements[r][c] = WHITE;
                grid_curr.elements[r][next] = RED;
            }
        }

        grid_copy(&grid_prev, &grid_curr);

        // BLUE movement -- blue can move down
        for (uint32_t r = 0; r < args.grid_size; r++) {
            for (uint32_t c = 0; c < args.grid_size; c++) {
                if (grid_prev.elements[r][c] != BLUE) {
                    continue;
                }

                uint32_t next = (r+1) % args.grid_size;
                if (grid_prev.elements[next][c] != WHITE) {
                    continue;
                }

                grid_curr.elements[r][c] = WHITE;
                grid_curr.elements[next][c] = BLUE;
            }
        }

        grid_print(&grid_curr, args.tile_size);
        grid_copy(&grid_prev, &grid_curr);

        finished = grid_check_tiles(&grid_curr, args.tile_size, args.threshold);

        iterations++;
    }

    fprintf(stderr, "Performed %d iterations.\n", iterations);

    return 0;
}
