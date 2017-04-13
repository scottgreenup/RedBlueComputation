#ifndef _ROW_H_
#define _ROW_H_

#include "grid.h"

struct grid_row_t {
    uint32_t id;
    uint32_t len;
    enum cell_type* cells;
};

// Calculate the byte size of grid_row_t
size_t grid_row_serialize_size(uint32_t grid_size);

// Create a serialized buffer for MPI_Send of a grid_row_t
void* grid_row_serialize(struct grid_row_t* self);

// Unserialize the output from grid_row_serialize
void grid_row_unserialize(struct grid_row_t* self, void* buf);

// Intialize a grid_row_t
void grid_row_init(struct grid_row_t *self, uint32_t len);

// Destroy a grid_row_t
void grid_row_free(struct grid_row_t *self);

// Useful way to print a grid_row_t to a buffer
void grid_row_print(struct grid_row_t *self, char* buf);

// Copy the grid_row_t into a new grid_row_t
void grid_row_copy(struct grid_row_t *self, const struct grid_row_t *copy);

#endif
