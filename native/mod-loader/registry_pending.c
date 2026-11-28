/* The order the pending weapons are registered in (registry_load.c RegisterWeapons). Included by registry_load.c,
 * after RegisterWeapon and PendingBaseWeapon. */

/* In passes: one whose base is another pending one waits until that is in, whatever their order. The order is the same
 * on every machine, so short ids still agree. A base that never comes in (a loop) is refused in a last pass. */
static void RegisterPending(WeaponLoad* L, const int* order)
{
    for (int progress = 1, waiting = 1, last = 0; waiting; last = !progress)
    {
        progress = waiting = 0;
        for (int k = 0; k < g_weaponCount; k++)
        {
            if (g_weapons[order[k]].state != 0) continue;
            if (!last && PendingBaseWeapon(&g_weapons[order[k]], 0) != NULL) waiting = 1;
            else { RegisterWeapon(L, &g_weapons[order[k]]); progress = 1; }
        }
    }
}
