# Host build of the 2.8" T5.1 pages, see sim.c. Needs a C compiler and LVGL 8.4:
#   make LVGL_DIR=/path/to/lvgl
ROOT     := ../..
L2       := $(ROOT)/../notstock-lcd2-emu/main
LVGL_DIR ?= $(ROOT)/lvgl
OUT      ?= build

CC      ?= cc
CFLAGS  += -O1 -g0 -w -std=gnu99 \
           -DLV_CONF_SKIP -DLV_LVGL_H_INCLUDE_SIMPLE \
           -DLV_COLOR_DEPTH=16 -DLV_COLOR_16_SWAP=0 -DLV_MEM_SIZE='(128U*1024U)' \
           -I$(ROOT)/main -I$(LVGL_DIR) -I$(LVGL_DIR)/src

APP_SRC  := sim.c $(ROOT)/main/ui_t5.c $(L2)/l2_bg.c $(wildcard $(L2)/fonts/*.c)
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

FORCE:
.PHONY: all clean FORCE
