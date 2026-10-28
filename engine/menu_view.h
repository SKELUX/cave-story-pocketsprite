#ifndef CS_MENU_VIEW_H
#define CS_MENU_VIEW_H
#include "md/types.h"
#include "system.h"
#include "credit_text.h"
typedef struct {
    uint8_t page,cursor,count,capturing,confirming;
    const char *caption[12];
    uint8_t kind[12],value[12];
} ConfigView;
void config_get_view(ConfigView *view);
extern uint8_t sound_view_track,sound_view_status;
extern uint8_t map_view_active;
extern SaveEntry save_view[4];
extern CsCreditText credit_text;
extern uint8_t credit_ended,credit_image_visible;
extern uint16_t credit_icon_attr;
extern uint8_t credit_icon_visible;
#endif
