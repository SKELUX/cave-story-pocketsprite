/* Interactive native PC runner. Shares gameplay, renderer, mixer and saves. */
#define _POSIX_C_SOURCE 200809L
#include <SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "md/sys.h"
#include "video.h"
#include "presentation.h"
#include "input.h"
#include "assets.h"
#include "music.h"
#include "mixer.h"
#include "save_io.h"
#include "common.h"
#include "dialog.h"
#include "entity.h"
#include "player.h"
#include "stage.h"
#include "gamemode.h"
#include "weapon.h"
#include "hud.h"
#include "menu_view.h"
#include "window.h"
#include "tsc.h"
#include "pause.h"
#include "md/dma.h"

void cave_main(void) __attribute__((noreturn));
const uint32_t BlankData[64]={0};
const char Version[]="Native PC test",Date[]="POCKET   2026.10";
const char Homepage[]="https://github.com/andwn/cave-story-md";
volatile uint8_t vblank;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio;
static pthread_mutex_t audio_lock;
static cs_music *music;
static cs_mixer mixer;
static uint8_t *music_data;
static size_t music_size;
static struct {const int8_t *data;uint32_t length:31;uint32_t packed:1;} samples[256];
static uint16_t pixels[320*240];
static uint32_t rng=1,frame,limit;
static uint64_t deadline;
static int preview=1,fast;
static char **restart_argv;
static struct {uint32_t frame,keys;} scripted_keys[1024];
static unsigned script_count,script_index;
static uint32_t script_keys;
static uint32_t le32(const uint8_t *p) {
    return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
void disable_ints(void) {pthread_mutex_lock(&audio_lock);}
void enable_ints(void) {pthread_mutex_unlock(&audio_lock);}
void mem_init(void) {}
void cs_srand(uint16_t seed) {rng=seed?seed:1;}
uint16_t cs_rand(void) {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (uint16_t)rng;}
void cs_assert_fail(const char *expr,const char *file,int line) {
    fprintf(stderr,"%s:%d: %s (frame %u)\n",file,line,expr,frame);abort();
}
static void close_runtime(void) {
    if(audio) SDL_CloseAudioDevice(audio);
    cs_music_destroy(music);free(music_data);
    SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);
    SDL_Quit();
}
static void finish(void) {
    const char *shot=getenv("CS_DESKTOP_SCREENSHOT");
    if(shot) {
    if(getenv("CS_CONTROL_AUDIT") && frame>=8050) {
        static int old=-1;
        int state=currentWeapon+paused*16+map_view_active*32;
        if(old!=state) {printf("Controls frame=%u weapon=%u inventory=%u map=%u\n",frame,currentWeapon,paused,map_view_active);old=state;}
    }
    unsigned w=preview?80:320,h=preview?64:240;
        FILE *f=fopen(shot,"wb");
        if(f) {
            fprintf(f,"P6\n%u %u\n255\n",w,h);
            for(unsigned i=0;i<w*h;i++) {
                uint16_t p=pixels[i];
                uint8_t rgb[3]={((p>>11)&31)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};
                if(fwrite(rgb,1,3,f)!=3) {perror("screenshot");break;}
            }
            fclose(f);
        }
    }
    printf("Desktop stopped after %u frames. Save status %d.\n",frame,cs_sram_ok());
    printf("Mode %u, stage %u, player (%ld,%ld), health %u, paused %u\n",gamemode,g_stage.id,
        (long)player.x,(long)player.y,player.health,paused);
    if(gamemode==GM_CREDITS) {
        printf("Credits ended=%u rows=%u\n",credit_ended,credit_text.count);
        for(unsigned i=0;i<credit_text.count;i++) printf("Credit: %s\n",credit_text.line[i]);
    }
    if(getenv("CS_TRACE_DIALOGUE")) for(unsigned row=0;row<3;row++)
        printf("Dialogue: %.*s\n",36,windowText[row]);
    close_runtime();fflush(NULL);exit(0);
}
void sys_hard_reset(void) {
    close_runtime();execv(restart_argv[0],restart_argv);perror("restart");exit(1);
}
static void set_view(void) {
    cs_dialog_enabled=preview;
    windowDialogPage=windowDialogWaiting=windowDialogRead=0;
    if(texture) SDL_DestroyTexture(texture);
    int w=preview?80:320,h=preview?64:240;
    texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,w,h);
    if(!texture) {fprintf(stderr,"Texture: %s\n",SDL_GetError());exit(1);}
    SDL_RenderSetLogicalSize(renderer,w,h);
    SDL_RenderSetIntegerScale(renderer,SDL_TRUE);
    SDL_SetWindowSize(window,preview?640:960,preview?512:720);
    SDL_SetWindowTitle(window,preview?"Cave Story - 80x64 diagnostic preview | F2 full view | Esc quit":
        "Cave Story - native PC test | Z jump X shoot Enter pause | F2 80x64 preview");
}
static void events(void) {
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
        if(e.type==SDL_QUIT) finish();
        if(e.type==SDL_KEYDOWN && !e.key.repeat) {
            if(e.key.keysym.sym==SDLK_ESCAPE) finish();
            if(e.key.keysym.sym==SDLK_F2) {preview=!preview;set_view();}
        }
    }
}
uint32_t cs_input_poll(void) {
    events();
    if(gamemode==GM_CONFIG && getenv("CS_CONFIG_AUDIT")) {
        ConfigView view; config_get_view(&view);
        static const unsigned counts[]={11,10,2};
        static int previous=-1;
        if(view.page>=3 || view.count!=counts[view.page] || view.cursor>=view.count) {
            fprintf(stderr,"Invalid config page/cursor/count: %u/%u/%u\n",view.page,view.cursor,view.count);
            exit(2);
        }
        if(previous!=view.page) {
            printf("Config page %u count %u\n",view.page,view.count);
            previous=view.page;
        }
    }
    if(script_count) {
        while(script_index<script_count && scripted_keys[script_index].frame<=frame)
            script_keys=scripted_keys[script_index++].keys;
        return script_keys;
    }
    if(!(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)) return 0;
    const uint8_t *k=SDL_GetKeyboardState(NULL);
    uint32_t keys=0;
    if(k[SDL_SCANCODE_RIGHT]) keys|=CS_KEY_RIGHT;
    if(k[SDL_SCANCODE_LEFT]) keys|=CS_KEY_LEFT;
    if(k[SDL_SCANCODE_UP]) keys|=CS_KEY_UP;
    if(k[SDL_SCANCODE_DOWN]) keys|=CS_KEY_DOWN;
    if(k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_SPACE]) keys|=CS_KEY_A;
    if(k[SDL_SCANCODE_X]) keys|=CS_KEY_B;
    if(k[SDL_SCANCODE_RETURN]) keys|=CS_KEY_START;
    if(k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) keys|=CS_KEY_SELECT;
    return keys;
}
void sys_wait_vblank(void) {
    events();
    if(frame==8050 && getenv("CS_UI_FIXTURE")) {
        /* Host-only visual fixture in a temporary save slot; no progression
         * claim is made from screenshots populated this way. */
        static const uint8_t weapons[]={2,3,4,5,7};
        static const uint8_t items[]={1,2,4,5,6,8,10,11,12,14,15,16};
        for(unsigned i=0;i<sizeof(weapons);i++) player_give_weapon(weapons[i],30);
        for(unsigned i=0;i<sizeof(items);i++) player_give_item(items[i]);
        playerEquipment|=EQUIP_MAPSYSTEM;
        puts("UI fixture: populated equipment for visual review, not progression evidence.");
    }
    static int scene_fixture_entered;
    if(frame==8050 && !scene_fixture_entered && getenv("CS_UI_SCENE")) {
        scene_fixture_entered=1;
        const char *scene=getenv("CS_UI_SCENE");
        if(!strcmp(scene,"credits")) {
            if(getenv("CS_CREDITS_BEST")) system_set_flag(FLAG_CREDITS_BESTEND,TRUE);
            credits_main();
        } else if(!strcmp(scene,"savefailure")) {
            setenv("CS_HOST_FAIL_WRITES","1",1);
        } else if(!strcmp(scene,"shelter")) {
            player.x=(6*16+8)*512;player.y=(10*16+8)*512;
            stage_load(STAGE_GRASSTOWN_SHELTER);
            player.hidden=0;tsc_call_event(200);
        } else if(!strncmp(scene,"dialog-",7)) {
            extern const uint8_t *curCommand;
            static const uint8_t end[]={0x8c}; /* original TSC END */
            curCommand=end;controlsLocked=1;window_open(0);
            const char *text=!strcmp(scene,"dialog-short")?"\nSue?\nIt's me.":
                "A long message needs two pages.\nEvery word must remain readable.\nPlease read this before answering.";
            while(*text) window_draw_char(*text++);
            tscState=TSC_WAITINPUT;
            if(!strcmp(scene,"dialog-stream")) {
                static const uint8_t stream[]="First line.\nSecond line.\nThird line.\nLast line." "\x9a\x8c";
                window_clear();curCommand=stream;windowFlags=WF_TUR;tscState=TSC_RUNNING;
            }
            if(!strcmp(scene,"dialog-prompt")) {window_prompt_open();tscState=TSC_PROMPT;}
        } else if(!strcmp(scene,"portrait")) {
            controlsLocked=1;
            window_set_face(10,1);
            const char *text="Sue?\nYou there?\nIt's me.";
            while(*text) window_draw_char(*text++);
            tscState=TSC_WAITINPUT;
        } else if(!strncmp(scene,"hud-",4)) {
            player_give_weapon(WEAPON_POLARSTAR,0);
            player_give_weapon(WEAPON_MISSILE,200);
            player_give_weapon(WEAPON_FIREBALL,0);
            player_give_weapon(WEAPON_MACHINEGUN,100);
            player_give_weapon(WEAPON_BUBBLER,100);
            currentWeapon=!strcmp(scene,"hud-unlimited")?0:1;
            Weapon *w=&playerWeapon[currentWeapon];
            w->level=3;w->energy=!strcmp(scene,"hud-max")?10:5;
            w->ammo=!strcmp(scene,"hud-empty")?0:(!strcmp(scene,"hud-max")?200:17);
            playerMaxHealth=50;player.health=!strcmp(scene,"hud-low")?1:50;
            if(!strcmp(scene,"hud-clock")) playerEquipment|=EQUIP_CLOCK;
            hud_show();hud_force_redraw();
        } else if(!strcmp(scene,"meters")) {
            extern uint8_t airPercent;
            extern uint16_t bossHealth,bossMaxHealth;
            airPercent=42;showingBossHealth=1;bossHealth=60;bossMaxHealth=100;
        } else if(!strcmp(scene,"reward")) {
            window_open(0);window_show_item(1);
            const char *text="Got an item!";
            while(*text) window_draw_char(*text++);
        }
        dma_flush();
        puts("UI scene fixture: visual review only.");
    }
    if(frame==10200 && getenv("CS_UI_RECOVER_SAVE")) unsetenv("CS_HOST_FAIL_WRITES");
    if(getenv("CS_DIALOG_AUDIT") && (frame==8051 || frame==8110 || frame==8170)) {
        CsDialog d;cs_dialog_layout(&d,windowText);
        printf("Dialog audit %u state=%u page=%u rows=%u answer=%u\n",frame,tscState,windowDialogPage,d.count,promptAnswer);
        printf("Dialog waiting %u\n",windowDialogWaiting);
        for(unsigned i=0;i<d.count;i++) printf("Dialog line: %s\n",d.line[i]);
    }
    if(getenv("CS_CONTROL_AUDIT") && frame>=8050) {
        static int old=-1;
        int state=currentWeapon+paused*16+map_view_active*32;
        if(old!=state) {printf("Controls frame=%u weapon=%u inventory=%u map=%u\n",frame,currentWeapon,paused,map_view_active);old=state;}
    }
    unsigned w=preview?80:320,h=preview?64:240;
    if(preview) cs_present_80(pixels);
    else if(!fast || frame+1==limit) cs_video_render(pixels,w,h,0,0,320,240);
    if(!fast || frame+1==limit) {
        SDL_UpdateTexture(texture,NULL,pixels,w*2);
        SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,NULL,NULL);SDL_RenderPresent(renderer);
    }
    if(!fast) {
        uint64_t now=SDL_GetTicks64();
        if(deadline>now) SDL_Delay((uint32_t)(deadline-now));
        else if(now-deadline>20) deadline=now;
        deadline+=20;
    }
    vblank=1;
    if(++frame==limit) finish();
}
static void audio_callback(void *unused,uint8_t *out,int length) {
    (void)unused;
    int16_t pcm[320];
    disable_ints();
    while(length>0) {
        int count=length<320?length:320;
        if(!cs_music_read(music,pcm,count)) {fprintf(stderr,"Music decode failed\n");cs_music_stop(music);}
        cs_mixer_render(&mixer,pcm,out,count);
        out+=count;length-=count;
    }
    enable_ints();
}
void xgm_init(void) {
    FILE *f=fopen("build/music/12000/cavestory.dat","rb");
    if(!f) {perror("soundtrack");exit(1);}
    fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    if(size<4) {fprintf(stderr,"Invalid soundtrack\n");exit(1);}
    music_size=size;music_data=malloc(music_size);
    if(!music_data || fread(music_data,1,music_size,f)!=music_size) {fprintf(stderr,"Soundtrack read failed\n");exit(1);}
    fclose(f);
    music=cs_music_create();if(!music) abort();cs_mixer_init(&mixer);
    SDL_AudioSpec wanted={0},got;
    wanted.freq=16000;wanted.format=AUDIO_U8;wanted.channels=1;wanted.samples=512;
    wanted.callback=audio_callback;
    audio=SDL_OpenAudioDevice(NULL,0,&wanted,&got,0);
    if(!audio) fprintf(stderr,"Audio device unavailable: %s (continuing muted)\n",SDL_GetError());
    else {SDL_PauseAudioDevice(audio,0);printf("Audio: native 16kHz Opus + PCM effects\n");}
}
void xgm_pcm_set(uint8_t id,const uint8_t *data,uint32_t length) {
    disable_ints();
    samples[id].data=(const int8_t *)data;samples[id].length=length;samples[id].packed=0;
    for(size_t i=0;i<cs_asset_count;i++) if(cs_assets[i].data==data && cs_assets[i].expanded) {
        samples[id].length=cs_assets[i].expanded;samples[id].packed=1;break;
    }
    enable_ints();
}
void xgm_pcm_play(uint8_t id,uint8_t priority,uint16_t channel) {
    disable_ints();cs_mixer_play(&mixer,channel,priority,samples[id].data,samples[id].length,samples[id].packed);enable_ints();
}
void xgm_pcm_loop(uint8_t id,uint16_t channel,uint32_t rate) {
    disable_ints();
    cs_mixer_loop(&mixer,channel,samples[id].data,samples[id].length,rate,samples[id].packed);
    enable_ints();
}
void xgm_music_fade(uint32_t milliseconds) {
    if(milliseconds>60000) milliseconds=60000;
    disable_ints();cs_music_fade(music,milliseconds*16);enable_ints();
}
void xgm_music_pause(void) {disable_ints();cs_music_stop(music);enable_ints();}
void xgm_music_play(const uint8_t *data) {
    size_t length=0;
    for(size_t i=0;i<cs_asset_count;i++) if(cs_assets[i].data==data) {length=cs_assets[i].size;break;}
    disable_ints();int ok=0;
    if(length==16 && !memcmp(data,"CSE1",4)) {
        uint32_t offset=le32(data+4),bytes=le32(data+8);
        if(offset<=music_size && bytes<=music_size-offset) ok=cs_music_start(music,music_data+offset,bytes);
    }
    enable_ints();if(!ok) {fprintf(stderr,"Invalid music descriptor\n");abort();}
}
int main(int argc,char **argv) {
    restart_argv=argv;
    const char *script=getenv("CS_HOST_INPUT");
    if(script) {
        FILE *f=fopen(script,"r");if(!f) {perror(script);return 1;}
        uint32_t when,keys;
        while(fscanf(f,"%u %u",&when,&keys)==2) {
            if(script_count==1024 || (script_count && when<scripted_keys[script_count-1].frame)) abort();
            scripted_keys[script_count].frame=when;scripted_keys[script_count++].keys=keys;
        }
        fclose(f);
    }
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--preview")) preview=1;
        else if(!strcmp(argv[i],"--full")) preview=0;
        else if(!strcmp(argv[i],"--fast")) fast=1;
        else if(!strcmp(argv[i],"--frames") && i+1<argc) limit=strtoul(argv[++i],NULL,10);
        else {fprintf(stderr,"usage: %s [--preview|--full] [--frames N] [--fast]\n",argv[0]);return 1;}
    }
    if(!getenv("CS_HOST_SAVE")) setenv("CS_HOST_SAVE","artifacts/desktop.state",1);
    pthread_mutexattr_t attr;pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&audio_lock,&attr);pthread_mutexattr_destroy(&attr);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)) {fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    window=SDL_CreateWindow("Cave Story",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,720,SDL_WINDOW_RESIZABLE);
    if(!window) {fprintf(stderr,"Window: %s\n",SDL_GetError());return 1;}
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer) {fprintf(stderr,"Renderer: %s\n",SDL_GetError());return 1;}
    set_view();deadline=SDL_GetTicks64()+20;
    puts("Native Cave Story PC test: arrows move, Z/Space jump/confirm, X shoot/cancel,");
    puts("Enter inventory, Shift Select (weapon / cutscene fast-forward / inventory map).");
    puts("F2 switches full view / current 80x64 hardware preview. Esc quits.");
    printf("Save file: %s\n",getenv("CS_HOST_SAVE"));fflush(stdout);
    cave_main();
}
