#include "common.h"
#include "presentation.h"
#include "input.h"
#include "player.h"
#include "pause.h"

#include "audio.h"
#include "md/dma.h"
#include "effect.h"
#include "gamemode.h"
#include "md/joy.h"
#include "md/stdlib.h"
#include "resources.h"
#include "stage.h"
#include "system.h"
#include "md/comp.h"
#include "tsc.h"
#include "md/sys.h"
#include "md/vdp.h"
#include "md/xgm.h"

volatile uint8_t ready;

uint8_t gamemode;
uint8_t paused;
uint8_t gameFrozen;

void aftervsync(void) {
    vdp_fade_step_dma();
	dqueued = FALSE;
	if(ready) {
		if(inFade) vdp_sprites_clear();
		vdp_sprites_update();
		ready = FALSE;
	}
	dma_flush();
    vdp_fade_step_calc();
    cs_input_set_context(map_view_active || paused ? CS_INPUT_INVENTORY :
        gamemode!=GM_GAME ? CS_INPUT_MENU :
        controlsLocked || tscState ? CS_INPUT_CUTSCENE : CS_INPUT_GAME);
    joy_update();
    cs_present_commit();
}

__attribute__((noreturn))
void cave_main(void) {
    srand(0xC427); // initiate rand number generator
    mem_init();
    vdp_init();
#ifndef NSOUND
	xgm_init();
#endif
    cs_system_init();
    g_stage.back_id = 0xFF;
	if(system_checkdata() != SRAM_INVALID) {
		system_load_config();
	}
	joy_init();
    // Initialize time and speed tables (framerate adjusted)
    if(use_pal_speed) {
		time_tab = time_tab_pal;
        speed_tab = speed_tab_pal;
	} else {
        time_tab = time_tab_ntsc;
        speed_tab = speed_tab_ntsc;
	}
    sound_init();
	
	// Error Tests
	//__asm__("move.w (1),%d0"); // Address Error
	//__asm__("illegal"); // Illegal Instruction
	//__asm__("divu #0,%d0"); // Zero Divide

	//splash_main();
	intro_main();
    while(TRUE) {
        sound_ambient_stop();
		uint8_t select = titlescreen_main();
		if(select == 0) {
			select = saveselect_main();
			if(select >= 4) continue;
			game_main(select);
			credits_main();
		} else if(select == 2) {
			soundtest_main();
		} else if(select == 3) {
			config_main();
		} else {
			game_main(select);
			credits_main();
		}
    }
}
