/* Native forward ZX0 v2 decoder. Format: Einar Saukas's ZX0, as emitted by
 * upstream tools/salvador. Bounds are checked before all reads and copies.
 */
#include "zx0.h"
#include <stdint.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t size, pos;
    unsigned mask, bits;
    int error;
} Reader;

static unsigned bit(Reader *r) {
    if (!r->mask) {
        if (r->pos == r->size) { r->error = 1; return 1; }
        r->bits = r->data[r->pos++];
        r->mask = 128;
    }
    unsigned result = !!(r->bits & r->mask);
    r->mask >>= 1;
    return result;
}

static unsigned elias(Reader *r, unsigned first, unsigned invert) {
    unsigned value = 1;
    while (!first && !r->error) {
        if (value > 32767) { r->error = 1; break; }
        value = value * 2 + (bit(r) ^ invert);
        first = bit(r);
    }
    return value;
}

size_t cs_zx0_decode(const void *input, size_t input_size, void *output, size_t capacity) {
    Reader r = {input, input_size, 0, 0, 0, 0};
    uint8_t *out = output;
    size_t used = 0, offset = 1;
    unsigned match = 0;
    for (;;) {
        if (!match) {
            unsigned length = elias(&r, bit(&r), 0);
            if (r.error || length > r.size - r.pos || length > capacity - used)
                return SIZE_MAX;
            memcpy(out + used, r.data + r.pos, length);
            used += length;
            r.pos += length;
            match = bit(&r);
        }
        unsigned length;
        if (match) {
            unsigned high = elias(&r, bit(&r), 1);
            if (r.error) return SIZE_MAX;
            if (high == 256) return used;
            if (high > 256 || r.pos == r.size) return SIZE_MAX;
            unsigned low = r.data[r.pos++];
            offset = (high - 1) * 128 + 128 - (low >> 1);
            length = elias(&r, low & 1, 0) + 1;
        } else {
            length = elias(&r, bit(&r), 0);
        }
        if (r.error || offset > used || length > capacity - used) return SIZE_MAX;
        for (unsigned i = 0; i < length; ++i) {
            out[used] = out[used - offset];
            ++used;
        }
        match = bit(&r);
        if (r.error) return SIZE_MAX;
    }
}
