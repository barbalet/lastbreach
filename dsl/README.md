# lastbreach DSL Runner (C)

This folder contains a standalone C99 program that parses and runs:
- Two required `.lbp` character plan files
- Optional `.lbw` world file
- Optional `.lbc` catalog file

## Build
```sh
make
```

## Run
```sh
./lastbreach examples/mara.lbp examples/joel.lbp --days 3 --world world.lbw --catalog catalog.lbc
```

If `--world` is omitted and `world.lbw` exists in the current directory, it is auto-loaded.
If `--catalog` is omitted and `catalog.lbc` exists in the current directory, it is auto-loaded.

## Output
- Tick-by-tick schedule choices
- Station conflicts (e.g., both try to use the kitchen)
- Event output (breach + overnight checks)
- Simple stat drift and task completion messages

This is a lightweight runner meant to be extended with:
- Full item consumption/production and tool wear from `.lbc`
- Richer world event logic and posture effects
- More strict validation against the full DSL spec
