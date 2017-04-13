
#include <assert.h>
#include <argp.h>
#include <execinfo.h>
#include <mpi.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "grid.h"
#include "row.h"

const int MPI_DEFAULT_TAG = 1;
const int MPI_MASTER_ID = 0;

// name, key, arg name, falgs, doc, group
static struct argp_option options[] = {
    {"gridsize",  'n', "grid_size", 0, "Size of the grid."},
    {"tilesize",  't', "tile_size", 0, "Size of the tile."},
    {"threshold", 'c', "threshold", 0, "The threshold."},
    {"max_iters", 'm', "max_iters", 0, "Max iterations."},
    {"verbose",   'v', 0,           0, "Verbose mode."},
    {"print",     'p', 0,           0, "Print."},
    {0}
};

struct arguments {
    uint32_t grid_size;
    uint32_t tile_size;
    uint32_t threshold;
    uint32_t max_iters;
    bool verbose;
    bool print;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;
    switch (key) {
        case 'n': args->grid_size = atoi(arg); break;
        case 't': args->tile_size = atoi(arg); break;
        case 'c': args->threshold = atoi(arg); break;
        case 'm': args->max_iters = atoi(arg); break;
        case 'v': args->verbose = true; break;
        case 'p': args->print = true; break;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, "", "", 0, 0, 0};

void print_and_exit(uint32_t errnum, const char *message) {
    fprintf(stderr, "Error %d: %s\n", errnum, message);
    exit(errnum);
}

void serial_check(struct grid_t* grid_curr, struct arguments args) {
    fprintf(stderr, "Performing serial check.\n");

    struct grid_t grid_prev;
    grid_init_copy(&grid_prev, grid_curr);

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

                grid_curr->elements[r][c] = WHITE;
                grid_curr->elements[r][next] = RED;
            }
        }

        grid_copy(&grid_prev, grid_curr);

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

                grid_curr->elements[r][c] = WHITE;
                grid_curr->elements[next][c] = BLUE;
            }
        }

        grid_copy(&grid_prev, grid_curr);
        finished = grid_check_tiles(grid_curr, args.tile_size, args.threshold);
        iterations++;

        if (args.print) {
            grid_print(grid_curr, args.tile_size);
        }
    }

    if (!args.print) {
        grid_print(grid_curr, args.tile_size);
    }

    if (!finished) {
        fprintf(stderr, "Serial: Hit maximum iterations\n");
    }
}

void master(struct arguments args, uint32_t id, uint32_t num_procs) {
    assert(id == MPI_MASTER_ID);

    // Calculate the owner of each row: row[i] = process_id
    uint32_t row_owners[args.grid_size];
    for (uint32_t i = 0; i < (args.grid_size / args.tile_size); i++) {
        uint32_t process_id = 1 + i % (num_procs - 1);
        for (uint32_t j = 0; j < args.tile_size; j++) {
            row_owners[i * args.tile_size + j] = process_id;
        }
    }

    // Send the row owner meta data to each process
    for (uint32_t dest = 1; dest < num_procs; dest++) {
        MPI_Send(
            row_owners,
            args.grid_size,
            MPI_INT,
            dest,
            MPI_DEFAULT_TAG,
            MPI_COMM_WORLD);
    }

    // Send the row data to relevant process
    srand(time(NULL));
    struct grid_t grid_curr;
    grid_init(&grid_curr, args.grid_size);

    struct grid_t grid_backup;
    grid_init_copy(&grid_backup, &grid_curr);

    for (uint32_t r = 0; r < args.grid_size; r++) {
        uint32_t dest = row_owners[r];

        // Send row number
        MPI_Send(&r, 1, MPI_INT, dest, MPI_DEFAULT_TAG, MPI_COMM_WORLD);

        // Send row data
        MPI_Send(
            grid_curr.elements[r],
            args.grid_size,
            MPI_INT,
            dest,
            MPI_DEFAULT_TAG,
            MPI_COMM_WORLD);
    }

    size_t ser_size = (
        sizeof(uint32_t) +
        sizeof(uint32_t) +
        sizeof(enum cell_type) * args.grid_size);

    uint32_t tx, ty;
    enum cell_type color;
    double ratio;

    bool done = false;
    for (uint32_t i = 0; i < args.max_iters; i++) {
        if (args.print) {
            struct grid_row_t rows[args.grid_size];

            for (uint32_t r = 0; r < args.grid_size; r++) {
                struct grid_row_t row;
                void* serialized_data = calloc(1, ser_size);
                MPI_Recv(
                    serialized_data,
                    ser_size,
                    MPI_BYTE,
                    row_owners[r],
                    MPI_DEFAULT_TAG,
                    MPI_COMM_WORLD,
                    MPI_STATUS_IGNORE);

                grid_row_unserialize(&row, serialized_data);
                free(serialized_data);

                rows[row.id].id = row.id;
                rows[row.id].len = row.len;
                rows[row.id].cells = row.cells;
            }

            fprintf(stderr, "-----------\n");

            for (uint32_t r = 0; r < args.grid_size; r++) {

                char buf[2048] = {0};
                grid_row_print(&rows[r], buf);
                fprintf(stderr, "row %02d: %s\n", rows[r].id, buf);
                grid_row_free(&rows[r]);
            }
        }

        size_t sz = (
            sizeof(bool) +
            sizeof(uint32_t) +
            sizeof(uint32_t) +
            sizeof(enum cell_type) +
            sizeof(double));

        for (uint32_t p = 1; p < num_procs; p++) {
            void* data = calloc(1, sz);
            MPI_Recv(
                data,
                sz,
                MPI_BYTE,
                p,
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE);

            bool finished = ((bool*)data)[0];
            size_t cursor = sizeof(finished);

            if (!finished) {
                continue;
            }

            // Unpack all the data.
            tx = ((uint32_t*)(data + cursor))[0];
            cursor += sizeof(tx);
            ty = ((uint32_t*)(data + cursor))[0];
            cursor += sizeof(ty);
            color = ((enum cell_type*)(data + cursor))[0];
            cursor += sizeof(color);
            ratio = ((double*)(data + cursor))[0];

            if (!done) {
                fprintf(
                    stderr,
                    "Tile (c=%d, r=%d) has %f%% %s\n",
                    tx,
                    ty,
                    ratio * 100.0,
                    color == BLUE ? "BLUE" : "RED");
                done = true;
            }

        }

        for (uint32_t p = 1; p < num_procs; p++) {
            MPI_Send(
                &done,
                1,
                MPI_INT,
                p,
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD);
        }

        if (done) {
            break;
        }

        if (args.verbose) {
            fprintf(stderr, "Performed %d of %d iterations.\n", i+1, args.max_iters);
        }
    }

    if (!done) {
        fprintf(stderr, "MPI: Hit maximum iterations\n");
    }

    serial_check(&grid_backup, args);
}



int compare(const void * a, const void * b) {
    uint32_t _a = *(uint32_t*)a;
    uint32_t _b = *(uint32_t*)b;
    return a - b;
}

void master_finished(
        bool finished,
        uint32_t tx,
        uint32_t ty,
        enum cell_type color,
        double ratio) {

    // Serialize the data we want to send to master o.0
    size_t sz = (
        sizeof(finished) +
        sizeof(tx) +
        sizeof(ty) +
        sizeof(color) +
        sizeof(ratio));
    size_t cursor = 0;
    void* buf = calloc(1, sz);
    memcpy(buf + cursor, &finished, sizeof(finished));
    cursor += sizeof(finished);
    memcpy(buf + cursor, &tx, sizeof(tx));
    cursor += sizeof(tx);
    memcpy(buf + cursor, &ty, sizeof(ty));
    cursor += sizeof(ty);
    memcpy(buf + cursor, &color, sizeof(color));
    cursor += sizeof(color);
    memcpy(buf + cursor, &ratio, sizeof(ratio));
    cursor += sizeof(ratio);

    // Send the data to master.
    MPI_Send(buf, sz, MPI_BYTE, MPI_MASTER_ID, MPI_DEFAULT_TAG, MPI_COMM_WORLD);
}

uint32_t get_rowgroup_id(const struct grid_row_t* row, uint32_t tile_size) {
    return (uint32_t)(row->id / tile_size);
}

void slave(struct arguments args, uint32_t id, uint32_t num_procs) {
    uint32_t row_owners[args.grid_size];
    MPI_Status status;

    // Get the list of row owners from Master
    MPI_Recv(
        row_owners, args.grid_size, MPI_INT,
        MPI_MASTER_ID, MPI_DEFAULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Count how many rows we own
    uint32_t rows_len = 0;
    for (uint32_t r = 0; r < args.grid_size; r++) {
        if (row_owners[r] == id) {
            rows_len++;
        }
    }

    if (rows_len == 0) {
        return;
    }

    // Get the row data from Master.
    struct grid_row_t rows[rows_len];
    for (uint32_t i = 0; i < rows_len; i++) {
        grid_row_init(&rows[i], args.grid_size);

        MPI_Recv(
            &rows[i].id, 1, MPI_INT,
            MPI_MASTER_ID, MPI_DEFAULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        MPI_Recv(
            rows[i].cells, rows[i].len, MPI_INT,
            MPI_MASTER_ID, MPI_DEFAULT_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        if (args.verbose) {
            char buf[2048] = {0};
            sprintf(buf, "Recv Row %d: ", rows[i].id);
            grid_row_print(&rows[i], buf);
            fprintf(stderr, "%d: %s\n", id, buf);
        }
    }


    for (uint32_t i = 0; i < rows_len-1; i++) {
        assert(rows[i].id < rows[i+1].id);
    }

    // This is the main action loop. Red -> Blue -> Check
    // Red is easy, we have all the data we need. Blue is harder, it requires
    // communicating with the owner of the next row. I've implemented this using
    // a pass the token algorithm, this is naive but it is easier to get working
    // than a modulo algorithm.
    for (uint32_t iterations = 0; iterations < args.max_iters; iterations++) {

        // Perform Red
        for (uint32_t r = 0; r < rows_len; r++) {
            struct grid_row_t copy;
            grid_row_copy(&copy, &rows[r]);
            for (uint32_t c = 0; c < rows[r].len; c++) {
                if (copy.cells[c] != RED) {
                    continue;
                }

                uint32_t next = (c+1) % copy.len;
                if (copy.cells[next] != WHITE) {
                    continue;
                }

                rows[r].cells[c] = WHITE;
                rows[r].cells[next] = RED;
            }
            grid_row_free(&copy);
        }

        // Perform blue.

        // Calculate the IDs of the Row Groups we own.
        uint32_t rowgroups_len = (uint32_t)(rows_len / args.tile_size);
        uint32_t rowgroups_owned[rowgroups_len];
        for (uint32_t r = 0; r < rowgroups_len; r++) {
            uint32_t row_id = r * args.tile_size;
            rowgroups_owned[r] = get_rowgroup_id(&rows[row_id], args.tile_size);
        }


        // Ascending sort; just to make sure.
        qsort(rowgroups_owned, rowgroups_len, sizeof(int), compare);
        MPI_Request requests[rowgroups_len];
        size_t ser_size = grid_row_serialize_size(&rows[0]);

        // For each rowgroup, send our data to the owner of the previous row
        for (uint32_t i = 0; i < rowgroups_len; i++) {
            uint32_t rowgroup_id = rowgroups_owned[i];

            uint32_t row_id = rowgroup_id * args.tile_size;
            struct grid_row_t* first = NULL;

            // Get the first row back
            for (uint32_t j = 0; j < rows_len; j++) {
                if (rows[j].id == row_id) {
                    first = &rows[j];
                    break;
                }
            }

            int32_t prev_row_id = (first->id == 0 ? args.grid_size - 1 : first->id - 1);
            uint32_t owner_id = row_owners[prev_row_id];

            void* ser = grid_row_serialize(first);

            MPI_Isend(
                ser,
                ser_size,
                MPI_BYTE,
                owner_id,
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD,
                &requests[i]);

            // TODO Move outside... free after requests are done...
            free(ser);
        }

        struct grid_row_t recv_rows[rowgroups_len];
        struct grid_row_t send_rows[rowgroups_len];

        for (uint32_t i = 0; i < rowgroups_len; i++) {
            uint32_t rowgroup_id = rowgroups_owned[i];

            // the owner we are receiving from is the owner of the next
            // rowgroup, get the row_id of the the first row of next rowgroup
            uint32_t row_id = rowgroup_id * args.tile_size + args.tile_size;
            if (row_id == args.grid_size) {
                row_id = 0;
            }
            uint32_t owner_id = row_owners[row_id];

            // Recv the row from owner_id
            void* ser = calloc(1, ser_size);
            MPI_Recv(
                ser,
                ser_size,
                MPI_BYTE,
                owner_id,
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            );
            grid_row_unserialize(&recv_rows[i], ser);
            grid_row_init(&send_rows[i], recv_rows[i].len);
            send_rows[i].id = recv_rows[i].id;
            free(ser);
        }

        struct grid_row_t rows_copy[rows_len];
        for (uint32_t r = 0; r < rows_len; r++) {
            grid_row_copy(&rows_copy[r], &rows[r]);
        }


        // Now we have all the rows we need to validate blue movement.
        // Now we will move. We need to tell the owner of the rows what the rows
        // will be.
        for (uint32_t r = 0; r < rows_len; r++) {
            struct grid_row_t* curr = &rows[r];
            struct grid_row_t* next = NULL;
            struct grid_row_t* send = NULL;

            uint32_t next_id = (rows[r].id + 1) % args.grid_size;

            if (row_owners[next_id] == id) {
                for (uint32_t j = 0; j < rows_len; j++) {
                    if (rows[j].id == next_id) {
                        next = &rows[j];
                        break;
                    }
                }
            } else {
                for (uint32_t j = 0; j < rowgroups_len; j++) {
                    if (recv_rows[j].id == next_id) {
                        next = &recv_rows[j];
                        send = &send_rows[j];
                        break;
                    }
                }
            }

            assert(next != NULL);

            struct grid_row_t* copy = &rows_copy[r];

            for (uint32_t c = 0; c < rows[r].len; c++) {
                if (copy->cells[c] != BLUE) {
                    continue;
                }

                if (next->cells[c] != WHITE) {
                    continue;
                }

                if (send != NULL) {
                    send->cells[c] = BLUE;
                    curr->cells[c] = WHITE;
                } else {
                    next->cells[c] = BLUE;
                    curr->cells[c] = WHITE;
                }
            }
        }

        void* sers[rowgroups_len];

        // We have moved all the blues, so update the owners of the blue
        for (uint32_t i = 0; i < rowgroups_len; i++) {
            sers[i] = grid_row_serialize(&send_rows[i]);
            MPI_Isend(
                sers[i],
                ser_size,
                MPI_BYTE,
                row_owners[send_rows[i].id],
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD,
                &requests[i]);
        }

        for (uint32_t i = 0; i < rowgroups_len; i++) {
            uint32_t rowgroup_id = rowgroups_owned[i];

            // Find row data of the last row of previous rowgroup
            uint32_t row_id = rowgroup_id * args.tile_size;
            row_id = (row_id ? row_id - 1 : args.grid_size - 1);
            uint32_t owner_id = row_owners[row_id];

            struct grid_row_t blue_row;

            void* ser = calloc(1, ser_size);
            MPI_Recv(
                ser,
                ser_size,
                MPI_BYTE,
                owner_id,
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD,
                MPI_STATUS_IGNORE
            );
            grid_row_unserialize(&blue_row, ser);
            free(ser);

            struct grid_row_t* local_row = NULL;
            for (uint32_t i = 0; i < rows_len; i++) {
                if (rows[i].id == blue_row.id) {
                    local_row = &rows[i];
                }
            }
            assert(local_row != NULL);

            char blue_row_buf[2048] = {0};
            grid_row_print(&blue_row, blue_row_buf);

            // blue_row contains the new blue items
            for (uint32_t c = 0; c < args.grid_size; c++) {
                if (blue_row.cells[c] == BLUE) {
                    local_row->cells[c] = BLUE;
                }
            }
        }

        // Free the send buffers when the send actions have completed.
        for (uint32_t i = 0; i < rowgroups_len; i++) {
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
            free(sers[i]);
        }

        // Free the row copies for this run iteration
        for (uint32_t r = 0; r < rows_len; r++) {
            grid_row_free(&rows_copy[r]);
        }

        if (args.print) {
            for (uint32_t i = 0; i < rows_len; i++) {
                void* ser = grid_row_serialize(&rows[i]);
                MPI_Isend(
                    ser,
                    ser_size,
                    MPI_BYTE,
                    0,
                    MPI_DEFAULT_TAG,
                    MPI_COMM_WORLD,
                    &requests[i]);
            }
        }

        sleep(0.5);

        double threshold = args.threshold / 100.0;

        for (uint32_t i = 0; i < rowgroups_len; i++) {
            // get the tile_size rows and deal with it
            // Get tile_size worth of rows, then check the square, count...

            uint32_t row_start = i * args.tile_size;
            uint32_t row_end = row_start + args.tile_size - 1;
            uint32_t tiles_num = args.grid_size / args.tile_size;
            uint32_t blue_counts[tiles_num];
            uint32_t red_counts[tiles_num];

            for (uint32_t j = 0; j < tiles_num; j++) {
                blue_counts[j] = 0;
                red_counts[j] = 0;
            }


            for (uint32_t r = row_start; r <= row_end; r++) {
                for (uint32_t c = 0; c < args.grid_size; c++) {
                    uint32_t t = c / args.tile_size;

                    switch(rows[r].cells[c]) {
                    case BLUE: blue_counts[t]++; break;
                    case RED: red_counts[t]++; break;
                    default: break;
                    }

                }
            }

            uint32_t cells_per_tile = (args.tile_size * args.tile_size);

            for (uint32_t t = 0; t < tiles_num; t++) {
                double blue_ratio = (double)(blue_counts[t]) / (double)(cells_per_tile);
                uint32_t row_id = rows[i * args.tile_size].id;
                uint32_t ty = row_id / args.tile_size;

                if (blue_ratio >= threshold) {
                    master_finished(true, t, ty, BLUE, blue_ratio);
                    return;
                }

                double red_ratio = (double)(red_counts[t] / cells_per_tile);
                if (red_ratio >= threshold) {
                    master_finished(true, t, ty, RED, red_ratio);
                    return;
                }
            }

        }

        master_finished(false, 0, 0, 0, 0);

        bool finished;
        MPI_Recv(
            &finished,
            1,
            MPI_INT,
            MPI_MASTER_ID,
            MPI_DEFAULT_TAG,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);

        if (finished) {
            return;
        }
    }
}

int main(int argc, char** argv) {
    uint32_t id;
    uint32_t num_procs;
    uint32_t retval;

    if ((retval = MPI_Init(&argc, &argv)) != MPI_SUCCESS) {
        print_and_exit(retval, "Could not initialize MPI");
    }

    if ((retval = MPI_Comm_size(MPI_COMM_WORLD, &num_procs)) != MPI_SUCCESS) {
        print_and_exit(retval, "Error getting number of processes in MPI");
    }

    if ((retval = MPI_Comm_rank(MPI_COMM_WORLD, &id)) != MPI_SUCCESS) {
        print_and_exit(retval, "Error getting rank/id from MPI");
    }

    struct arguments args;
    args.grid_size = 0;
    args.tile_size = 0;
    args.threshold = 0;
    args.max_iters = 0;
    args.verbose = false;
    args.print = false;
    argp_parse(&argp, argc, argv, 0, 0, &args);

    assert(args.grid_size > 0);
    assert(args.tile_size > 0);
    assert(args.threshold > 0);
    assert(args.max_iters > 0);

    args.tile_size = (uint32_t)(args.grid_size / args.tile_size);

    if (id == MPI_MASTER_ID) {
        master(args, id, num_procs);
    } else {
        slave(args, id, num_procs);
    }

    MPI_Finalize();


    // Perform serialized part.

    return 0;
}
