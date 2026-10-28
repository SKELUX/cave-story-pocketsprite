#ifndef CS_INPUT_H
#define CS_INPUT_H
#include <stdint.h>
/* PocketSprite HAL button bits. Host adapters use the same values. */
enum {
    CS_KEY_RIGHT=1, CS_KEY_LEFT=2, CS_KEY_UP=4, CS_KEY_DOWN=8,
    CS_KEY_START=16, CS_KEY_SELECT=32, CS_KEY_A=64, CS_KEY_B=128
};
enum cs_input_context {CS_INPUT_GAME,CS_INPUT_CUTSCENE,CS_INPUT_INVENTORY,CS_INPUT_MENU};
void cs_input_set_context(enum cs_input_context context);
uint32_t cs_input_poll(void);
void cs_input_apply(uint32_t keys);
#endif
