#include "common.h"
#include "entity.h"
#include "player.h"
#include "stage.h"
#include "camera.h"
#include "gamemode.h"
#include "md/joy.h"
#include "md/vdp.h"
#include "window.h"
#include "tsc.h"
#include "video.h"
#include "ui.h"
#include "dialog.h"
#include "viewport.h"
#include "scene_focus.h"
#include "save_io.h"
#include "presentation.h"
#include "menu_view.h"
#include "weapon.h"
#include "system.h"
#include "tables.h"
#include "npc.h"
#include "res/local.h"
#include <stdio.h>
#include <string.h>
#ifndef ESP_PLATFORM
#include <stdlib.h>
#endif
extern int8_t selectedItem;
extern Sprite itemSprite[];
extern uint8_t hudEnergyPixel,hudMaxEnergy,hudMaxBlink,showing,swapWepNum;
extern uint8_t airPercent,airDisplayTime;
extern uint16_t bossHealth,bossMaxHealth,showingItem;
extern Sprite handSpr;
extern uint8_t teleMenuActive,teleMenuSlotCount,teleMenuSelection;
extern Sprite teleMenuSprite[8];
static void panel(uint16_t *p,int x,int y,int w,int h) {
    cs_ui_frame(p,x,y,w,h,cs_video_color(2),cs_video_color(3),cs_video_color(8),cs_video_color(1));
}
static void compact(uint16_t *p,int x,int y,const char *text) {
    cs_ui_text(p,x+1,y+1,text,cs_video_color(1));
    cs_ui_text(p,x,y,text,cs_video_color(15));
}
static void title_logo(uint16_t *p) {
    /* Redrawn small serif wordmark, not a shrunken gameplay framebuffer. */
    static const uint8_t glyph[9][7]={
        {14,25,16,16,16,25,14}, /* C */
        {0,0,14,1,15,19,13},  /* a */
        {0,0,27,10,10,4,4},  /* v */
        {0,0,14,17,31,16,15}, /* e */
        {15,24,16,14,1,3,30}, /* S */
        {4,4,14,4,4,5,2},    /* t */
        {0,0,14,17,17,17,14},/* o */
        {0,0,22,25,16,16,24},/* r */
        {0,0,27,10,6,4,24},  /* y */
    };
    for(unsigned i=0;i<9;i++) for(unsigned y=0;y<7;y++) for(unsigned x=0;x<5;x++)
        if(glyph[i][y]&(16>>x)) cs_ui_rect(p,12+i*6+(i>=4?3:0)+x,4+y,1,1,cs_video_color(12));
    cs_ui_rect(p,11,12,58,1,cs_video_color(12));
    compact(p,4,15,"Doukutsu Monogatari");
}
static unsigned ui_tick;
static void quote_cursor(uint16_t *p,int x,int y) {
    cs_video_ui_icon(p,x,y,TILE_ATTR(PAL1,0,0,0,TILE_PROMPTINDEX+4),2,2,8,8);
}
static void selection(uint16_t *p,int x,int y,int w,int h) {
    uint16_t c=cs_video_color(8);
    cs_ui_rect(p,x,y,3,1,c);cs_ui_rect(p,x,y,1,3,c);
    cs_ui_rect(p,x+w-3,y+h-1,3,1,c);cs_ui_rect(p,x+w-1,y+h-3,1,3,c);
}
static void inventory(uint16_t *p) {
    panel(p,0,0,80,43);
    compact(p,5,3,"--ARMS--");
    if(selectedItem<0 && selectedItem>=-6 && selectedItem+6<MAX_WEAPONS) {
        Weapon *w=&playerWeapon[selectedItem+6];char ammo[12];
        if(w->maxammo) snprintf(ammo,sizeof(ammo),"%u/%u",w->ammo,w->maxammo);
        else snprintf(ammo,sizeof(ammo),"--/--");
        compact(p,53,3,ammo);
    }
    for(unsigned i=0;i<MAX_WEAPONS;i++) {
        Weapon *w=&playerWeapon[i];
        if(!w->type) continue;
        int x=5+i*14;
        cs_video_ui_icon(p,x+1,10,TILE_FACEINDEX+16+i*4,2,2,10,8);
        char level[4];snprintf(level,sizeof(level),"L%u",w->level);
        compact(p,x+2,18,level);
        cs_ui_rect(p,x+1,17,10,1,cs_video_color(1));
        if(w->next) cs_ui_rect(p,x+1,17,(w->energy>w->next?w->next:w->energy)*10/w->next,1,cs_video_color(8));
        if(selectedItem==(int)i-6) selection(p,x,9,13,15);
    }
    compact(p,5,25,"--ITEM--");
    int row=selectedItem>=0?selectedItem/6:0;
    char page[6];snprintf(page,sizeof(page),"%d/4",row+1);compact(p,61,25,page);
    for(int col=0;col<6;col++) {
        int index=row*6+col,x=4+col*12;
        if(playerInventory[index]) cs_video_ui_icon(p,x,32,itemSprite[index].attr,3,2,12,8);
        if(selectedItem==index) selection(p,x,31,12,10);
    }
}
/* Preserve every description character, wrapping at words and cycling complete
 * pages. Reset on selection/text changes, so the first page is never skipped. */
static void description(uint16_t *p) {
    static uint8_t previous[3][36];
    static int previous_item=-100;
    static unsigned since;
    if(previous_item!=selectedItem || memcmp(previous,windowText,sizeof(previous))) {
        memcpy(previous,windowText,sizeof(previous));previous_item=selectedItem;since=ui_tick;
    }
    char lines[12][19]={{0}};unsigned count=0;
    for(unsigned row=0;row<3;row++) {
        unsigned length=0;
        while(length<36 && windowText[row][length]) length++;
        while(length && windowText[row][length-1]==' ') length--;
        unsigned start=0;
        while(start<length && count<12) {
            unsigned end=start+18;if(end>length) end=length;
            if(end<length) {
                unsigned word=end;while(word>start && windowText[row][word]!=' ') word--;
                if(word>start) end=word;
            }
            memcpy(lines[count++],windowText[row]+start,end-start);
            start=end;while(start<length && windowText[row][start]==' ') start++;
        }
    }
    unsigned pages=(count+2)/3;if(!pages) pages=1;
    unsigned page=((ui_tick-since)/200)%pages;
    panel(p,0,40,80,24);
    for(unsigned i=0;i<3 && page*3+i<count;i++) compact(p,4,43+i*6,lines[page*3+i]);
    if(pages>1) for(unsigned i=0;i<pages;i++)
        cs_ui_rect(p,66+i*3,40,2,1,cs_video_color(i==page?15:8));
}
static void settings(uint16_t *p) {
    ConfigView v;config_get_view(&v);
    static const char *pages[]={"Controller","Gameplay","Save Data"};
    cs_ui_rect(p,0,0,80,64,cs_video_color(0));
    compact(p,2,2,"<");compact(p,14,2,pages[v.page%3]);compact(p,73,2,">");
    static const char *captions[3][12]={
        {"Jump / OK","Shoot / Back","Weapon / FF","Fast text (6B)","Next weapon","Prev weapon","Open map","Pause menu","Button mode","Apply","Defaults"},
        {"CS+ speed","Fast forward","Up interacts","Hell shake","Pause damage","Text sound","Mute music","Mute sounds","Apply","Defaults"},
        {"Erase counter","Erase saves!"}
    };
    unsigned start=v.cursor>2?v.cursor-2:0;
    for(unsigned row=0;row<6 && start+row<v.count;row++) {
        unsigned i=start+row;int y=12+row*8;
        if(i==v.cursor) quote_cursor(p,1,y-1);
        compact(p,10,y,captions[v.page%3][i]);
        if(v.kind[i]==2) compact(p,66,y,v.value[i]?"ON":"OFF");
        if(v.kind[i]==5) compact(p,66,y,v.value[i]==0?"OFF":v.value[i]==1?"3B":"6B");
        if(v.kind[i]==1) {
            static const char *buttons[]={"U","D","L","R","B","A","S","P",">","<","M","?"};
            compact(p,73,y,buttons[v.value[i]<12?v.value[i]:11]);
        }
    }
    if(v.capturing) {panel(p,0,22,80,22);compact(p,10,30,"Press button");}
    if(v.confirming) {
        panel(p,0,14,80,42);
        compact(p,6,20,"Erase ALL saves?");
        compact(p,6,31,"Start: 3 times");
        compact(p,6,43,"B: Cancel");
    }
}
static void sound_menu(uint16_t *p) {
    cs_video_menu_background(p);
    /* Keep the original mint backdrop and right-aligned Curly illustration,
     * reserving the left column for readable status and track information. */
    uint16_t mint=p[0];
    cs_ui_rect(p,0,0,58,40,mint);
    cs_ui_rect(p,0,40,80,9,mint);
    compact(p,20,2,"Sound Test");
    compact(p,4,13,sound_view_status==1?"Playing":sound_view_status==2?"Paused":"Stopped");
    char label[33];snprintf(label,sizeof(label),"Track: %02X",sound_view_track);
    compact(p,4,23,label);
    if(sound_view_track<SONG_COUNT) {
        const char *name=(const char*)MUSIC_NAMES+32*sound_view_track;
        snprintf(label,sizeof(label),"%.32s",name);
        unsigned len=strlen(label),offset=len>13?(ui_tick/35)%(len-12):0;
        char visible[14];snprintf(visible,sizeof(visible),"%.13s",label+offset);
        compact(p,4,32,visible);
    } else compact(p,4,32,"Sound effect");
    compact(p,2,42,"A:Play B:Stop S:Exit");

}
static void map_menu(uint16_t *p) {
    cs_ui_rect(p,0,0,80,64,cs_video_color(1));
    int px=player.x/(16*512),py=player.y/(16*512);
    int left=px-36,top=py-24;
    if(left<0) left=0;
    if(top<0) top=0;
    if(g_stage.pxm.width<72) left=-((72-g_stage.pxm.width)/2);
    if(g_stage.pxm.height<48) top=-((48-g_stage.pxm.height)/2);
    for(int y=0;y<48;y++) for(int x=0;x<72;x++) {
        int bx=left+x,by=top+y;
        if(bx<0 || by<0 || bx>=g_stage.pxm.width || by>=g_stage.pxm.height) continue;
        uint8_t type=stage_get_block_type(bx,by);
        unsigned color=type==0x01?9:type==0x41?11:type==0x43?10:1;
        p[(y+12)*80+x+4]=cs_video_color(color);
    }
    int x=px-left+4,y=py-top+12;
    cs_ui_rect(p,x-1,y,3,1,cs_video_color(15));cs_ui_rect(p,x,y-1,1,3,cs_video_color(15));
}
static void tiny(uint16_t *p,int x,int y,const char *text) {
    cs_ui_tiny(p,x+1,y+1,text,cs_video_color(1));
    cs_ui_tiny(p,x,y,text,cs_video_color(15));
}
static void slim_bar(uint16_t *p,int x,int y,int width,unsigned value,unsigned maximum,unsigned color) {
    cs_ui_rect(p,x,y,width,3,cs_video_color(15));
    cs_ui_rect(p,x,y+1,width,1,cs_video_color(1));
    if(maximum) {
        if(value>maximum) value=maximum;
        unsigned fill=(value*width+maximum-1)/maximum;
        cs_ui_rect(p,x,y+1,fill,1,cs_video_color(color));
    }
}
static void game_hud(uint16_t *p) {
    if(!showing) return;
    Weapon *w=&playerWeapon[currentWeapon];
    char text[20];
    if(w->type) {
        /* HUD icon uses a four-tile column stride; render each 8x8 quadrant
         * into a 4x4 quadrant so the familiar weapon silhouette stays intact. */
        for(unsigned x=0;x<2;x++) for(unsigned y=0;y<2;y++)
            cs_video_ui_icon(p,1+x*4,y*4,TILE_HUDINDEX+x*4+y,1,1,4,4);
        if(w->maxammo) {
            snprintf(text,sizeof(text),"%u/%u",w->ammo,w->maxammo);
            tiny(p,11,0,text);
        } else {
            /* Original unlimited-ammunition symbol, drawn as a small figure eight. */
            static const uint8_t infinity[]={0x36,0x49,0x36};
            for(int y=0;y<3;y++) for(int x=0;x<7;x++) if(infinity[y]&(1<<x))
                cs_ui_rect(p,12+x,1+y,1,1,cs_video_color(15));
        }
        snprintf(text,sizeof(text),"Lv%u",w->level);tiny(p,11,6,text);
        if(w->energy==hudMaxEnergy && !hudMaxBlink) tiny(p,26,6,"MAX");
        else slim_bar(p,25,7,22,hudEnergyPixel,40,8);
        /* Remaining weapons retain the original order, alongside the active one. */
        for(unsigned i=0;i<swapWepNum && i<4;i++)
            cs_video_ui_icon(p,48+i*8,0,TILE_EXWEPINDEX+i*4,2,2,8,8);
    }
    slim_bar(p,1,13,30,player.health>0?player.health:0,playerMaxHealth,6);
    snprintf(text,sizeof(text),"%u",player.health>0?player.health:0);tiny(p,33,12,text);
    if(playerEquipment&EQUIP_CLOCK) {
        unsigned ticks=system_counter_ticks();
        snprintf(text,sizeof(text),"%02u:%02u.%u",ticks/3000,(ticks/50)%60,(ticks%50)/5);
        tiny(p,52,11,text);
    }
    if(airPercent<100 || airDisplayTime) {
        snprintf(text,sizeof(text),"AIR %u",airPercent);
        tiny(p,26,showingBossHealth?52:58,text);
    }
    if(showingBossHealth && bossMaxHealth) {
        tiny(p,1,58,"BOSS");
        slim_bar(p,19,60,60,bossHealth,bossMaxHealth,6);
    }
}
static CsViewport view;
static uint16_t last_stage=65535;
static struct {
    CsViewTarget target;
    int origin_x,origin_y,stage,valid;
} committed;
void cs_present_commit(void) {
    committed.valid=gamemode==GM_GAME && !paused;
    if(!committed.valid) return;
    Entity *focus=&player;
    int scene=controlsLocked || player.hidden;
    if(scene) focus=cs_scene_focus(&player,entityList,scene_camera_explicit?camera.target:NULL,
                                   scene_actor_event,showingFace,window_is_open());
    committed.target=(CsViewTarget){
        .x=focus->x,.y=focus->y,.vx=focus->x_speed,.vy=focus->y_speed,
        .width=g_stage.pxm.width*16,.height=g_stage.pxm.height*16,
        .right=focus->dir!=0,.up=joy_down(JOY_UP),.down=joy_down(JOY_DOWN),
        .grounded=focus->grounded,.cutscene=scene
    };
    if(window_is_open()) committed.target.y+=10*512;
    committed.origin_x=(int16_t)camera.x_shifted;
    committed.origin_y=(int16_t)camera.y_shifted;
    committed.stage=g_stage.id;
}
static int pixel_floor(int32_t position) {
    return position>=0?position/512:-((-position+511)/512);
}
static void present(uint16_t *pixels) {
    ui_tick++;
    if(gamemode==GM_GAME && !paused) {
        /* aftervsync captures positions together with committed sprite/scroll
         * buffers, avoiding a one-frame mismatch while the engine advances. */
        if(!committed.valid || committed.stage!=g_stage.id) cs_present_commit();
        CsViewTarget target=committed.target;
        int scene=target.cutscene;
        target.snap=last_stage!=committed.stage;
#ifndef ESP_PLATFORM
        static unsigned trace_frame;
        if(getenv("CS_TRACE_VIEW") && ++trace_frame%250==0) {
            Entity *focus=scene?cs_scene_focus(&player,entityList,scene_camera_explicit?camera.target:NULL,
                scene_actor_event,showingFace,window_is_open()):&player;
            printf("View stage=%u scene=%d playerHidden=%u focusType=%u focus=(%ld,%ld) actorEvent=%u face=%u\n",
                g_stage.id,scene,player.hidden,focus->type,(long)(focus->x/512),(long)(focus->y/512),scene_actor_event,window_is_open()?showingFace:0);
            for(Entity *e=entityList;e;e=e->next) printf("  actor %u event %u hidden %u at %ld,%ld\n",
                e->type,e->event,e->hidden,(long)(e->x/512),(long)(e->y/512));
        }
#endif
        cs_viewport_update(&view,&target);last_stage=g_stage.id;
        int left=pixel_floor(view.x)-committed.origin_x;
        int top=pixel_floor(view.y)-committed.origin_y;
        if(g_stage.back_type==0) {
            /* The original 320px camera clamps much earlier than our 80px
             * view. Sample the repeating background from the native camera,
             * at its original quarter-speed ratio, independently of that clamp. */
            int bgx=pixel_floor((view.x+40*512)/4)-40;
            int bgy=pixel_floor((view.y+32*512)/4)-32;
            cs_video_render_world_parallax(pixels,left,top,bgx,bgy);
        } else cs_video_render_world(pixels,left,top);
        if(!scene && !window_is_open()) game_hud(pixels);
    } else {
        view.initialized=0;
        /* Remaining screens are cropped at native pixel size while their
         * layouts are replaced. Never shrink the game's graphics or text. */
        cs_video_render(pixels,80,64,120,80,80,64);
    }
    if(gamemode==GM_CREDITS) {
        /* Reflow the original credit column above its scene, retaining every
         * name. The small scene viewport keeps actors at native pixel size. */
        int x=204,y=100;
        Entity *actor=NULL;
        for(Entity *e=entityList;e;e=e->next) {
            if(e->hidden) continue;
            if(!actor || e->event==400 || e->x_speed || e->y_speed) actor=e;
            if(e->event==400) break;
        }
        if(actor) {x=actor->x/512-(int16_t)camera.x_shifted-40;y=actor->y/512-(int16_t)camera.y_shifted-12;}
        if(x<160) x=160;
        if(x>240) x=240;
        if(y<0) y=0;
        if(y>216) y=216;
        if(credit_ended) {
            unsigned pan=(ui_tick/8)%160;
            x=160+(pan<=80?pan:160-pan);y=184;
        }
        cs_video_credit_scene(pixels+40*80,x,y);
        cs_ui_rect(pixels,0,0,80,40,cs_video_color(0));
        for(unsigned row=0;row<credit_text.count;row++) compact(pixels,2,2+row*6,credit_text.line[row]);
        if(credit_ended) {
            cs_ui_rect(pixels,0,0,80,40,cs_video_color(0));
            compact(pixels,20,5,"Cave Story");compact(pixels,18,17,"~ The End ~");
            compact(pixels,20,29,"Thank you!");
        }
        if(credit_icon_visible && !credit_ended) cs_video_ui_icon(pixels,1,49,credit_icon_attr,3,3,12,12);
        if(credit_image_visible) cs_video_credit_image(pixels,64,40);
        return;
    }
    if(gamemode==GM_INTRO) {
        int x=120,y=100;
        for(Entity *e=entityList;e;e=e->next) if(!e->hidden && e->type==OBJ_INTRO_DOCTOR) {
            x=e->x/512-(int16_t)camera.x_shifted-40;
            y=e->y/512-(int16_t)camera.y_shifted-32;
            break;
        }
        /* The intro has no gameplay HUD to suppress. Keep its window mask
         * and every wipe sprite: filtering HUD tile slots drops two thirds of
         * the fade because those slots are reused for transition artwork. */
        cs_video_render(pixels,80,64,x,y,80,64);
        return;
    }
    if(gamemode==GM_TITLE && title_menu_active) {
        cs_ui_rect(pixels,0,0,80,64,cs_video_color(0));
        title_logo(pixels);
        const char *labels[]={"Start Game","Sound Test","Config"};
        unsigned selected=title_selection==0?0:title_selection-1;
        for(unsigned i=0;i<3;i++) {
            if(i==selected) quote_cursor(pixels,11,28+i*11);
            compact(pixels,24,29+i*11,labels[i]);
        }
    }
    if(gamemode==GM_SAVESEL) {
        static const char *modes[]={"Load Save Data","Copy Save Data","Paste Save Data","Delete Save Data","Confirm Delete?"};
        cs_ui_rect(pixels,0,0,80,64,cs_video_color(0));
        compact(pixels,10,2,modes[save_mode<5?save_mode:0]);
        int next_y=12;
        for(unsigned i=0;i<6;i++) {
            int y=i<4?next_y:56;
            int x=i<4?18:i==4?10:49;
            if(i==save_selection) {
                if(i<4) quote_cursor(pixels,6,y-1);
                else compact(pixels,x-6,y,">");
            }
            if(i<4) {
                if(save_used[i]) {
                    char caption[16];
                    snprintf(caption,sizeof(caption),"%.14s",(const char*)STAGE_NAMES+32*save_view[i].stage_id);
                    compact(pixels,x,y,caption);
                    if(i==save_selection) {
                        snprintf(caption,sizeof(caption),"%u/%u %02u:%02u",save_view[i].health,
                            save_view[i].max_health,save_view[i].hour,save_view[i].minute);
                        compact(pixels,x,y+7,caption);next_y+=7;
                    }
                } else compact(pixels,x,y,"New Game");
                next_y+=8;
            } else compact(pixels,x,y,i==4?"Copy":"Delete");
        }
    }
    if(gamemode==GM_CONFIG) settings(pixels);
    if(gamemode==GM_SOUNDTEST) sound_menu(pixels);
    if(gamemode==GM_GAME && paused) inventory(pixels);
    if(map_view_active) {map_menu(pixels);return;}
    if(gamemode==GM_GAME && window_is_open()) {
        if(paused) {
            description(pixels);
            return;
        }
        CsDialog dialog;cs_dialog_layout(&dialog,windowText);
        cs_ui_rect(pixels,0,43,80,21,cs_video_color(8));
        cs_ui_rect(pixels,1,44,78,19,cs_video_color(2));
        for(unsigned row=0;row<3 && windowDialogPage*3+row<dialog.count;row++)
            compact(pixels,2,44+row*6,dialog.line[windowDialogPage*3+row]);
        if(showingFace && showingFace<FACE_COUNT) {
            panel(pixels,0,20,23,23);
            /* Portrait belongs to the interface, independently of the 1:1 actor. */
            cs_video_ui_portrait(pixels,3,23,TILE_ATTR(face_info[showingFace].palette,1,0,0,TILE_FACEINDEX));
        }
        if(showingItem) {
            unsigned w=((handSpr.size>>2)&3)+1,h=(handSpr.size&3)+1;
            panel(pixels,27,22,28,21);
            cs_video_ui_icon(pixels,29,24,handSpr.attr,w,h,w*8,h*8);
        }
        if(teleMenuActive && teleMenuSlotCount) {
            cs_ui_rect(pixels,0,0,80,23,cs_video_color(1));
            compact(pixels,24,1,"Teleport");
            unsigned first=teleMenuSelection>1?teleMenuSelection-1:0;
            if(teleMenuSlotCount>3 && first>teleMenuSlotCount-3) first=teleMenuSlotCount-3;
            for(unsigned i=0;i<3 && first+i<teleMenuSlotCount;i++) {
                int x=5+i*24;
                cs_video_ui_icon(pixels,x,9,teleMenuSprite[first+i].attr,4,2,20,10);
                if(first+i==teleMenuSelection) selection(pixels,x-1,8,22,13);
            }
        }
        if(tscState==TSC_PROMPT && !window_dialog_more()) {
            panel(pixels,28,28,52,15);
            compact(pixels,32,32,promptAnswer?">Yes  No":" Yes >No");
        }
    }
}

void cs_present_80(uint16_t *pixels) {
    present(pixels);
    if(!cs_sram_ok()) {
        panel(pixels,0,0,80,12);
        compact(pixels,6,3,"SAVE UNAVAILABLE");
    }
}
