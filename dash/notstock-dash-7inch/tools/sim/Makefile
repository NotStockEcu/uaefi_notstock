# Host build of the dash UI, see sim.c. Needs a C compiler and LVGL 8.4.
#
# After one `idf.py build` the component manager has already fetched LVGL
# into managed_components/, which is the default below. Otherwise point
# LVGL_DIR at any LVGL v8.4 checkout:  make LVGL_DIR=/path/to/lvgl

ROOT     := ../..
LVGL_DIR ?= $(ROOT)/managed_components/lvgl__lvgl
OUT      ?= build

CC      ?= cc
# pointers are 8 bytes here, so the LVGL heap is 4x the 64 kB on the panel
CFLAGS  += -O1 -g0 -w -std=gnu99 \
           -DLV_CONF_SKIP -DLV_LVGL_H_INCLUDE_SIMPLE \
           -DLV_COLOR_DEPTH=16 -DLV_COLOR_16_SWAP=0 -DLV_MEM_SIZE='(256U*1024U)' \
           -DLV_USE_METER=1 -DLV_FONT_MONTSERRAT_14=1 \
           -Istubs -I$(ROOT)/main -I$(LVGL_DIR) -I$(LVGL_DIR)/src

APP_SRC := sim.c \
           $(ROOT)/main/ui.c $(ROOT)/main/ui_menu.c $(ROOT)/main/settings.c \
           $(ROOT)/main/icons.c $(ROOT)/main/dials.c $(ROOT)/main/splash.c $(ROOT)/main/boot_anim.c \
           $(wildcard $(ROOT)/main/fonts/*.c)
LVGL_SRC := $(shell find $(LVGL_DIR)/src -name '*.c')

LVGL_OBJ := $(patsubst $(LVGL_DIR)/%.c,$(OUT)/lvgl/%.o,$(LVGL_SRC))

all: $(OUT)/sim

$(OUT)/lvgl/%.o: $(LVGL_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# the app sources are few and change often: always rebuild them
$(OUT)/sim: $(LVGL_OBJ) FORCE
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(APP_SRC) $(LVGL_OBJ) -lm -o $@

clean:
	rm -rf $(OUT)

.PHONY: all clean FORCE
FORCE:
