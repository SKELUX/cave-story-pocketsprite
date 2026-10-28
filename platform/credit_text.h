#ifndef CS_CREDIT_TEXT_H
#define CS_CREDIT_TEXT_H
#include <stdint.h>
/* Six readable 19-column rows, preserving whole source lines via word wrapping. */
typedef struct {char line[6][20];unsigned count;} CsCreditText;
void cs_credit_text_clear(CsCreditText *view);
void cs_credit_text_add(CsCreditText *view,const uint8_t *text,unsigned length);
#endif
