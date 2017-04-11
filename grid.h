#ifndef _GRID_H_
#define _GRID_H_

#include <stdbool.h>
#include <stdint.h>

enum cell_type {
    BLUE,
    RED,
    WHITE
};

struct grid_t {
    enum cell_type** elements;
    uint32_t size;
};

void grid_init(struct grid_t *self, uint32_t size);

void grid_init_copy(struct grid_t *self, const struct grid_t* copy);

void grid_copy(struct grid_t *self, const struct grid_t* source);

void grid_print_line(const struct grid_t *self, uint32_t tile_size);

void grid_print(const struct grid_t *self, uint32_t tile_size);

bool grid_check_tiles(
    const struct grid_t *self, uint32_t tile_size, uint32_t threshold);

#endif
