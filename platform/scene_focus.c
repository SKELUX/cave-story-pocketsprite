#include "scene_focus.h"
#include "npc.h"
/* Face identities are from res/tiles_c/face*.png and the English TSC dialogue.
 * Alternative expressions and NPC variants share one identity. */
enum { NONE,SUE,BALROG,TOROKO,KING,JACK,KAZUMA,JENKA,MISERY,BOOSTER,CURLY,MOMORIN,DOCTOR,ITOH,BALLOS,OTHER };
static unsigned identity(unsigned type) {
    switch(type) {
    case OBJ_SUE:case OBJ_SUE_AT_COMPUTER:case OBJ_SUE_TELEPORT_IN:case OBJ_SUE_FRENZIED:return SUE;
    case OBJ_BALROG:case OBJ_BALROG_DROP_IN:case OBJ_BALROG_BUST_IN:
    case OBJ_BALROG_BOSS_RUNNING:case OBJ_BALROG_BOSS_FLYING:case OBJ_BALROG_BOSS_MISSILES:
    case OBJ_BALROG_MEDIC:case OBJ_BALROG_FLYING:return BALROG;
    case OBJ_TOROKO:case OBJ_TOROKO_SHACK:case OBJ_TOROKO_TELEPORT_IN:case OBJ_TOROKO_FRENZIED:return TOROKO;
    case OBJ_KING:return KING;
    case OBJ_JACK:return JACK;
    case OBJ_KAZUMA:case OBJ_KAZUMA_AT_COMPUTER:return KAZUMA;
    case OBJ_JENKA:case OBJ_JENKA_COLLAPSED:return JENKA;
    case OBJ_MISERY_FLOAT:case OBJ_MISERY_STAND:case OBJ_BOSS_MISERY:
    case OBJ_MISERY_FRENZIED:case OBJ_MISERY_WIND:return MISERY;
    case OBJ_PROFESSOR_BOOSTER:case OBJ_BOOSTER_FALLING:return BOOSTER;
    case OBJ_CURLY:case OBJ_CURLY_BOSS:case OBJ_CURLY_COLLAPSED:
    case OBJ_CURLY_AI:case OBJ_CURLY_CARRIED:case OBJ_CURLY_CARRIED_SHOOTING:return CURLY;
    case OBJ_MOMORIN:return MOMORIN;
    case OBJ_DOCTOR:case OBJ_DOCTOR_CROWNED:case OBJ_BOSS_DOCTOR:
    case OBJ_BOSS_DOCTOR_FRENZIED:case OBJ_DOCTOR_GHOST:return DOCTOR;
    case OBJ_ITOH:return ITOH;
    case OBJ_BALLOS_PRIEST:return BALLOS;
    case OBJ_SANTA:case OBJ_SANTA_CAGED:case OBJ_CHACO:case OBJ_CHACO_CAGED:return OTHER;
    default:return NONE;
    }
}
static unsigned speaker(unsigned face) {
    static const uint8_t faces[]={NONE,SUE,SUE,SUE,SUE,BALROG,TOROKO,KING,TOROKO,JACK,
        KAZUMA,TOROKO,KING,JENKA,BALROG,MISERY,MISERY,BOOSTER,BOOSTER,CURLY,CURLY,
        KAZUMA,MOMORIN,BALROG,DOCTOR,CURLY,MISERY,SUE,ITOH,BALLOS};
    return face<sizeof(faces)?faces[face]:NONE;
}
Entity *cs_scene_focus(Entity *player,Entity *actors,Entity *script_target,
                       unsigned event,unsigned face,int dialogue) {
    Entity *talker=NULL,*script=NULL,*animated=NULL;
    int64_t talk_distance=INT64_MAX;
    unsigned who=dialogue?speaker(face):NONE;
    for(Entity *e=actors;e;e=e->next) {
        if(e->hidden) continue;
        /* Resolve explicit targets through the live list before dereferencing. */
        if(e==script_target) script=e;
        if(event && e->event==event) animated=e;
        unsigned person=identity(e->type);
        if(!person) continue;
        int64_t dx=(int64_t)e->x-player->x,dy=(int64_t)e->y-player->y;
        int64_t distance=(dx<0?-dx:dx)+(dy<0?-dy:dy);
        if(who && who==person && distance<talk_distance) {talker=e;talk_distance=distance;}
    }
    if(talker) return talker;
    if(script) return script;
    if(animated) return animated;
    /* Only script participants can override the player. Ambient NPC movement
     * is unrelated to chest/item scripts and must not pull the camera away. */
    return player;
}
