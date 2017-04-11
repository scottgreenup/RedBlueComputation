
#include <assert.h>
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "grid.h"

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

int main(int argc, char** argv) {

    struct arguments args;
    args.grid_size = 0;
    args.tile_size = 0;
    args.threshold = 0;
    args.max_iters = 0;
    argp_parse(&argp, argc, argv, 0, 0, &args);

    assert(args.grid_size > 0);
    assert(args.tile_size > 0);
    assert(args.threshold > 0);
    assert(args.max_iters > 0);

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
