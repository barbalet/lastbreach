#ifndef VOXEL_RANDOMIZER_H
#define VOXEL_RANDOMIZER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lb_randomize_voxels(size_t cell_count, uint8_t *voxel_types_out, uint8_t *surfaces_out, size_t faces_per_cell);

#ifdef __cplusplus
}
#endif

#endif
