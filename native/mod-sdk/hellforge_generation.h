/* HellForge mod SDK: room in the map generator's arrays (generation_capacity in hellforge_mod.h). Included by
 * hellforge_mod.h. */
#ifndef HELLFORGE_GENERATION_H
#define HELLFORGE_GENERATION_H

#include <stdint.h>

/* The block's regions, as HfGenerationCapacity.regions names them. An element's size in bytes: the locations' two
 * arrays 0xE0 and 0x40, their types 0x10, the minimap's four lists 0x30, 0x58, 4 and 4, a mission stamp 0x10, a place
 * 8, a place set entry 0x10, a cluster 0xE0, an important location 0x108, a linked one 0x18, a route node 0x98, an
 * edge 0x48, its class 1, a road point 8. The objective list is one record: `at` is its row field (the ids at +4),
 * `places` the objectives it holds. Later SDKs add names before HF_GEN_REGIONS; unnamed entries are 0. */
enum { HF_GEN_LOCATIONS_A, HF_GEN_LOCATIONS_B, HF_GEN_LOCATION_TYPES, HF_GEN_MINIMAP_PROXIES, HF_GEN_MINIMAP_SHAPES, HF_GEN_MINIMAP_U,
    HF_GEN_MINIMAP_V, HF_GEN_MISSION_STAMPS, HF_GEN_OBJECTIVE_LIST, HF_GEN_PLACES, HF_GEN_PLACE_SET, HF_GEN_CLUSTERS, HF_GEN_IMPORTANT,
    HF_GEN_LINKED, HF_GEN_ROUTE_NODES, HF_GEN_ROUTE_EDGES, HF_GEN_ROUTE_CLASSES, HF_GEN_ROUTE_POINTS, HF_GEN_REGIONS = 32 };

/* Room in the map generator's arrays, for a mod that puts more on a map than level 11 does (generation_capacity). The
 * arrays are HellForge's enlarged ones, which exist when a mod added level 11; a field left 0 keeps HellForge's size.
 * The game appends to them without a check, except where noted: ask for what your content makes at most.
 *
 *     HfGenerationCapacity want = {sizeof want};
 *     want.locations = 40000;
 *     if (hf->size >= offsetof(HfApi, generation_capacity) + sizeof(void*)) hf->generation_capacity(mod, &want, NULL); */
typedef struct HfGenerationCapacity
{
    uint32_t size, locations; /* sizeof(HfGenerationCapacity); everything placed on the map (objectives, outposts,
                                 points of interest, placed stamps): 8192, up to 65024 */
    uint32_t minimap_proxies, mission_stamps; /* the minimap's shapes, one or more per location and prop: 16384, up to
                                 32768, in steps of 4096 (its other lists grow in proportion). The objectives'
                                 candidate stamps: 1024, up to 16384; with a request, a stamp past the last place is
                                 left out */
    uint32_t places, place_set; /* objective and extraction placement attempts (512, up to 8192) and their set (256,
                                   up to 4096) */
    uint32_t block_bytes, moved; /* filled in: the size of the generator's block with these capacities; 1 once the
                                    game's code uses them (level 11 is installed with them), 0 before that or when it
                                    kept its own arrays */
    /* Lists HellForge leaves where the game has them unless a mod asks for more than the game's own places:
     * the important locations (objectives, extraction, outposts, points of interest and their satellites: 256, up
     * to 1024) and the linked ones (a satellite and the location it belongs to: 64, up to 1024). The route graph,
     * moved the same way: its nodes (975, up to 2304), its edges (1621, up to 3456) and the road points along them
     * (9936, up to 65520). The game reads the route capacities; it appends nodes and edges unchecked. */
    uint32_t important_locations, linked_locations, route_nodes, route_edges, route_points;
    /* Filled in: where each region lies, as an offset from the block's start, and the elements it holds. A list left
     * where the game has it reports the game's own offset. They hold for the game's code once `moved` is 1. */
    struct { uint32_t at, places; } regions[HF_GEN_REGIONS];
} HfGenerationCapacity;

#endif
