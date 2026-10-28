/* Host-only gameplay diagnostic. Single-threaded, deterministic, no audio.
 * This adapter is never included in PocketSprite builds. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "dialog.h"
#include "md/vdp.h"
#include "video.h"
#include "save_io.h"
#include "input.h"
#include "stage.h"
#include "gamemode.h"
#include "entity.h"
#include "npc.h"
#include "player.h"
#include "tsc.h"
#include "tables.h"
#include "menu_view.h"
#include "system.h"
#include "window.h"
#include "weapon.h"
#include "ai_gen.h"

void cave_main(void);
const uint32_t BlankData[64]={0};
const char Version[]="Native WIP", Date[]="POCKET   2026.10", Homepage[]="https://github.com/andwn/cave-story-md";
volatile uint8_t vblank;
static uint32_t rng=1,frame,limit=1000;
static struct {uint32_t frame,keys;} input_events[1024];
static unsigned input_count,input_index;
static uint32_t current_keys;
static uint16_t last_stage=65535;
static unsigned boss_fixture_kind,boss_fixture_frames,boss_fixture_next,boss_fixture_delay;
static unsigned boss_defeat_started;
static unsigned boss_fixture_changes,boss_fixture_peak,boss_fixture_state=65535;
static int32_t boss_fixture_x,boss_fixture_y;

void cs_srand(uint16_t seed) {rng=seed?seed:1;}
uint16_t cs_rand(void) {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (uint16_t)rng;}
void mem_init(void) {} /* Host libc owns the heap. */
void disable_ints(void) {} /* The diagnostic has no interrupt/audio thread. */
void enable_ints(void) {}
void cs_assert_fail(const char *expr,const char *file,int line) {
    fprintf(stderr,"%s:%d: %s (frame %u stage %u)\n",file,line,expr,frame,g_stage.id);abort();
}
static void finish(void) {
    uint16_t *pixels=malloc(320*240*2);if(!pixels) abort();
    cs_video_render(pixels,320,240,0,0,320,240);
    const char *shot=getenv("CS_HOST_SCREENSHOT");
    FILE *f=fopen(shot?shot:"artifacts/host-frame.ppm","wb");if(!f) abort();
    fprintf(f,"P6\n320 240\n255\n");
    for(unsigned i=0;i<320*240;i++) {
        uint8_t rgb[3]={((pixels[i]>>11)&31)*255/31,((pixels[i]>>5)&63)*255/63,(pixels[i]&31)*255/31};
        if(fwrite(rgb,1,3,f)!=3) abort();
    }
    fclose(f);free(pixels);
    printf("Host diagnostic: %u frames, stage %u, %ux%u map, save status %d. No audio.\n",
           frame,g_stage.id,g_stage.pxm.width,g_stage.pxm.height,cs_sram_ok());
    printf("Mode %u, player (%ld,%ld), health %u\n",gamemode,(long)player.x,(long)player.y,player.health);
    if(gamemode==GM_CREDITS) {
        printf("Credits ended=%u rows=%u\n",credit_ended,credit_text.count);
        for(unsigned i=0;i<credit_text.count;i++) printf("Credit: %s\n",credit_text.line[i]);
    }
    if(boss_fixture_kind && !boss_defeat_started) printf("Boss fixture kind=%u frames=%u changes=%u peak=%u health=%u state=%u\n",
        boss_fixture_kind,boss_fixture_frames,boss_fixture_changes,boss_fixture_peak,
        bossEntity?bossEntity->health:0,bossEntity?bossEntity->state:65535);
    if(boss_defeat_started) printf("Boss defeat kind=%u lastEvent=%u stage=%u tsc=%u\n",boss_fixture_kind,lastRunEvent,g_stage.id,tscState);
    /* The engine owns live entities at exit; skip teardown-only leak reports. */
    fflush(NULL);_Exit(0);
}
void sys_wait_vblank(void) {
    static int fixture_entered;
    if(frame>=8050 && !fixture_entered && getenv("CS_HOST_PIGNON")) {
        fixture_entered=1;
        Bullet saved[MAX_BULLETS];memcpy(saved,playerBullet,sizeof(saved));
        memset(playerBullet,0,sizeof(saved));
        for(unsigned clone=0;clone<2;clone++) for(unsigned type=0;type<WEAPON_COUNT;type++)
        for(unsigned live=0;live<2;live++) {
            Entity e={.type=clone?OBJ_MA_PIGNON_CLONE:OBJ_MA_PIGNON,
                      .state=110,.y=32*512,.flags=NPC_SHOOTABLE};
            playerBullet[MAX_BULLETS-1].type=type;
            playerBullet[MAX_BULLETS-1].ttl=live;
            ai_ma_pignon(&e);
            int shield=!clone && live && (type==WEAPON_MISSILE || type==WEAPON_SUPERMISSILE ||
                type==WEAPON_BLADE || type==WEAPON_BLADE_SLASH);
            ASSERT_COLOR(0,!!(e.flags&NPC_INVINCIBLE)==shield);
            ASSERT_COLOR(0,!!(e.flags&NPC_SHOOTABLE)==!shield);
            playerBullet[MAX_BULLETS-1].ttl=0;e.timer=0;ai_ma_pignon(&e);
            ASSERT_COLOR(0,!(e.flags&NPC_INVINCIBLE) && (e.flags&NPC_SHOOTABLE));
        }
        const unsigned states[]={201,311,501};
        for(unsigned i=0;i<3;i++) {
            Entity e={.type=OBJ_MA_PIGNON,.state=states[i],.y=32*512,.flags=NPC_INVINCIBLE};
            ai_ma_pignon(&e);
            ASSERT_COLOR(0,e.flags==NPC_INVINCIBLE);
        }
        memcpy(playerBullet,saved,sizeof(saved));
        puts("Pignon: live/dead weapon types, clones, shield release and scripted immunity passed.");
    }
    if(frame>=8050 && !fixture_entered && getenv("CS_HOST_CREDITS")) {
        fixture_entered=1;
        if(getenv("CS_CREDITS_BEST")) system_set_flag(FLAG_CREDITS_BESTEND,TRUE);
        credits_main();
    }
    if(frame>=8050 && !fixture_entered && getenv("CS_HOST_STAGE")) {
        fixture_entered=1;
        unsigned stage,x,y;
        if(sscanf(getenv("CS_HOST_STAGE"),"%u,%u,%u",&stage,&x,&y)!=3 || stage==0 || stage>=STAGE_COUNT) abort();
        player.x=(x*16+8)*512;player.y=(y*16+8)*512;
        player.x_speed=player.y_speed=0;player.hidden=0;
        window_close();stage_load(stage);stage_setup_palettes();vdp_colors_apply_next_now();
        tscState=TSC_IDLE;gameFrozen=controlsLocked=0;
        printf("Stage fixture loaded %u at %u,%u; not progression evidence.\n",stage,x,y);
        if(getenv("CS_HOST_BOSS")) {
            unsigned initial;
            if(sscanf(getenv("CS_HOST_BOSS"),"%u,%u,%u,%u",&boss_fixture_kind,&initial,
                      &boss_fixture_next,&boss_fixture_delay)!=4 || boss_fixture_kind<1 || boss_fixture_kind>9) abort();
            if(!bossEntity) bossEntity=entity_create(0,0,360+boss_fixture_kind,0);
            if(!bossEntity || bossEntity->type!=360+boss_fixture_kind) abort();
            bossEntity->state=initial;
            if(boss_fixture_kind==2 || boss_fixture_kind==3 || boss_fixture_kind==4) bossEntity->event=1000;
            boss_fixture_x=player.x;boss_fixture_y=player.y;
            playerMaxHealth=50;player.health=50;playerEquipment|=EQUIP_AIRTANK;
            puts("Boss AI stress fixture: fixed invulnerable target, scripted initial state; not combat/progression evidence.");
        }
    }
    if(frame>=8050 && !fixture_entered && getenv("CS_HOST_TSC_AUDIT")) {
        fixture_entered=1;
        extern const uint8_t *curCommand;
        extern uint8_t execute_command(void);
        /* Operand bytes deliberately resemble branch opcodes. */
        static const uint8_t commands[]={0xc6,0x8d,0,0x8e,0,0xc0,0xd2,0x04,0x8c};
        curCommand=commands;tscState=TSC_RUNNING;
        execute_command();if(curCommand!=commands+5) abort();
        execute_command();if(!system_get_flag(1234) || curCommand!=commands+8) abort();
        execute_command();if(tscState!=TSC_IDLE) abort();
        puts("TSC runtime: boss-focus operands consumed; following flag command and END executed.");
    }

    if(boss_fixture_kind && !boss_defeat_started) {
        if(!bossEntity || (bossEntity->type!=360+boss_fixture_kind &&
           !(boss_fixture_kind==9 && (bossEntity->type==OBJ_BALLOS_FORM2 || bossEntity->type==OBJ_BALLOS_FORM3)))) {
            fprintf(stderr,"Boss disappeared at fixture frame %u\n",boss_fixture_frames);abort();
        }
        playerIFrames=200;player.health=50;
        player.x=boss_fixture_x;player.y=boss_fixture_y;player.x_speed=player.y_speed=0;
        if(boss_fixture_delay && boss_fixture_frames==boss_fixture_delay) bossEntity->state=boss_fixture_next;
        if(boss_fixture_frames==3000) bossEntity->health=bossEntity->health/3+1;
        if(getenv("CS_HOST_BOSS_PHASES")) {
            if(boss_fixture_kind==3 && boss_fixture_frames==2500) {
                for(unsigned i=4;i<8;i++) {pieces[i]->health=0;npc_info[pieces[i]->type].onDeath(pieces[i]);}
                puts("Fixture damage: Monster X targets defeated via original callbacks.");
            }
            if(boss_fixture_kind==9 && boss_fixture_frames==1500) {
                bossEntity->health=0;npc_info[bossEntity->type].onDeath(bossEntity);
                puts("Fixture damage: Ballos first form defeated; original event 1000 runs.");
            }
            if(boss_fixture_kind==9 && boss_fixture_frames==4000) {
                for(Entity *e=entityList;e;e=e->next) if(e->type==OBJ_BALLOS_ROTATOR) e->health=900;
                puts("Fixture damage: Ballos rotators reduced by 100 HP for original phase transition.");
            }
        }
        if(boss_fixture_state!=bossEntity->state) {
            boss_fixture_state=bossEntity->state;boss_fixture_changes++;
            printf("Boss state %u at %u\n",boss_fixture_state,boss_fixture_frames);
        }
        unsigned count=entities_count_active();if(count>boss_fixture_peak) boss_fixture_peak=count;
        if(getenv("CS_HOST_BOSS_DEFEAT") && boss_fixture_frames==(boss_fixture_kind==9?8000:5000)) {
            boss_defeat_started=1;
            bossEntity->health=0;npc_info[bossEntity->type].onDeath(bossEntity);
            puts("Fixture lethal damage: original boss death callback and event now run.");
        }
        boss_fixture_frames++;
    }
    if(boss_defeat_started) {playerIFrames=200;player.health=50;}
    if(g_stage.id!=last_stage) {
        printf("Stage %u at frame %u\n",g_stage.id,frame);fflush(stdout);last_stage=g_stage.id;
    }
    vblank=1;if(++frame>=limit) finish();
}
void sys_hard_reset(void) {finish();_Exit(0);}
uint32_t cs_input_poll(void) {
    if(boss_defeat_started) return frame%60<2?64:0;
    while(input_index<input_count && input_events[input_index].frame<=frame)
        current_keys=input_events[input_index++].keys;
    return current_keys;
}
int main(int argc,char **argv) {
    cs_dialog_enabled=0;
    if(argc>1) limit=strtoul(argv[1],NULL,10);
    const char *script=getenv("CS_HOST_INPUT");
    if(script) {
        FILE *f=fopen(script,"r");if(!f) {perror(script);return 1;}
        uint32_t when,keys;
        while(fscanf(f,"%u %u",&when,&keys)==2) {
            if(input_count==1024 || (input_count && when<input_events[input_count-1].frame)) abort();
            input_events[input_count].frame=when;input_events[input_count++].keys=keys;
        }
        fclose(f);
    }
    cave_main();
}

/* No presentation buffers in the logic-only host adapter. */
void cs_present_commit(void) {}
