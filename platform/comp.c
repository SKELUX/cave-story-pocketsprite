#include "common.h"
#include "md/comp.h"
#include "assets.h"
#include "zx0.h"

void zx0_unpack(const void *input, void *output) {
    for (size_t i = 0; i < cs_asset_count; ++i) {
        const CsAsset *asset = &cs_assets[i];
        if (asset->data != input) continue;
        if (asset->expanded && cs_zx0_decode(input, asset->size, output, asset->expanded) == asset->expanded)
            return;
        cs_assert_fail("ZX0 asset corrupt", __FILE__, __LINE__);
        return;
    }
    cs_assert_fail("ZX0 asset missing from registry", __FILE__, __LINE__);
}
