#ifndef _ROW_H_
#define _ROW_H_

#include "grid.h"

struct grid_row_t {
    uint32_t id;
    uint32_t len;
    enum cell_type* cells;
};

size_t grid_row_serialize_size(uint32_t grid_size);

void* grid_row_serialize(struct grid_row_t* self);

void grid_row_unserialize(struct grid_row_t* self, void* buf);

void grid_row_init(struct grid_row_t *self, uint32_t len);

void grid_row_free(struct grid_row_t *self);

void grid_row_print(struct grid_row_t *self, char* buf);

void grid_row_copy(struct grid_row_t *self, const struct grid_row_t *copy);

#endif
