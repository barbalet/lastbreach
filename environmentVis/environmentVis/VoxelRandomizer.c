#include "VoxelRandomizer.h"

#include <stdlib.h>

static uint8_t lb_random_voxel_type(void) {
    uint32_t draw = arc4random_uniform(100);

    if (draw < 34) {
        return 0; // water
    }
    if (draw < 68) {
        return 1; // soil
    }
    return 2; // air
}

static uint8_t lb_random_surface_type(void) {
    uint32_t draw = arc4random_uniform(100);

    if (draw < 25) {
        return 0; // open
    }
    if (draw < 50) {
        return 1; // trapdoor/door
    }
    if (draw < 75) {
        return 2; // window/skylight
    }
    return 3; // floor/wall
}

void lb_randomize_voxels(size_t cell_count, uint8_t *voxel_types_out, uint8_t *surfaces_out, size_t faces_per_cell) {
    if (voxel_types_out == NULL || surfaces_out == NULL || faces_per_cell == 0) {
        return;
    }

    for (size_t i = 0; i < cell_count; i++) {
        voxel_types_out[i] = lb_random_voxel_type();

        uint8_t *surface_start = surfaces_out + (i * faces_per_cell);
        for (size_t face = 0; face < faces_per_cell; face++) {
            surface_start[face] = lb_random_surface_type();
        }
    }
}
