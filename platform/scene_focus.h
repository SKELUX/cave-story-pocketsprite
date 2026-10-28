#ifndef CS_SCENE_FOCUS_H
#define CS_SCENE_FOCUS_H
#include "common.h"
#include "entity.h"
/* Select only live actors; no pointer is retained across entity deletion. */
Entity *cs_scene_focus(Entity *player,Entity *actors,Entity *script_target,
                       unsigned event,unsigned face,int dialogue);
#endif
