# Red Blue Computation

## Rules
The board is initialized with 1/3 blue, 1/3 red, and 1/3 blank cells. There are
two steps, one for red then one for blue.

 1. Red cells move right one cell if it is blank
 2. Blue cells move down one cell if it is blank

Cells wrap around to the top or left if they hit a wall.

## Help

```
$ ./main --help
Usage: main [OPTION...]

  -c, --threshold=threshold  The threshold.
  -m, --max_iters=max_iters  Max iterations.
  -n, --gridsize=grid_size   Size of the grid.
  -p, --print                Print.
  -t, --tilesize=tile_size   Size of the tile.
  -v, --verbose              Verbose mode.
  -?, --help                 Give this help list
      --usage                Give a short usage message

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```

## Compiling and Running

```
$ mpicc main.c grid.c -o main --std=c11
$ mpirun -np $NUM_PROCS  ./main -n 12 -t 3 -m 10 -c 75
```


