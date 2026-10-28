#include "md/types.h"

#include "res/tiles.h"
#include "md/vdp.h"

#include "tables.h"

const background_info_def background_info[BACKGROUND_COUNT] = {
	{ NULL, 	PAL2, 2, 0, 0, 0 },
	{ NULL,     PAL0, 0, 8, 8, 0 },
	{ UFTC_bkBlue, PAL2, 0, 8, 8, 0 }, // Mimiga Village, Grasstown, Waterway, Plantation
	{ UFTC_bkBlue, PAL2, 3, 8, 8, 0 }, // Main Artery
	{ NULL, 	PAL2, 5, 0, 0, 0 }, // Balcony
	{ UFTC_bkGard, PAL2, 0, 6, 8, 0 }, // Sand Zone Storehouse
	{ UFTC_bkGray, PAL3, 0, 8, 8, 0 }, // Boulder Chamber
	{ NULL,		PAL2, 1, 0, 0, 0 }, // Outer Wall
	{ UFTC_bkMaze, PAL3, 0, 8, 8, 0 }, // Labyrinth W - PAL_X
	{ UFTC_bkMaze2,PAL3, 0, 8, 8, 0 }, // Labyrinth I, O - PAL_XB
	{ UFTC_bkRed, 	PAL3, 0, 4, 4, 0 }, // Labyrinth M
	{ NULL,		PAL2, 4, 0, 0, 0 }, // Almond, Dark Place
	{ UFTC_bkHell, PAL2, 0, 4, 4, 0 }, // Hell B1 / B2 / B3
	{ UFTC_bkEggs, PAL2, 0, 8, 8, 0 }, // Egg Corridor
	{ UFTC_bkLabB, PAL3, 0, 8, 8, 0 }, // Labyrinth B, Itoh, Seal/Statue Chamber
	{ UFTC_bkFall, PAL2, 0, 8, 8, 0 }, // Fall
	{ UFTC_bkSand, PAL2, 0, 8, 8, 0 }, // Sand Zone
};
