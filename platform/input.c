#include "input.h"
#include "md/joy.h"
uint8_t joytype=JOY_TYPE_PAD6;
uint16_t joystate,joystate_old;
static enum cs_input_context context=CS_INPUT_GAME;
void cs_input_set_context(enum cs_input_context value) {context=value;}
void joy_init(void) {
    joystate=joystate_old=0;
    context=CS_INPUT_GAME;
    joytype=JOY_TYPE_PAD6;
}
void cs_input_apply(uint32_t keys) {
    uint16_t now=0;
    if(keys&CS_KEY_A) now|=JOY_C;
    if(keys&CS_KEY_B) now|=JOY_B;
    if(keys&CS_KEY_START) now|=JOY_START;
    if(keys&CS_KEY_SELECT) {
        if(context==CS_INPUT_GAME) now|=JOY_Z;
        else if(context==CS_INPUT_CUTSCENE) now|=JOY_A;
        else if(context==CS_INPUT_INVENTORY) now|=JOY_X;
    }
    if(keys&CS_KEY_UP) now|=JOY_UP;
    if(keys&CS_KEY_DOWN) now|=JOY_DOWN;
    if(keys&CS_KEY_LEFT) now|=JOY_LEFT;
    if(keys&CS_KEY_RIGHT) now|=JOY_RIGHT;
    joystate_old=joystate;joystate=now;
}
void joy_update(void) {cs_input_apply(cs_input_poll());}
