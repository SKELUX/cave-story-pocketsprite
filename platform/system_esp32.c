/* PocketSprite native runtime. Display layout adaptation is still in progress. */
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "8bkc-hal.h"
#include "powerbtn_menu.h"
#include "video.h"
#include "presentation.h"
#include "md/sys.h"

void cave_main(void) __attribute__((noreturn));
const uint32_t BlankData[64]={0};
const char Version[]="Native WIP", Date[]="POCKET   2026.10";
const char Homepage[]="https://github.com/andwn/cave-story-md";
volatile uint8_t vblank;
static uint16_t framebuffer[80*64];
static SemaphoreHandle_t audio_mutex;
static TickType_t last_frame;
static uint32_t rng;

/* These compatibility hooks serialize audio control; they must NOT disable
 * ESP32 interrupts across game logic or flash operations. */
void disable_ints(void) { xSemaphoreTakeRecursive(audio_mutex,portMAX_DELAY); }
void enable_ints(void) { xSemaphoreGiveRecursive(audio_mutex); }
void mem_init(void) {} /* ESP-IDF initializes the native libc heap. */
void cs_srand(uint16_t seed) { rng=seed?seed:1; }
uint16_t cs_rand(void) {
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return (uint16_t)rng;
}
void cs_assert_fail(const char *expr,const char *file,int line) {
    printf("Cave Story assertion %s:%d: %s\n",file,line,expr);
    abort();
}
void sys_hard_reset(void) { esp_restart(); for(;;) vTaskDelay(1); }
void sys_wait_vblank(void) {
    static uint32_t frames,worst_render,worst_logic,worst_display;
    static int64_t batch_start,previous_return;
    int64_t begin=esp_timer_get_time();
    if(!batch_start) batch_start=begin;
    if(previous_return && begin-previous_return>worst_logic) worst_logic=begin-previous_return;
    cs_present_80(framebuffer);
    int64_t rendered=esp_timer_get_time();
    for(unsigned n=0;n<80*64;n++) {
        uint16_t value=framebuffer[n];
        framebuffer[n]=(value>>8)|(value<<8);
    }
    kchal_send_fb(framebuffer);
    int64_t sent=esp_timer_get_time();
    uint32_t elapsed=rendered-begin,display=sent-rendered;
    if(elapsed>worst_render) worst_render=elapsed;
    if(display>worst_display) worst_display=display;
    if(++frames%250==0) {
        unsigned fps10=(unsigned)(2500000000ULL/(sent-batch_start));
        printf("CS frame=%u fps=%u.%u max us logic=%u render=%u display=%u heap=%u stack=%u\n",
            frames,fps10/10,fps10%10,worst_logic,worst_render,worst_display,
            esp_get_free_heap_size(),(unsigned)uxTaskGetStackHighWaterMark(NULL));
        worst_render=worst_logic=worst_display=0;batch_start=sent;
    }
    TickType_t now=xTaskGetTickCount();
    const TickType_t period=pdMS_TO_TICKS(20);
    if(now-last_frame>=period) {
        /* Rendering/game logic already consumed this frame's budget. Waiting
         * another full period here halves throughput under load. Yield once
         * for the idle/watchdog task, then start the next frame immediately. */
        last_frame=now;
        vTaskDelay(1);
    } else {
        vTaskDelayUntil(&last_frame,period);
    }
    previous_return=esp_timer_get_time();
    vblank=1;
}
uint32_t cs_input_poll(void) {
    uint32_t keys=kchal_get_keys();
    if(keys & KC_BTN_POWER) {
        kchal_sound_mute(1);
        int action=powerbtn_menu_show(framebuffer);
        if(action==POWERBTN_MENU_EXIT) kchal_exit_to_chooser();
        if(action==POWERBTN_MENU_POWERDOWN) kchal_power_down();
        kchal_wait_keys_released();
        kchal_sound_mute(0);
        last_frame=xTaskGetTickCount();
        return 0;
    }
    return keys;
}
void app_main(void) {
    kchal_init();
    audio_mutex=xSemaphoreCreateRecursiveMutex();
    if(!audio_mutex) abort();
    last_frame=xTaskGetTickCount();
    printf("Cave Story native start: free heap %u\n",esp_get_free_heap_size());
    cave_main();
}
