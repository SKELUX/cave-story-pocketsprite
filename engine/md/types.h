#ifndef MD_TYPES_H
#define MD_TYPES_H

/* Native host/ESP32 widths; never inherit the Mega Drive's long ABI. */
#include <stdint.h>
#include <stddef.h>
#define FALSE 0
#define TRUE 1

typedef struct {
    uint16_t width, height, frames, fsize;
    uint32_t tiles[0];
} LocSprite;

typedef struct {
    int16_t y;
    union {
        struct {
            uint8_t size;
            uint8_t link;
        };
        uint16_t size_link;
    };
    uint16_t attr;
    int16_t x;
} Sprite;

typedef struct {
	uint16_t numTile;
	const uint32_t *tiles;
} TileSet;

typedef struct {
	int16_t y;
	uint16_t size;
	int16_t x;
	uint16_t numTile;
} VDPSpriteInf;

typedef struct {
	uint16_t 		numFrame;
	const TileSet * const *tilesets;
	uint16_t 		numSprite;
	const VDPSpriteInf * const *sprites;
	int16_t			w;
	int16_t			h;
} SpriteDef;

#endif //MD_TYPES_H
