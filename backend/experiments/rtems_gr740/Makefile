# Flags follow /workspace/experiments/common.mk and the installed GR740 BSP.
RTEMS_ROOT ?= /opt/rtems/6
RTEMS_CC = $(RTEMS_ROOT)/bin/sparc-rtems6-gcc
RTEMS_PC = $(RTEMS_ROOT)/lib/pkgconfig/sparc-rtems6-gr740.pc
OUT ?= /tmp/yarda-rtems-gr740
KERNEL ?= atax
DEFINES ?= -DM=38 -DN=42
RTEMS_FLAGS = -std=c11 -O0 -g -mcpu=leon3 -mfpu -mhard-float \
  -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unknown-pragmas
RTEMS_CFLAGS = $(shell pkg-config --cflags $(RTEMS_PC))
RTEMS_LIBS = $(shell pkg-config --libs $(RTEMS_PC)) \
  -lrtemsbsp -lrtemstest -lrtemscpu -lm

.PHONY: all native run
all:
	mkdir -p $(OUT)
	$(RTEMS_CC) $(RTEMS_FLAGS) $(RTEMS_CFLAGS) $(DEFINES) \
	  init.c kernels/$(KERNEL).c -Wl,-Map,$(OUT)/input.map \
	  $(RTEMS_LIBS) -o $(OUT)/input.elf

native:
	mkdir -p $(OUT)
	$(CC) -std=c11 -O0 -g -Wall -Wextra -Wno-unknown-pragmas $(DEFINES) \
	  native_main.c kernels/$(KERNEL).c -o $(OUT)/native-check
	$(OUT)/native-check

run:
	laysim-gr740-cli -r -core0 $(OUT)/input.elf
