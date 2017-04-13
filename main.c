
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
    {"verbose",   'v', 0,   0, "Verbose mode."},
    {0}
};

struct arguments {
    uint32_t grid_size;
    uint32_t tile_size;
    uint32_t threshold;
    uint32_t max_iters;
    bool verbose;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *args = state->input;
    switch (key) {
        case 'n': args->grid_size = atoi(arg); break;
        case 't': args->tile_size = atoi(arg); break;
        case 'c': args->threshold = atoi(arg); break;
        case 'm': args->max_iters = atoi(arg); break;
        case 'v': args->verbose = true; break;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, "", "", 0, 0, 0};

void print_and_exit(uint32_t errnum, const char *message) {
    fprintf(stderr, "Error %d: %s\n", errnum, message);
    exit(errnum);
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
}

int compare(const void * a, const void * b) {
    uint32_t _a = *(uint32_t*)a;
    uint32_t _b = *(uint32_t*)b;
    return a - b;
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

            if (args.verbose) {
                fprintf(
                    stderr,
                    "%d: Sending row %d to process %d\n",
                    id, first->id, owner_id);
            }

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

            if (args.verbose) {
                char row_buf[2048] = {0};
                grid_row_print(&recv_rows[i], row_buf);

                fprintf(
                    stderr,
                    "%d: Recv Row %d from %d: %s\n",
                    id,
                    recv_rows[i].id,
                    owner_id,
                    row_buf);
            }
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
            fprintf(
                stderr,
                "%d: >>> Replying row %d to %d\n",
                id, send_rows[i].id, row_owners[send_rows[i].id]);
        }

        // TODO recv
        for (uint32_t i = 0; i < rowgroups_len; i++) {

            // Get the owner we are receiving from and the row_id that we are
            // receiving
            uint32_t rowgroup_id = rowgroups_owned[i];
            uint32_t row_id = rowgroup_id * args.tile_size;
            struct grid_row_t* first = NULL;
            for (uint32_t j = 0; j < rows_len; j++) {
                if (rows[j].id == row_id) {
                    first = &rows[j];
                    break;
                }
            }
            uint32_t owner_id = row_owners[
                (first->id == 0 ? args.grid_size - 1 : first->id - 1)];

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
            free(ser);
        }

        // Free the send buffers when the send actions have completed.
        for (uint32_t i = 0; i < rowgroups_len; i++) {
            fprintf(stderr, "%d: Waiting.\n", id);
            MPI_Wait(&requests[i], MPI_STATUS_IGNORE);
            fprintf(stderr, "%d: Waited.\n", id);
            free(sers[i]);
        }

        for (uint32_t r = 0; r < rows_len; r++) {
            grid_row_free(&rows_copy[r]);
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

    fprintf(stderr, "Closing %d\n", id);

    MPI_Finalize();
    return 0;
}
