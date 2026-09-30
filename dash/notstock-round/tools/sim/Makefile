# Host build of the round gauge UI, see sim.c. Needs a C compiler and LVGL 8.4:
#   make LVGL_DIR=/path/to/lvgl
ROOT     := ../..
LVGL_DIR ?= $(ROOT)/managed_components/lvgl__lvgl
OUT      ?= build
# panel: 480 for the Waveshare 2.1" LCD, 466 for a 1.32" AMOLED
SIZE     ?= 480

CC      ?= cc
CFLAGS  += -O1 -g0 -w -std=gnu99 \
           -DLV_CONF_SKIP -DLV_LVGL_H_INCLUDE_SIMPLE \
           -DLV_COLOR_DEPTH=16 -DLV_COLOR_16_SWAP=0 -DLV_MEM_SIZE='(256U*1024U)' \
           -DLV_USE_METER=1 -DLV_FONT_MONTSERRAT_14=1 -DRND_SIZE=$(SIZE) -DRND_SIM \
           -I$(ROOT)/main -I$(LVGL_DIR) -I$(LVGL_DIR)/src

APP_SRC  := sim.c $(ROOT)/main/ui_round.c $(ROOT)/main/ui_round_dpf.c $(ROOT)/main/ui_round_set.c $(ROOT)/main/faces.c $(wildcard $(ROOT)/main/fonts/*.c)
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
