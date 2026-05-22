#ifndef VOXEL_RANDOMIZER_H
#define VOXEL_RANDOMIZER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Populates `voxel_types_out` (size = grid_size^3) and `surfaces_out`
 * (size = grid_size^3 * faces_per_cell).
 * Caller owns buffers and must provide writable storage.
 */
void lb_randomize_voxels(size_t grid_size, uint8_t *voxel_types_out, uint8_t *surfaces_out, size_t faces_per_cell);

#ifdef __cplusplus
}
#endif

#endif
