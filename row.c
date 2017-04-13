
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "row.h"

size_t grid_row_serialize_size(uint32_t grid_size) {
    return (
        sizeof(uint32_t) +
        sizeof(uint32_t) +
        sizeof(enum cell_type) * grid_size);
}

// Create a serialized buffer for MPI_Send of a grid_row_t
void* grid_row_serialize(struct grid_row_t* self) {
    void* buf = calloc(1, grid_row_serialize_size(self->len));

    memcpy(buf, &self->id, sizeof(self->id));
    size_t cur = sizeof(self->id);

    memcpy(buf + cur, &self->len, sizeof(self->len));
    cur += sizeof(self->len);

    memcpy(buf + cur, self->cells, sizeof(enum cell_type) * self->len);
    return buf;
}

// Unserialize the output from grid_row_serialize
void grid_row_unserialize(struct grid_row_t* self, void* buf) {
    self->id = ((uint32_t*)buf)[0];
    self->len = ((uint32_t*)buf)[1];

    enum cell_type* cells_ptr = buf + (sizeof(self->id) + sizeof(self->len));
    self->cells = (enum cell_type*)malloc(sizeof(enum cell_type) * self->len);

    memcpy(self->cells, cells_ptr, self->len * sizeof(enum cell_type));
}

// Intialize a grid_row_t
void grid_row_init(struct grid_row_t *self, uint32_t len) {
    self->id = 0;
    self->len = len;
    self->cells = (enum cell_type*)calloc(len, sizeof(enum cell_type));
}

// Destroy a grid_row_t
void grid_row_free(struct grid_row_t *self) {
    free(self->cells);
}

// Useful way to print a grid_row_t to a buffer
void grid_row_print(struct grid_row_t *self, char* buf) {
    for (uint32_t c = 0; c < self->len; c++) {
        switch (self->cells[c]) {
        case RED:   sprintf(buf + strlen(buf), "> "); break;
        case BLUE:  sprintf(buf + strlen(buf), "v "); break;
        case WHITE: sprintf(buf + strlen(buf), "- "); break;
        default: sprintf(buf + strlen(buf), "%d ", self->cells[c]); break;
        }
    }
}

// Copy the grid_row_t into a new grid_row_t
void grid_row_copy(struct grid_row_t *self, const struct grid_row_t *copy) {
    grid_row_init(self, copy->len);
    self->id = copy->id;
    for (uint32_t c = 0; c < copy->len; c++) {
        self->cells[c] = copy->cells[c];
    }
}
