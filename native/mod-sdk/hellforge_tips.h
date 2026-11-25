/* HellForge mod SDK 51: loading screen tips (add_tip) and game texts by id (set_text). Included by hellforge_mod.h. */
#ifndef HELLFORGE_TIPS_H
#define HELLFORGE_TIPS_H

#include <stdint.h>

/* One loading screen tip (SDK 51, add_tip): a new one, a vanilla one with new text, or a vanilla one taken out. The game
 * shows one tip at a time on its transition screens ("TRAINING MANUAL TIPS"), a new one every 15 s, each equally likely. */
typedef struct HfTipDef
{
    uint32_t size;     /* sizeof(HfTipDef) */
    const char* text;  /* UTF-8, shown in every game language; NULL with replaces: that vanilla tip is taken out */
    uint32_t replaces; /* a vanilla tip's text id (docs/modding/ui.md lists them) to change or take out; 0: a new tip */
} HfTipDef;

#endif
