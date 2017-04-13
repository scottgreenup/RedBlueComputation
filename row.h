#ifndef _ROW_H_
#define _ROW_C_

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "grid.h"

struct grid_row_t {
    uint32_t id;
    uint32_t len;
    enum cell_type* cells;
};

void* grid_row_serialize(struct grid_row_t* self) {
    size_t size = (
        sizeof(self->id) +
        sizeof(self->len) +
        sizeof(enum cell_type) * self->len);
    void* buf = calloc(1, size);

    memcpy(buf, &self->id, sizeof(self->id));
    size_t cur = sizeof(self->id);

    memcpy(buf + cur, &self->len, sizeof(self->len));
    cur += sizeof(self->len);

    memcpy(buf + cur, self->cells, sizeof(enum cell_type) * self->len);
    return buf;
}

void grid_row_unserialize(struct grid_row_t* self, void* buf) {
    self->id = ((uint32_t*)buf)[0];
    fprintf(stderr, "unserialize: id = %u\n", self->id);

    self->len = ((uint32_t*)buf)[1];
    fprintf(stderr, "unserialize: len = %u\n", self->len);

    enum cell_type* cells_ptr = buf + (sizeof(self->id) + sizeof(self->len));
    self->cells = (enum cell_type*)malloc(sizeof(enum cell_type) * self->len);

    memcpy(self->cells, cells_ptr, self->len * sizeof(enum cell_type));
}

void grid_row_init(struct grid_row_t *self, uint32_t len) {
    self->id = 0;
    self->len = len;
    self->cells = (enum cell_type*)calloc(len, sizeof(enum cell_type));
}

void grid_row_free(struct grid_row_t *self) {
    free(self->cells);
}

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

void grid_row_copy(struct grid_row_t *self, const struct grid_row_t *copy) {
    grid_row_init(self, copy->len);
    self->id = copy->id;
    for (uint32_t c = 0; c < copy->len; c++) {
        self->cells[c] = copy->cells[c];
    }
}


struct row_request {
    uint32_t id;
    uint32_t owner;
};


#endif
