/* Native software tile/sprite renderer. API and fade/text logic adapted from
 * Cave Story MD (MIT, engine/LICENSE.md). No CPU emulation or MMIO. */
#include "md/dma.h"
#include "common.h"
#include "md/sys.h"
#include "md/vdp.h"
#include "md/comp.h"
#include "video.h"
#include "md/stdlib.h"
#include "res/system.h"

const uint16_t PAL_FadeOut[64] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xEEE,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
const uint16_t PAL_FadeOutBlue[64] = {
        0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0xEEE,
        0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
        0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
        0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200
};
const uint16_t PAL_FullWhite[64] = {
        0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE,
        0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE,
        0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE,
        0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE, 0xEEE
};

static uint8_t graphics[65536];
static int16_t vertical[40];
static uint8_t display_enabled, scroll_h, scroll_v, window_x, window_y, backcolor;
static Sprite presented[80];
static uint16_t presented_count;
static uint16_t read_word(unsigned address) {
    uint16_t word;
    memcpy(&word, graphics + (address & 65534), 2);
    return word;
}
static void write_word(unsigned address, uint16_t word) {
    memcpy(graphics + (address & 65534), &word, 2);
}
static void copy_graphics(unsigned address, const void *src, size_t count) {
    const uint8_t *bytes = src;
    while(count) {
        unsigned at = address & 65535;
        size_t part = count < 65536-at ? count : 65536-at;
        memcpy(graphics+at, bytes, part);
        bytes += part; address += part; count -= part;
    }
}

// Palette vars
static uint16_t pal_current[64];
static uint16_t pal_next[64];
static uint8_t pal_fading;
static uint8_t pal_fadespeed;
static uint8_t pal_fadecnt;

// Sprite vars
static uint16_t sprite_count;
static Sprite sprite_table[80];
static uint16_t sprite_ymax;

// Font vars
static uint16_t font_pal;

// Screen size
uint16_t ScreenHeight;
uint16_t ScreenHalfH;
uint8_t pal_mode;
uint8_t FPS;

void vdp_init(void) {
    memset(graphics, 0, sizeof(graphics));
    memset(vertical, 0, sizeof(vertical));
    memset(pal_current, 0, sizeof(pal_current));
    memset(pal_next, 0, sizeof(pal_next));
    pal_mode = 1; FPS = 50;
    ScreenHeight = 240; ScreenHalfH = 120; sprite_ymax = 272;
    display_enabled = 1; scroll_h = scroll_v = window_x = window_y = backcolor = 0;
    pal_fading = pal_fadecnt = 0;
    sprite_count = presented_count = 0;
    dma_clear();
    vdp_font_load(UFTC_SysFont);
    vdp_colors(0, PAL_FadeOut, 64);
}

//void vdp_vsync(void) {
//    vblank = 0;
//    while (!vblank);
//    vblank = 0;
//}

// Register stuff

void vdp_set_display(uint8_t enabled) {
    display_enabled = !!enabled;
}

void vdp_set_autoinc(uint8_t val) {
    (void)val; /* All native operations specify their own stride. */
}

void vdp_set_scrollmode(uint8_t hoz, uint8_t vert) {
    scroll_h = hoz; scroll_v = vert;
}

void vdp_set_highlight(uint8_t enabled) {
    (void)enabled; /* Unused by this game. */
}

void vdp_set_backcolor(uint8_t index) {
    backcolor = index & 63;
}

void vdp_set_window(uint8_t x, uint8_t y) {
    window_x = x; window_y = y;
}

// Tile patterns

void vdp_tiles_load(const uint32_t *data, uint16_t index, uint16_t num) {
    copy_graphics((unsigned)index * 32, data, (size_t)num * 32);
}

void vdp_tiles_load_uftc(const void *uftc_data, uint16_t index, uint16_t uftc_index, uint16_t num) {
    uint32_t buf[16*8];
    static const uint16_t maxlen = 16;

    while(num) {
        uint16_t now = (num < maxlen ? num : maxlen);
        uftc_unpack(uftc_data, buf, uftc_index, now);
        __asm__("": : :"memory");
        dma_now(DmaVRAM, (uintptr_t) buf, index << 5, now << 4, 2);
        num -= now;
        index += now;
        uftc_index += now;
    }
}

// Tile maps

void vdp_map_xy(uint16_t plan, uint16_t tile, uint16_t x, uint16_t y) {
    write_word(plan + (((y & 31) * 64 + (x & 63)) * 2), tile);
}

void vdp_map_hline(uint16_t plan, const uint16_t *tiles, uint16_t x, uint16_t y, uint16_t len) {
    dma_now(DmaVRAM, (uintptr_t) tiles, plan + ((x + (y << PLAN_WIDTH_SFT)) << 1), len, 2);
}

void vdp_map_vline(uint16_t plan, const uint16_t *tiles, uint16_t x, uint16_t y, uint16_t len) {
    dma_now(DmaVRAM, (uintptr_t) tiles, plan + ((x + (y << PLAN_WIDTH_SFT)) << 1), len, 128);
}

void vdp_map_fill_rect(uint16_t plan, uint16_t index, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t inc) {
    uint16_t tiles[64];
    for (uint16_t yy = 0; yy < h; yy++) {
        for (uint16_t xx = 0; xx < w; xx++) {
            tiles[xx] = index;
            index += inc;
        }
        __asm__("": : :"memory");
        dma_now(DmaVRAM, (uintptr_t) tiles, plan + ((x + ((y + yy) << PLAN_WIDTH_SFT)) << 1), w, 2);
    }
}

void vdp_map_clear(uint16_t plan) {
    uint16_t addr = plan;
    while (addr < plan + 0x1000) {
        __asm__("": : :"memory");
        dma_now(DmaVRAM, (uintptr_t) BlankData, addr, 0x80, 2);
        addr += 0x100;
    }
}

// Palettes

void vdp_colors(uint16_t index, const uint16_t *values, uint16_t count) {
    dma_now(DmaCRAM, (uintptr_t) values, index << 1, count, 2);
    for (uint16_t i = count; i--;) pal_current[index + i] = values[i];
}

void vdp_color(uint16_t index, uint16_t color) {
    if(index < 64) pal_current[index] = color;
}

void vdp_colors_next(uint16_t index, const uint16_t *values, uint16_t count) {
    for (uint16_t i = count; i--;) pal_next[index + i] = values[i];
}

void vdp_color_next(uint16_t index, uint16_t color) {
    pal_next[index] = color;
}

void vdp_colors_apply_next_now(void) {
    vdp_colors(0, pal_next, 64);
    //memcpy(pal_current, pal_next, sizeof(uint16_t) * 64);
}

uint16_t vdp_fade_step_calc(void) {
    if (pal_fading != FADE_INPROGRESS) return 0;
    if (++pal_fadecnt >= pal_fadespeed) {
        pal_fadecnt = 0;
        uint16_t colors_changed = 0;
        for (uint16_t i = 64; i--;) {
            uint16_t cR = pal_current[i] & 0x00E;
            uint16_t nR = pal_next[i] & 0x00E;
            uint16_t cG = pal_current[i] & 0x0E0;
            uint16_t nG = pal_next[i] & 0x0E0;
            uint16_t cB = pal_current[i] & 0xE00;
            uint16_t nB = pal_next[i] & 0xE00;
            if (cR != nR) {
                pal_current[i] += cR < nR ? 0x002 : -0x002;
                colors_changed++;
            }
            if (cG != nG) {
                pal_current[i] += cG < nG ? 0x020 : -0x020;
                colors_changed++;
            }
            if (cB != nB) {
                pal_current[i] += cB < nB ? 0x200 : -0x200;
                colors_changed++;
            }
        }
        if (!colors_changed) {
            pal_fading = FADE_LASTFRAME;
            return 2;
        }
    }
    return 1;
}

void vdp_fade_step_dma(void) {
    if (pal_fading != FADE_NONE) {
        dma_queue(DmaCRAM, (uintptr_t) pal_current, 0, 64, 2);
        if (pal_fading == FADE_LASTFRAME) pal_fading = FADE_NONE;
    }
}

void vdp_fade(const uint16_t *src, const uint16_t *dst, uint16_t speed, uint8_t async) {
    if (src) vdp_colors(0, src, 64);
    if (dst) vdp_colors_next(0, dst, 64);
    pal_fading = FADE_INPROGRESS;
    pal_fadespeed = speed;
    pal_fadecnt = 0;
    if (!async) {
        while (vdp_fade_step_calc()) {
            sys_wait_vblank();
            vdp_fade_step_dma();
            dma_flush();
        }
    }
}

// Scroll

void vdp_hscroll(uint16_t plan, int16_t hscroll) {
    write_word(VDP_HSCROLL_TABLE + (plan == VDP_PLANE_A ? 0 : 2), hscroll);
}

void vdp_hscroll_tile(uint16_t plan, int16_t *hscroll) {
    dma_now(DmaVRAM, (uintptr_t) hscroll, VDP_HSCROLL_TABLE + (plan == VDP_PLANE_A ? 0 : 2), 30, 32);
}

void vdp_vscroll(uint16_t plan, int16_t vscroll) {
    vertical[plan == VDP_PLANE_A ? 0 : 1] = vscroll;
}

// Sprites

void vdp_sprite_add(const Sprite *spr) {
    // Exceeded max number of sprites
    if (sprite_count >= 80) return;
    // Prevent drawing off-screen sprites
    if ((unsigned) (spr->x - 96) < 352 && (unsigned) (spr->y - 96) < sprite_ymax) {
        sprite_table[sprite_count] = *spr;
        sprite_table[sprite_count].link = sprite_count + 1;
        sprite_count++;
    }
}

void vdp_sprites_add(const Sprite *spr, uint16_t num) {
    for (uint16_t i = num; i--;) vdp_sprite_add(&spr[i]);
}

void vdp_sprites_add_force(const Sprite *spr, uint16_t num) {
    for (uint16_t i = num; i--;) {
        if (sprite_count >= 80) return;
        sprite_table[sprite_count] = spr[i];
        sprite_table[sprite_count].link = sprite_count + 1;
        sprite_count++;
    }
}

void vdp_sprites_clear(void) {
    static const Sprite NULL_SPRITE = {.x = 0x80, .y = 0x80};
    sprite_count = 0;
    vdp_sprites_add(&NULL_SPRITE, 1);
}

void vdp_sprites_update(void) {
    if (!sprite_count) return;
    memcpy(presented, sprite_table, sprite_count * sizeof(Sprite));
    presented_count = sprite_count;
    sprite_count = 0;
}

// Font / Text

void vdp_font_load(const uint32_t *tiles) {
    font_pal = 0;
    // ASCII 32-127
    vdp_tiles_load_uftc(tiles, TILE_FONTINDEX, 0, 0x60);
    // Extended charset
    uint16_t index = (VDP_PLANE_W >> 5) + 3;
    for (uint16_t i = 0; i < 30; i++) {
        vdp_tiles_load_uftc(tiles, index, 0x60 + i, 1);
        index += 4;
    }
}

void vdp_font_pal(uint16_t pal) {
    font_pal = pal;
}

void vdp_nputs(uint16_t plan, const char *str, uint16_t x, uint16_t y, uint16_t maxlen) {
    uint16_t written = 0;
    while (written < maxlen && *str) {
        uint16_t c = (uint8_t)*str++;
        if (c == 1) {
            if (!*str) break;
            c = (VDP_PLANE_W >> 5) - 1 + ((uint8_t)*str++ << 2);
        } else c = TILE_FONTINDEX + c - 0x20;
        vdp_map_xy(plan, TILE_ATTR(font_pal,1,0,0,c), (x+written)&63, y);
        ++written;
    }
}

void vdp_text_clear_ex(uint16_t plan, uint16_t x, uint16_t y, uint16_t len, uint16_t val) {
    for (uint16_t i=0; i<len; ++i) vdp_map_xy(plan,val,x+i,y);
}

void vdp_text_clear(uint16_t plan, uint16_t x, uint16_t y, uint16_t len) {
    vdp_text_clear_ex(plan, x, y, len, 0);
}

/* Deferred transfers retain the engine's submit/pop/flush ordering. Native
 * memory copies replace the console DMA hardware; no bus/register execution. */
typedef struct {
    uint32_t kind;
    uintptr_t source;
    uint16_t target, words, stride;
} Transfer;
static Transfer transfers[64];
static unsigned transfer_count;

static void transfer_apply(const Transfer *t) {
    const uint8_t *source = (const uint8_t *)t->source;
    if (t->kind == DmaVRAM && t->stride == 2) {
        copy_graphics(t->target, source, (size_t)t->words * 2);
        return;
    }
    for (unsigned i=0; i<t->words; ++i) {
        uint16_t value;
        memcpy(&value, source + i*2, 2);
        unsigned at = (t->target + i*t->stride) & 65535;
        if (t->kind == DmaVRAM) write_word(at, value);
        else if (t->kind == DmaCRAM && at < 128) pal_current[at/2] = value;
        else if (t->kind == DmaVSRAM && at < sizeof(vertical)) vertical[at/2] = value;
    }
}
void dma_queue(uint32_t kind, uintptr_t from, uint16_t to, uint16_t words, uint16_t stride) {
    if (transfer_count == 64) dma_flush();
    transfers[transfer_count++] = (Transfer){kind, from, to, words, stride};
}
void dma_queue_rom(uint32_t kind, uintptr_t from, uint16_t to, uint16_t words, uint16_t stride) {
    dma_queue(kind, from, to, words, stride);
}
void dma_pop(void) {
    if (transfer_count) transfer_apply(&transfers[--transfer_count]);
}
void dma_flush(void) {
    for (unsigned i=0; i<transfer_count; ++i) transfer_apply(&transfers[i]);
    transfer_count = 0;
}
void dma_clear(void) { transfer_count = 0; }
uint16_t vdp_get_palmode(void) { return pal_mode; }
uint16_t vdp_get_vblank(void) { return 1; }

static unsigned tile_pixel(unsigned index, unsigned x, unsigned y) {
    unsigned address = (index & 2047)*32 + y*4 + x/2;
    return (graphics[address] >> ((x&1) ? 0 : 4)) & 15;
}

uint16_t cs_video_color(unsigned index) {
    unsigned c=pal_current[index&63];
    return ((((c>>1)&7)*31/7)<<11)|((((c>>5)&7)*63/7)<<5)|(((c>>9)&7)*31/7);
}
static void ui_art(uint16_t *out,int x,int y,uint16_t attr,
                      unsigned tw,unsigned th,unsigned width,unsigned height,int row_major) {
    if(!width || !height) return;
    for(unsigned yy=0;yy<height;yy++) for(unsigned xx=0;xx<width;xx++) {
        int dx=x+(int)xx,dy=y+(int)yy;
        if(dx<0 || dx>=80 || dy<0 || dy>=64) continue;
        unsigned sx=xx*tw*8/width,sy=yy*th*8/height;
        if(attr&0x800) sx=tw*8-1-sx;
        if(attr&0x1000) sy=th*8-1-sy;
        unsigned tile=(attr&2047)+(row_major?(sy/8)*tw+sx/8:(sx/8)*th+sy/8);
        unsigned color=tile_pixel(tile,sx&7,sy&7);
        if(color) out[dy*80+dx]=cs_video_color(color+((attr>>9)&48));
    }
}

void cs_video_ui_icon(uint16_t *out,int x,int y,uint16_t attr,
                      unsigned tw,unsigned th,unsigned width,unsigned height) {
    ui_art(out,x,y,attr,tw,th,width,height,0);
}
void cs_video_ui_portrait(uint16_t *out,int x,int y,uint16_t attr) {
    ui_art(out,x,y,attr,6,6,16,16,1);
}

/* Palette index plus bit 6 for layer priority; transparent pixel is zero. */
static int world_only;
static int background_only;
static int credit_scene;
static int native_parallax,bg_origin_x,bg_origin_y,world_left,world_top;
static unsigned plane_pixel(unsigned plane, int x, int y) {
    unsigned tx, ty, base;
    if(credit_scene && plane) return 0;
    if(native_parallax && plane) {
        tx=(unsigned)(x-world_left+bg_origin_x)&511;
        ty=(unsigned)(y-world_top+bg_origin_y)&255;base=VDP_PLANE_B;
    } else if (!world_only && !plane && x >= 0 && y >= 0 && (((window_x & 128) ? x >= (window_x&31)*16 : x < (window_x&31)*16) ||
                   ((window_y & 128) ? y >= (window_y&31)*8 : y < (window_y&31)*8))) {
        tx = (unsigned)x & 511; ty = (unsigned)y & 255; base = VDP_PLANE_W;
    } else {
        unsigned row = scroll_h == HSCROLL_TILE ? ((unsigned)y & 248) :
                       scroll_h == HSCROLL_LINE ? (unsigned)y : 0;
        int hs = (int16_t)read_word(VDP_HSCROLL_TABLE + row*4 + plane*2);
        unsigned column = scroll_v == VSCROLL_2TILE ? ((unsigned)x / 16)*2 : 0;
        int vs = vertical[(column+plane)%40];
        tx = (unsigned)(x-hs)&511; ty = (unsigned)(y+vs)&255;
        base = plane ? VDP_PLANE_B : VDP_PLANE_A;
    }
    unsigned attr = read_word(base + ((ty/8)*64+tx/8)*2);
    unsigned px = tx&7, py = ty&7;
    if (attr & 0x800) px ^= 7;
    if (attr & 0x1000) py ^= 7;
    unsigned pixel = tile_pixel(attr, px, py);
    return pixel ? pixel | ((attr>>9)&48) | ((attr>>9)&64) : 0;
}

/* Credit illustrations are UI artwork: preserve their 160:240 aspect ratio
 * as a 16x24 thumbnail, independently of the native actor viewport. */
void cs_video_credit_image(uint16_t *out,int x,int y) {
    for(unsigned yy=0;yy<24;yy++) for(unsigned xx=0;xx<16;xx++) {
        int dx=x+xx,dy=y+yy;
        if(dx>=0 && dx<80 && dy>=0 && dy<64)
            out[dy*80+dx]=cs_video_color(plane_pixel(0,xx*10,yy*10)&63);
    }
}

static unsigned sprite_pixel(int x, int y, const unsigned *active, unsigned count) {
    for (unsigned n=0; n<count; ++n) {
        const Sprite *s = &presented[active[n]];
        unsigned index=s->attr&2047;
        if(credit_scene && index>=TILE_SHEETINDEX+372 && index<TILE_SHEETINDEX+372+16*9) continue;
        if(world_only && ((index>=TILE_HUDINDEX && index<TILE_HUDINDEX+TILE_HUDSIZE) ||
                         (index>=TILE_EXWEPINDEX && index<TILE_EXWEPINDEX+16) ||
                         (index>=TILE_AIRINDEX && index<TILE_AIRINDEX+TILE_AIRSIZE) ||
                         (index>=TILE_NAMEINDEX && index<TILE_NAMEINDEX+TILE_NAMESIZE))) continue;
        unsigned w = ((s->size>>2)&3)+1, h = (s->size&3)+1;
        int sx = x - (s->x-128), sy = y - (s->y-128);
        if ((unsigned)sx >= w*8 || (unsigned)sy >= h*8) continue;
        if (s->attr & 0x800) sx = w*8-1-sx;
        if (s->attr & 0x1000) sy = h*8-1-sy;
        unsigned tile = (s->attr&2047) + ((unsigned)sx/8)*h+(unsigned)sy/8;
        unsigned pixel = tile_pixel(tile, sx&7, sy&7);
        if (pixel) return pixel | ((s->attr>>9)&48) | ((s->attr>>9)&64);
    }
    return 0;
}

void cs_video_credit_scene(uint16_t *out,int x,int y) {
    credit_scene=world_only=1;
    cs_video_render(out,80,24,x,y,80,24);
    credit_scene=world_only=0;
}

void cs_video_render_world(uint16_t *out,int x,int y) {
    world_only=1;
    cs_video_render(out,80,64,x,y,80,64);
    world_only=0;
}
void cs_video_render_world_parallax(uint16_t *out,int x,int y,int bgx,int bgy) {
    native_parallax=1;world_left=x;world_top=y;bg_origin_x=bgx;bg_origin_y=bgy;
    cs_video_render_world(out,x,y);
    native_parallax=0;
}
void cs_video_menu_background(uint16_t *out) {
    background_only=1;
    cs_video_render(out,80,64,0,0,320,240);
    background_only=0;
}

/* Native-resolution scanlines: tile attributes/scrolling are resolved once
 * per span, and each sprite contributes its visible pixels once per row. */
static void plane_row(uint8_t *out,unsigned plane,int left,int y,unsigned width) {
    if(credit_scene && plane) {memset(out,0,width);return;}
    unsigned row=scroll_h==HSCROLL_TILE?((unsigned)y&248):
                 scroll_h==HSCROLL_LINE?(unsigned)y:0;
    int hs=(int16_t)read_word(VDP_HSCROLL_TABLE+row*4+plane*2);
    for(unsigned xx=0;xx<width;) {
        int x=left+(int)xx;
        unsigned tx,ty,base;
        int window=!world_only && !plane && x>=0 && y>=0 &&
            (((window_x&128)?x>=(window_x&31)*16:x<(window_x&31)*16) ||
             ((window_y&128)?y>=(window_y&31)*8:y<(window_y&31)*8));
        if(native_parallax && plane) {
            tx=(unsigned)(x-world_left+bg_origin_x)&511;
            ty=(unsigned)(y-world_top+bg_origin_y)&255;base=VDP_PLANE_B;
        } else if(window) {tx=(unsigned)x&511;ty=(unsigned)y&255;base=VDP_PLANE_W;}
        else {
            unsigned column=scroll_v==VSCROLL_2TILE?((unsigned)x/16)*2:0;
            tx=(unsigned)(x-hs)&511;
            ty=(unsigned)(y+vertical[(column+plane)%40])&255;
            base=plane?VDP_PLANE_B:VDP_PLANE_A;
        }
        unsigned attr=read_word(base+((ty/8)*64+tx/8)*2);
        unsigned span=8-(tx&7),screen_span=8-((unsigned)x&7);
        /* Screen boundaries also bound window transitions and column scroll. */
        if(span>screen_span) span=screen_span;
        if(span>width-xx) span=width-xx;
        unsigned py=(ty&7)^((attr&0x1000)?7:0),prefix=(attr>>9)&112;
        for(unsigned i=0;i<span;i++) {
            unsigned px=((tx+i)&7)^((attr&0x800)?7:0);
            unsigned color=tile_pixel(attr,px,py);
            out[xx+i]=color?(color|prefix):0;
        }
        xx+=span;
    }
}
/* Filter the committed sprite list once per viewport, retaining draw order.
 * The old loop repeated these checks for every one of the 64 output rows. */
static unsigned visible_sprites(uint8_t *visible,int left,int top,unsigned width,unsigned height) {
    unsigned count=0;
    for(unsigned n=0;n<presented_count;n++) {
        const Sprite *s=&presented[n];
        unsigned index=s->attr&2047;
        if(credit_scene && index>=TILE_SHEETINDEX+372 && index<TILE_SHEETINDEX+372+16*9) continue;
        if(world_only && ((index>=TILE_HUDINDEX && index<TILE_HUDINDEX+TILE_HUDSIZE) ||
                         (index>=TILE_EXWEPINDEX && index<TILE_EXWEPINDEX+16) ||
                         (index>=TILE_AIRINDEX && index<TILE_AIRINDEX+TILE_AIRSIZE) ||
                         (index>=TILE_NAMEINDEX && index<TILE_NAMEINDEX+TILE_NAMESIZE))) continue;
        int x=s->x-128,y=s->y-128;
        unsigned w=(((s->size>>2)&3)+1)*8,h=((s->size&3)+1)*8;
        if(x>=left+(int)width || x+(int)w<=left ||
           y>=top+(int)height || y+(int)h<=top) continue;
        visible[count++]=n;
    }
    return count;
}
static void sprite_row(uint8_t *out,int left,int y,unsigned width,
                       const uint8_t *visible,unsigned count) {
    memset(out,0,width);
    for(unsigned n=0;n<count;n++) {
        const Sprite *s=&presented[visible[n]];
        unsigned index=s->attr&2047;
        unsigned h=(s->size&3)+1,w=((s->size>>2)&3)+1;
        int sy=y-(s->y-128),origin=s->x-128-left;
        if((unsigned)sy>=h*8 || origin>=(int)width || origin+(int)w*8<=0) continue;
        if(s->attr&0x1000) sy=h*8-1-sy;
        int begin=origin<0?0:origin,end=origin+w*8;
        if(end>(int)width) end=width;
        unsigned prefix=(s->attr>>9)&112;
        for(int dx=begin;dx<end;dx++) {
            if(out[dx]) continue; /* Earlier sprites win even across priorities. */
            unsigned sx=dx-origin;
            if(s->attr&0x800) sx=w*8-1-sx;
            unsigned tile=index+(sx/8)*h+(unsigned)sy/8;
            unsigned color=tile_pixel(tile,sx&7,sy&7);
            if(color) out[dx]=color|prefix;
        }
    }
}
#ifdef CS_VIDEO_REFERENCE
static int reference_render;
#endif
static void render_native(uint16_t *out,unsigned width,unsigned height,
                          int left,int top,const uint16_t *rgb) {
    uint8_t a[320],b[320],s[320],visible[80];
    unsigned count=background_only?0:visible_sprites(visible,left,top,width,height);
    for(unsigned yy=0;yy<height;yy++) {
        int y=top+(int)yy;
        plane_row(b,1,left,y,width);
        if(!background_only) {plane_row(a,0,left,y,width);sprite_row(s,left,y,width,visible,count);}
        for(unsigned xx=0;xx<width;xx++) {
            unsigned color=backcolor;
            if(b[xx] && !(b[xx]&64)) color=b[xx];
            if(!background_only) {
                if(a[xx] && !(a[xx]&64)) color=a[xx];
                if(s[xx] && !(s[xx]&64)) color=s[xx];
            }
            if(b[xx]&64) color=b[xx];
            if(!background_only) {
                if(a[xx]&64) color=a[xx];
                if(s[xx]&64) color=s[xx];
            }
            *out++=rgb[color&63];
        }
    }
}

void cs_video_render(uint16_t *out, unsigned width, unsigned height,
                     int left, int top, unsigned view_width, unsigned view_height) {
    if (!width || !height) return;
    uint16_t rgb[64];
    for (unsigned i=0; i<64; ++i) {
        unsigned c = pal_current[i];
        unsigned r = ((c>>1)&7)*31/7, g = ((c>>5)&7)*63/7, b = ((c>>9)&7)*31/7;
        rgb[i] = (r<<11)|(g<<5)|b;
    }
    if(!display_enabled) {memset(out,0,width*height*sizeof(*out));return;}
    if(width<=320 && width==view_width && height==view_height
#ifdef CS_VIDEO_REFERENCE
       && !reference_render
#endif
    ) {render_native(out,width,height,left,top,rgb);return;}
    for (unsigned yy=0; yy<height; ++yy) {
        int y = top + (int)(yy*view_height/height);
        unsigned active[80], count=0;
        for (unsigned i=0; i<presented_count; ++i) {
            int sy = presented[i].y-128;
            if (y>=sy && y<sy+((presented[i].size&3)+1)*8) active[count++]=i;
        }
        for (unsigned xx=0; xx<width; ++xx) {
            if (!display_enabled) { *out++ = 0; continue; }
            int x = left + (int)(xx*view_width/width);
            unsigned a=background_only?0:plane_pixel(0,x,y), b=plane_pixel(1,x,y);
            unsigned s=background_only?0:sprite_pixel(x,y,active,count);
            unsigned color=backcolor;
            if (b && !(b&64)) color=b;
            if (a && !(a&64)) color=a;
            if (s && !(s&64)) color=s;
            if (b&64) color=b;
            if (a&64) color=a;
            if (s&64) color=s;
            *out++ = rgb[color&63];
        }
    }
}

#ifdef CS_VIDEO_REFERENCE
/* Test oracle only: production retains the general path for menu artwork. */
void cs_video_render_reference(uint16_t *out,unsigned width,unsigned height,
                               int left,int top) {
    reference_render=1;
    cs_video_render(out,width,height,left,top,width,height);
    reference_render=0;
}
#endif
