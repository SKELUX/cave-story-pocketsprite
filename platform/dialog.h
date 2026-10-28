#ifndef CS_DIALOG_H
#define CS_DIALOG_H
#include <stdint.h>
typedef struct { char line[6][20]; unsigned count; } CsDialog;
extern uint8_t cs_dialog_enabled;
void cs_dialog_layout(CsDialog *out,const uint8_t text[3][36]);
#endif
