
#include <assert.h>
#include <argp.h>
#include <mpi.h>
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


    return;

    // The first tile_size go to 



    // Initialize the grid with blue, red, and white cells
    //srand(time(NULL));
    //struct grid_t grid_curr;
    //grid_init(&grid_curr, args.grid_size);

    // Create copy of grid.
    struct grid_t grid_p;
    grid_init_copy(&grid_p, &grid_curr);

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

        // For each rowgroup, send our data to the owner of the previous row
        for (uint32_t i = 0; i < rowgroups_len; i++) {
            uint32_t rowgroup_id = rowgroups_owned[i];

            // Foreach of our rowgroups. Send out first row to the rowgroup
            // proceeding us. Therefore, get our first row in each rowgroup.

            uint32_t row_id = rowgroup_id * args.tile_size;
            struct grid_row_t* first = NULL;

            // Get the first row back
            for (uint32_t j = 0; j < rows_len; j++) {
                if (rows[j].id == row_id) {
                    first = &rows[j];
                    break;
                }
            }

            int32_t prev_row_id = first->id - 1;
            if (prev_row_id < 0) {
                prev_row_id = args.grid_size - 1;
            }
            uint32_t owner_id = row_owners[prev_row_id];

            if (args.verbose) {
                fprintf(
                    stderr,
                    "%d: Sending row %d to process %d\n",
                    id, first->id, owner_id);
            }

            MPI_Isend(
                &first->id,
                1,
                MPI_INT,
                owner_id,
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD,
                &requests[i]);

            MPI_Isend(
                &first->id,
                1,
                MPI_INT,
                owner_id,
                MPI_DEFAULT_TAG,
                MPI_COMM_WORLD,
                &requests[i]);
        }

        struct grid_row_t rows[rowgroups_len];

        for (uint32_t i = 0; i < rowgroups_len; i++) {
            uint32_t rowgroup_id = rowgroups_owned[i];

            // We should've been sent row data, store it.
            //MPI_Recv(
            //    &
            //);
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

    /*
    struct grid_row_t row;
    grid_row_init(&row, 8);
    row.id = 1;
    row.len=8;
    for (int i = 0; i < 8; i++) {
        row.cells[i] = (enum cell_type)(i % 3);
    }
    void* sbuf = grid_row_serialize(&row);

    uint32_t* ubuf = (uint32_t*)sbuf;
    for (uint32_t i = 0; i < 10; i++) {
        fprintf(stderr, "%d\n", ubuf[i]);
    }

    struct grid_row_t row_;
    grid_row_unserialize(&row_, sbuf);

    char buf[512] = {0};
    grid_row_print(&row_, &buf[0]);
    fprintf(stderr, "%s\n", buf);

    return 1;
    */

    if (id == MPI_MASTER_ID) {
        master(args, id, num_procs);
    } else {
        slave(args, id, num_procs);
    }

    MPI_Finalize();
    return 0;
}
