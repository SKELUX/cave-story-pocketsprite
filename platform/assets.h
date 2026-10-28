#ifndef CS_ASSETS_H
#define CS_ASSETS_H
#include <stddef.h>
typedef struct {
    const void *data;
    size_t size;
    size_t expanded;
} CsAsset;
extern const CsAsset cs_assets[];
extern const size_t cs_asset_count;
#endif
