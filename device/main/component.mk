# Native gameplay and backends. Original engine/md assembly is never compiled.
COMPONENT_SRCDIRS := ../../engine ../../engine/ai ../../engine/db ../../platform ../../build/device-assets
COMPONENT_ADD_INCLUDEDIRS := ../../platform ../../build/generated ../../upstream/opus-1.3.1/include
COMPONENT_ADD_LDFLAGS := -lmain $(COMPONENT_PATH)/../../build/opus/.libs/libopus.a
COMPONENT_ADD_LINKER_DEPS := $(COMPONENT_PATH)/../../build/opus/.libs/libopus.a
CPPFLAGS += -iquote $(COMPONENT_PATH)/../../engine
# The game/render loops need speed optimization; keep debug symbols/assertions.
# Legacy engine resource tables contain type-punned data, so disable alias
# assumptions rather than letting optimization reinterpret those accesses.
CFLAGS += -O2 -fno-strict-aliasing -std=gnu11 -fcommon -Wno-error=unused-variable -Wno-error=unused-function
