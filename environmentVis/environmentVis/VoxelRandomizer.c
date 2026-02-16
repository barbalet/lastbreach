#include "VoxelRandomizer.h"

#include <stdlib.h>

/*
 * Voxel generator goals:
 * - deterministic shape constraints with randomized variation
 * - mostly-air top layers and a center shaft to keep interior readable
 * - surface labels derived from voxel-to-voxel transitions
 */

static int32_t lb_clamp_int32(int32_t value, int32_t min_value, int32_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int32_t lb_random_int32(int32_t min_value, int32_t max_value) {
    if (max_value <= min_value) {
        return min_value;
    }

    /* arc4random_uniform gives unbiased values in [0, span). */
    uint32_t span = (uint32_t)(max_value - min_value + 1);
    return min_value + (int32_t)arc4random_uniform(span);
}

static size_t lb_random_size_t(size_t min_value, size_t max_value) {
    if (max_value <= min_value) {
        return min_value;
    }

    size_t span = max_value - min_value + 1;
    if (span > (size_t)UINT32_MAX) {
        span = (size_t)UINT32_MAX;
    }

    return min_value + (size_t)arc4random_uniform((uint32_t)span);
}

static size_t lb_random_index(size_t count) {
    if (count <= 1) {
        return 0;
    }

    if (count > (size_t)UINT32_MAX) {
        return (size_t)arc4random_uniform(UINT32_MAX);
    }
    return (size_t)arc4random_uniform((uint32_t)count);
}

static size_t lb_diagonal_position(size_t grid_size, size_t x, size_t z, uint32_t mode) {
    size_t edge = grid_size - 1;

    /* Four diagonal orientations so terrain slope can rotate between runs. */
    switch (mode) {
        case 1:
            return x + (edge - z);
        case 2:
            return (edge - x) + z;
        case 3:
            return (edge - x) + (edge - z);
        default:
            return x + z;
    }
}

static int32_t lb_diagonal_soil_top(
    size_t grid_size,
    size_t x,
    size_t z,
    uint32_t diagonal_mode,
    int32_t low_height,
    int32_t high_height,
    int32_t jitter
) {
    /* Interpolate between low/high bounds along the selected diagonal axis. */
    int32_t base_height = low_height;

    if (grid_size > 1) {
        size_t diagonal_max = 2 * (grid_size - 1);
        size_t diagonal = lb_diagonal_position(grid_size, x, z, diagonal_mode);
        int64_t delta = (int64_t)high_height - (int64_t)low_height;

        base_height += (int32_t)((delta * (int64_t)diagonal) / (int64_t)diagonal_max);
    }

    if (jitter > 0) {
        base_height += lb_random_int32(-jitter, jitter);
    }

    return lb_clamp_int32(base_height, 0, (int32_t)grid_size - 1);
}

static size_t lb_voxel_index(size_t grid_size, size_t x, size_t y, size_t z) {
    return (x * grid_size * grid_size) + (y * grid_size) + z;
}

static int lb_is_in_center_air(
    size_t x,
    size_t z,
    size_t center_start_x,
    size_t center_end_x,
    size_t center_start_z,
    size_t center_end_z
) {
    return x >= center_start_x && x < center_end_x && z >= center_start_z && z < center_end_z;
}

static void lb_adjust_soil_to_target(
    int32_t *soil_tops,
    size_t column_count,
    int32_t max_top,
    size_t target_soil_cells,
    size_t *soil_cells_inout
) {
    /*
     * Nudge per-column soil tops toward an exact aggregate target.
     * Random passes keep shapes organic; deterministic fallback guarantees finish.
     */
    size_t guard = (column_count * (size_t)(max_top + 1) * 6) + 1;

    while (*soil_cells_inout < target_soil_cells && guard > 0) {
        size_t index = lb_random_index(column_count);
        if (soil_tops[index] >= 0 && soil_tops[index] < max_top) {
            soil_tops[index] += 1;
            *soil_cells_inout += 1;
        }
        guard--;
    }

    if (*soil_cells_inout < target_soil_cells) {
        for (size_t i = 0; i < column_count && *soil_cells_inout < target_soil_cells; i++) {
            while (soil_tops[i] >= 0 && soil_tops[i] < max_top && *soil_cells_inout < target_soil_cells) {
                soil_tops[i] += 1;
                *soil_cells_inout += 1;
            }
        }
    }

    guard = (column_count * (size_t)(max_top + 1) * 6) + 1;

    while (*soil_cells_inout > target_soil_cells && guard > 0) {
        size_t index = lb_random_index(column_count);
        if (soil_tops[index] > 0) {
            soil_tops[index] -= 1;
            *soil_cells_inout -= 1;
        }
        guard--;
    }

    if (*soil_cells_inout > target_soil_cells) {
        for (size_t i = 0; i < column_count && *soil_cells_inout > target_soil_cells; i++) {
            while (soil_tops[i] > 0 && *soil_cells_inout > target_soil_cells) {
                soil_tops[i] -= 1;
                *soil_cells_inout -= 1;
            }
        }
    }
}

static void lb_raise_soil_protrusions(
    int32_t *soil_tops,
    size_t column_count,
    int32_t water_level,
    int32_t max_height,
    size_t minimum_water_cells,
    size_t *soil_cells_inout
) {
    /*
     * Convert some water volume into taller soil outcrops while preserving a
     * minimum water budget for visual/structural balance.
     */
    if (column_count == 0 || water_level >= max_height) {
        return;
    }

    size_t water_cells = 0;
    for (size_t i = 0; i < column_count; i++) {
        if (soil_tops[i] >= 0 && soil_tops[i] < water_level) {
            water_cells += (size_t)(water_level - soil_tops[i]);
        }
    }

    if (water_cells <= minimum_water_cells) {
        return;
    }

    size_t water_surplus = water_cells - minimum_water_cells;
    size_t attempts = column_count * 4;

    while (attempts > 0 && water_surplus > 0) {
        size_t index = lb_random_index(column_count);
        int32_t top = soil_tops[index];

        if (top < 0 || top >= max_height) {
            attempts--;
            continue;
        }

        int32_t raise_by = lb_random_int32(1, max_height - top);
        int32_t raised_top = top + raise_by;

        size_t water_cost = 0;
        if (top < water_level) {
            int32_t capped_raised_top = raised_top < water_level ? raised_top : water_level;
            water_cost = (size_t)(capped_raised_top - top);
        }

        if (water_cost > water_surplus) {
            attempts--;
            continue;
        }

        soil_tops[index] = raised_top;
        *soil_cells_inout += (size_t)raise_by;
        water_surplus -= water_cost;
        attempts--;
    }
}

static void lb_add_top_air_soil_spikes(
    int32_t *soil_tops,
    size_t column_count,
    size_t active_column_count,
    int32_t fill_top,
    int32_t top_air_layers
) {
    /*
     * Optional sparse spikes that pierce the top air layer; this breaks up
     * perfectly flat silhouettes while keeping most of the cap open.
     */
    if (column_count == 0 || active_column_count == 0 || top_air_layers <= 0) {
        return;
    }

    if (arc4random_uniform(100) < 35) {
        return; // Keep spikes optional so flat tops are still common.
    }

    size_t top_air_cells = active_column_count * (size_t)top_air_layers;
    size_t max_spike_cells = (top_air_cells * 29) / 100;
    if (max_spike_cells == 0) {
        return;
    }

    size_t target_spike_cells = lb_random_size_t(1, max_spike_cells);
    size_t max_spike_columns = (active_column_count * 20) / 100;
    if (max_spike_columns == 0) {
        max_spike_columns = 1;
    }
    if (max_spike_columns > active_column_count) {
        max_spike_columns = active_column_count;
    }

    size_t spike_column_target = lb_random_size_t(1, max_spike_columns);
    uint8_t *spike_markers = (uint8_t *)calloc(column_count, sizeof(uint8_t));
    if (spike_markers == NULL) {
        return;
    }

    size_t remaining_cells = target_spike_cells;
    size_t remaining_columns = spike_column_target;
    size_t attempts = column_count * 8;

    while (remaining_columns > 0 && remaining_cells > 0 && attempts > 0) {
        size_t column_index = lb_random_index(column_count);
        if (soil_tops[column_index] < 0 || spike_markers[column_index] != 0) {
            attempts--;
            continue;
        }

        spike_markers[column_index] = 1;

        size_t minimum_cells_for_rest = remaining_columns > 1 ? (remaining_columns - 1) : 0;
        size_t max_cells_for_this_column = remaining_cells - minimum_cells_for_rest;
        int32_t desired_raise = lb_random_int32(1, top_air_layers);
        if ((size_t)desired_raise > max_cells_for_this_column) {
            desired_raise = (int32_t)max_cells_for_this_column;
        }
        if (desired_raise < 1) {
            desired_raise = 1;
        }

        soil_tops[column_index] = fill_top + desired_raise;
        remaining_cells -= (size_t)desired_raise;
        remaining_columns--;
        attempts--;
    }

    attempts = column_count * 8;
    while (remaining_cells > 0 && attempts > 0) {
        size_t column_index = lb_random_index(column_count);
        if (soil_tops[column_index] < 0 || spike_markers[column_index] == 0) {
            attempts--;
            continue;
        }

        int32_t current_raise = soil_tops[column_index] - fill_top;
        if (current_raise >= top_air_layers) {
            attempts--;
            continue;
        }

        soil_tops[column_index] += 1;
        remaining_cells--;
        attempts--;
    }

    free(spike_markers);
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

static uint8_t lb_voxel_type_at(
    size_t grid_size,
    const uint8_t *voxel_types,
    int32_t x,
    int32_t y,
    int32_t z
) {
    if (x < 0 || y < 0 || z < 0) {
        return 2; // outside bounds is treated as air
    }

    if ((size_t)x >= grid_size || (size_t)y >= grid_size || (size_t)z >= grid_size) {
        return 2; // outside bounds is treated as air
    }

    size_t index = lb_voxel_index(grid_size, (size_t)x, (size_t)y, (size_t)z);
    return voxel_types[index];
}

static uint8_t lb_surface_type_from_transition(uint8_t current_type, uint8_t adjacent_type) {
    /*
     * Surface meaning is inferred from material boundary:
     * - air/water -> window/skylight
     * - air/soil  -> floor/wall
     * - everything else defaults to open
     */
    if ((current_type == 2 && adjacent_type == 0) || (current_type == 0 && adjacent_type == 2)) {
        return 2; // air <-> water => window/skylight
    }

    if ((current_type == 2 && adjacent_type == 1) || (current_type == 1 && adjacent_type == 2)) {
        return 3; // air <-> soil => floor/wall
    }

    return 0; // open for all other transitions for now
}

static void lb_assign_surfaces_from_transitions(
    size_t grid_size,
    const uint8_t *voxel_types,
    uint8_t *surfaces_out,
    size_t faces_per_cell
) {
    if (grid_size == 0 || voxel_types == NULL || surfaces_out == NULL || faces_per_cell == 0) {
        return;
    }

    // Matches CubeFace ordering in Swift: front, right, back, left, top, bottom.
    static const int32_t face_dx[6] = {0, 1, 0, -1, 0, 0};
    static const int32_t face_dy[6] = {0, 0, 0, 0, 1, -1};
    static const int32_t face_dz[6] = {1, 0, -1, 0, 0, 0};

    for (size_t x = 0; x < grid_size; x++) {
        for (size_t y = 0; y < grid_size; y++) {
            for (size_t z = 0; z < grid_size; z++) {
                size_t index = lb_voxel_index(grid_size, x, y, z);
                uint8_t current_type = voxel_types[index];
                uint8_t *surface_start = surfaces_out + (index * faces_per_cell);

                for (size_t face = 0; face < faces_per_cell; face++) {
                    if (face < 6) {
                        int32_t nx = (int32_t)x + face_dx[face];
                        int32_t ny = (int32_t)y + face_dy[face];
                        int32_t nz = (int32_t)z + face_dz[face];
                        if (
                            nx < 0 ||
                            ny < 0 ||
                            nz < 0 ||
                            (size_t)nx >= grid_size ||
                            (size_t)ny >= grid_size ||
                            (size_t)nz >= grid_size
                        ) {
                            surface_start[face] = 0; // no surface types on outside of the cube
                            continue;
                        }
                        uint8_t adjacent_type = lb_voxel_type_at(grid_size, voxel_types, nx, ny, nz);
                        surface_start[face] = lb_surface_type_from_transition(current_type, adjacent_type);
                    } else {
                        /* Extra face channels (if any) are filled with weighted random labels. */
                        surface_start[face] = lb_random_surface_type();
                    }
                }
            }
        }
    }
}

enum {
    LB_FACE_FRONT = 0,
    LB_FACE_RIGHT = 1,
    LB_FACE_BACK = 2,
    LB_FACE_LEFT = 3,
    LB_FACE_TOP = 4,
    LB_FACE_BOTTOM = 5
};

static void lb_set_surface_at(
    size_t grid_size,
    uint8_t *surfaces_out,
    size_t faces_per_cell,
    size_t x,
    size_t y,
    size_t z,
    size_t face,
    uint8_t surface_type
) {
    if (
        surfaces_out == NULL ||
        x >= grid_size ||
        y >= grid_size ||
        z >= grid_size ||
        face >= faces_per_cell
    ) {
        return;
    }

    size_t index = lb_voxel_index(grid_size, x, y, z);
    surfaces_out[(index * faces_per_cell) + face] = surface_type;
}

static void lb_apply_window_layout(
    size_t grid_size,
    uint8_t *surfaces_out,
    size_t faces_per_cell,
    int32_t water_level,
    size_t center_start_x,
    size_t center_end_x,
    size_t center_start_z,
    size_t center_end_z
) {
    if (grid_size == 0 || surfaces_out == NULL || faces_per_cell < 6) {
        return;
    }

    /* Reset transition-derived windows; explicit layout below re-applies them. */
    size_t cell_count = grid_size * grid_size * grid_size;
    for (size_t i = 0; i < cell_count; i++) {
        uint8_t *surface_start = surfaces_out + (i * faces_per_cell);
        for (size_t face = 0; face < 6; face++) {
            if (surface_start[face] == 2) {
                surface_start[face] = 3; // remove all default windows first
            }
        }
    }

    // Require an inner core with perimeter so no windows appear on outer cube edges.
    if (
        center_start_x == 0 ||
        center_start_z == 0 ||
        center_end_x >= grid_size ||
        center_end_z >= grid_size
    ) {
        return;
    }

    int32_t max_y = (int32_t)grid_size - 1;
    int32_t window_start_y = lb_clamp_int32(water_level + 1, 1, max_y);
    int32_t window_end_y = lb_clamp_int32(water_level + 2, window_start_y, max_y);

    // Vertical window band: two voxels above the waterline around the open core.
    for (int32_t y = window_start_y; y <= window_end_y; y++) {
        size_t uy = (size_t)y;

        for (size_t z = center_start_z; z < center_end_z; z++) {
            size_t min_x = center_start_x;
            size_t max_x = center_end_x - 1;

            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, min_x, uy, z, LB_FACE_LEFT, 2);
            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, min_x - 1, uy, z, LB_FACE_RIGHT, 2);

            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, max_x, uy, z, LB_FACE_RIGHT, 2);
            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, max_x + 1, uy, z, LB_FACE_LEFT, 2);
        }

        for (size_t x = center_start_x; x < center_end_x; x++) {
            size_t min_z = center_start_z;
            size_t max_z = center_end_z - 1;

            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, x, uy, min_z, LB_FACE_BACK, 2);
            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, x, uy, min_z - 1, LB_FACE_FRONT, 2);

            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, x, uy, max_z, LB_FACE_FRONT, 2);
            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, x, uy, max_z + 1, LB_FACE_BACK, 2);
        }
    }

    // Skylight ceiling over the open core.
    size_t ceiling_y = (size_t)window_end_y;
    for (size_t x = center_start_x; x < center_end_x; x++) {
        for (size_t z = center_start_z; z < center_end_z; z++) {
            lb_set_surface_at(grid_size, surfaces_out, faces_per_cell, x, ceiling_y, z, LB_FACE_TOP, 2);
            if (ceiling_y + 1 < grid_size) {
                lb_set_surface_at(
                    grid_size,
                    surfaces_out,
                    faces_per_cell,
                    x,
                    ceiling_y + 1,
                    z,
                    LB_FACE_BOTTOM,
                    2
                );
            }
        }
    }

}

void lb_randomize_voxels(size_t grid_size, uint8_t *voxel_types_out, uint8_t *surfaces_out, size_t faces_per_cell) {
    if (grid_size == 0 || voxel_types_out == NULL || surfaces_out == NULL || faces_per_cell == 0) {
        return;
    }

    size_t column_count = grid_size * grid_size;
    int32_t max_height = (int32_t)grid_size - 1;

    /* Keep the top mostly clear (2-3 layers for larger grids). */
    int32_t top_air_layers = 0;
    if (grid_size > 4) {
        top_air_layers = lb_random_int32(2, 3);
    } else if (grid_size > 2) {
        top_air_layers = 2;
    } else if (grid_size > 1) {
        top_air_layers = 1;
    }
    if (top_air_layers > max_height) {
        top_air_layers = max_height;
    }

    int32_t fill_top = max_height - top_air_layers;
    /* Carve a central open shaft that drives window/skylight composition. */
    size_t center_span_x = grid_size >= 5 ? lb_random_size_t(4, 5) : (grid_size >= 4 ? 4 : grid_size);
    size_t center_span_z = grid_size >= 5 ? lb_random_size_t(4, 5) : (grid_size >= 4 ? 4 : grid_size);
    if (center_span_x > grid_size) {
        center_span_x = grid_size;
    }
    if (center_span_z > grid_size) {
        center_span_z = grid_size;
    }

    size_t center_start_x = (grid_size - center_span_x) / 2;
    size_t center_end_x = center_start_x + center_span_x;
    size_t center_start_z = (grid_size - center_span_z) / 2;
    size_t center_end_z = center_start_z + center_span_z;

    size_t center_air_columns = center_span_x * center_span_z;
    size_t active_column_count = column_count > center_air_columns ? (column_count - center_air_columns) : 0;
    if (active_column_count == 0) {
        size_t cell_count = grid_size * grid_size * grid_size;
        for (size_t i = 0; i < cell_count; i++) {
            voxel_types_out[i] = 2; // air
        }
        lb_assign_surfaces_from_transitions(grid_size, voxel_types_out, surfaces_out, faces_per_cell);
        lb_apply_window_layout(
            grid_size,
            surfaces_out,
            faces_per_cell,
            0,
            center_start_x,
            center_end_x,
            center_start_z,
            center_end_z
        );
        return;
    }

    size_t fillable_cells = active_column_count * (size_t)(fill_top + 1);

    /* Enforce a non-trivial amount of both water and soil below the air cap. */
    size_t minimum_material_cells = ((fillable_cells * 40) / 100) + 1;
    size_t max_balanced_minimum = fillable_cells / 2;
    if (max_balanced_minimum == 0) {
        minimum_material_cells = 0;
    } else if (minimum_material_cells > max_balanced_minimum) {
        minimum_material_cells = max_balanced_minimum;
    }

    size_t minimum_combined_cells = minimum_material_cells * 2;
    size_t minimum_levels = (minimum_combined_cells + active_column_count - 1) / active_column_count;
    int32_t minimum_water_level = lb_clamp_int32((int32_t)minimum_levels - 1, 0, fill_top);

    /* Two draws favor mid/high water tables without hard-coding exact bias. */
    int32_t water_level_a = lb_random_int32(minimum_water_level, fill_top);
    int32_t water_level_b = lb_random_int32(minimum_water_level, fill_top);
    int32_t water_level = water_level_a > water_level_b ? water_level_a : water_level_b;

    size_t combined_cells_at_water_level = active_column_count * (size_t)(water_level + 1);
    size_t maximum_soil_cells = 0;
    if (combined_cells_at_water_level > minimum_material_cells) {
        maximum_soil_cells = combined_cells_at_water_level - minimum_material_cells;
    }
    if (maximum_soil_cells < minimum_material_cells) {
        maximum_soil_cells = minimum_material_cells;
    }

    size_t balanced_low = minimum_material_cells;
    size_t balanced_high = maximum_soil_cells;
    if (balanced_high > balanced_low) {
        size_t midpoint = (balanced_low + balanced_high) / 2;
        size_t band = (balanced_high - balanced_low) / 4;

        if (band > 0) {
            if (midpoint > band && midpoint - band > balanced_low) {
                balanced_low = midpoint - band;
            }
            if (midpoint + band < balanced_high) {
                balanced_high = midpoint + band;
            }
        }
    }

    size_t target_soil_cells = lb_random_size_t(balanced_low, balanced_high);

    int32_t soil_center = lb_clamp_int32((int32_t)(target_soil_cells / active_column_count) - 1, 0, water_level);
    int32_t diagonal_span_limit = lb_clamp_int32((water_level / 2) + 1, 1, water_level > 0 ? water_level : 1);
    int32_t diagonal_span = lb_random_int32(1, diagonal_span_limit);

    int32_t soil_low = lb_clamp_int32(soil_center - diagonal_span, 0, water_level);
    int32_t soil_high = lb_clamp_int32(soil_center + diagonal_span, 0, water_level);
    if (soil_low == soil_high && water_level > 0) {
        if (soil_high < water_level) {
            soil_high += 1;
        } else {
            soil_low -= 1;
        }
    }

    uint32_t diagonal_mode = arc4random_uniform(4);
    int32_t jitter_limit = lb_clamp_int32((int32_t)grid_size / 3, 0, water_level > 0 ? water_level : 1);
    int32_t jitter = lb_random_int32(0, jitter_limit);

    /* One height per x/z column; y volume is materialized afterward. */
    int32_t *soil_tops = (int32_t *)malloc(column_count * sizeof(int32_t));
    if (soil_tops == NULL) {
        return;
    }

    size_t soil_cells = 0;
    for (size_t x = 0; x < grid_size; x++) {
        for (size_t z = 0; z < grid_size; z++) {
            size_t column_index = (x * grid_size) + z;
            if (lb_is_in_center_air(x, z, center_start_x, center_end_x, center_start_z, center_end_z)) {
                soil_tops[column_index] = -1; // central core is forced air
                continue;
            }
            soil_tops[column_index] = lb_diagonal_soil_top(
                grid_size,
                x,
                z,
                diagonal_mode,
                soil_low,
                soil_high,
                jitter
            );
            soil_tops[column_index] = lb_clamp_int32(soil_tops[column_index], 0, water_level);
            soil_cells += (size_t)(soil_tops[column_index] + 1);
        }
    }

    lb_adjust_soil_to_target(soil_tops, column_count, water_level, target_soil_cells, &soil_cells);
    lb_raise_soil_protrusions(
        soil_tops,
        column_count,
        water_level,
        fill_top,
        minimum_material_cells,
        &soil_cells
    );
    lb_add_top_air_soil_spikes(soil_tops, column_count, active_column_count, fill_top, top_air_layers);

    /* Convert column tops into concrete voxel types for every (x, y, z). */
    for (size_t x = 0; x < grid_size; x++) {
        for (size_t z = 0; z < grid_size; z++) {
            size_t column_index = (x * grid_size) + z;
            int32_t soil_top = soil_tops[column_index];
            int is_center_air = lb_is_in_center_air(x, z, center_start_x, center_end_x, center_start_z, center_end_z);

            for (size_t y = 0; y < grid_size; y++) {
                size_t index = lb_voxel_index(grid_size, x, y, z);

                uint8_t voxel_type = 2; // air
                if (is_center_air && y > 0) {
                    voxel_type = 2; // central 4/5 x 4/5 core is air above the bottom layer
                } else if ((int32_t)y <= soil_top) {
                    voxel_type = 1; // soil
                } else if ((int32_t)y <= water_level) {
                    voxel_type = 0; // water
                }

                voxel_types_out[index] = voxel_type;
            }
        }
    }

    /*
     * Surface labels are a second pass so they can inspect neighboring voxel
     * types and then apply deterministic window overrides.
     */
    lb_assign_surfaces_from_transitions(grid_size, voxel_types_out, surfaces_out, faces_per_cell);
    lb_apply_window_layout(
        grid_size,
        surfaces_out,
        faces_per_cell,
        water_level,
        center_start_x,
        center_end_x,
        center_start_z,
        center_end_z
    );

    free(soil_tops);
}
