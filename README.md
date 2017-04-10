# Red Blue Computation

## Rules
The board is initialized with 1/3 blue, 1/3 red, and 1/3 blank cells. There are
two steps, one for red then one for blue.

 1. Red cells move right one cell if it is blank
 2. Blue cells move down one cell if it is blank

Cells wrap around to the top or left if they hit a wall.

## Help

```
Usage: main [OPTION...]

  -c, --threshold=threshold  The threshold.
  -m, --max_iters=max_iters  Max iterations.
  -n, --gridsize=grid_size   Size of the grid.
  -t, --tilesize=tile_size   Size of the tile.
  -?, --help                 Give this help list
      --usage                Give a short usage message

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```

## Compiling and Running

```
$ gcc main.c -o main
$ ./main --gridsize 12 --tilesize 3 --threshold 90 -max_iters 1000
```


