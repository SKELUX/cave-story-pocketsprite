#ifndef CS_ZX0_H
#define CS_ZX0_H
#include <stddef.h>
/* Decode forward ZX0 v2 (inverted offset Elias bits). SIZE_MAX on bad input. */
size_t cs_zx0_decode(const void *input, size_t input_size, void *output, size_t capacity);
#endif
