/* HellForge mod SDK 54: every mod's options as a list (option_info), for the HellForge Editor's MOD VALUES view.
 * Included by hellforge_world.h after HfOption (HF_OPTION_...). */
#ifndef HELLFORGE_OPTIONS_H
#define HELLFORGE_OPTIONS_H

#include <stdint.h>

/* One option of any mod (option_info). The texts stay valid for the session; value is the one now. */
typedef struct HfOptionInfo
{
    uint32_t size;           /* sizeof(HfOptionInfo) before the call */
    uint32_t kind;           /* HF_OPTION_TOGGLE, _CHOICE or _SLIDER */
    const char* mod_id;      /* the mod that added it */
    const char* mod_name;    /* its name from its manifest, else its id */
    const char* id;          /* the option's id within its mod */
    const char* label;
    double value, min, max, step; /* min, max and step a slider's */
    uint32_t flags;          /* HfOption.flags: HF_OPTION_EDITOR_ONLY */
    uint32_t choice_count;   /* a choice's names */
    const char* choices[HF_OPTION_MAX_CHOICES];
} HfOptionInfo;

#endif
