# Host build of the EMU page, see sim.c. Needs a C compiler and LVGL 8.4:
#   make LVGL_DIR=/path/to/lvgl
ROOT     := ../..
LVGL_DIR ?= $(ROOT)/managed_components/lvgl__lvgl
OUT      ?= build

CC      ?= cc
CFLAGS  += -O1 -g0 -w -std=gnu99 \
           -DLV_CONF_SKIP -DLV_LVGL_H_INCLUDE_SIMPLE \
           -DLV_COLOR_DEPTH=16 -DLV_COLOR_16_SWAP=0 -DLV_MEM_SIZE='(256U*1024U)' \
           -I$(ROOT)/main -I$(LVGL_DIR) -I$(LVGL_DIR)/src

APP_SRC  := sim.c $(ROOT)/main/ui_emu.c $(ROOT)/main/emu_stream.c $(ROOT)/main/emu_bg.c $(wildcard $(ROOT)/main/fonts/*.c)
LVGL_SRC := $(shell find $(LVGL_DIR)/src -name '*.c')
LVGL_OBJ := $(patsubst $(LVGL_DIR)/%.c,$(OUT)/lvgl/%.o,$(LVGL_SRC))

all: $(OUT)/sim

$(OUT)/lvgl/%.o: $(LVGL_DIR)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(OUT)/sim: $(LVGL_OBJ) FORCE
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(APP_SRC) $(LVGL_OBJ) -lm -o $@

clean:
	rm -rf $(OUT)

.PHONY: all clean FORCE
FORCE:
