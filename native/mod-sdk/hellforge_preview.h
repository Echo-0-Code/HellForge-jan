/* HellForge mod SDK 46: an item's fit in the Armory's 3D preview (set_preview_fit). Included by hellforge_mod.h. */
#ifndef HELLFORGE_PREVIEW_H
#define HELLFORGE_PREVIEW_H

#include <stdint.h>

/* An item's size and place in the Armory's 3D preview (SDK 46, set_preview_fit). The game fits the camera to the
 * unit's bounding box, which misses parts some models draw outside it. */
typedef struct HfPreviewFit
{
    uint32_t size;    /* sizeof(HfPreviewFit) */
    float scale;      /* how big it shows against the game's fit: 0.5 half as big; 0.05 .. 20 (0 in set_preview_fit: clear) */
    float offset[3];  /* metres the model moves in the preview's own axes (z up), -5 .. 5; 0 0 0 keeps the game's place */
    float thumb_scale; /* the grid thumbnail's scale against the game's fit, 0.05 .. 20; 0: scale (a smaller struct: 0) */
} HfPreviewFit;

#endif
