# ==========================================================
# Hello World for Ibex Simple System
# ==========================================================

# ---------- Paths ----------
IBEX_ROOT := $(HOME)/ibex

BUILD_DIR := build
SRC_DIR   := src

TARGET := AES

# ---------- Toolchain ----------
CC      := riscv64-unknown-elf-gcc
OBJCOPY := riscv64-unknown-elf-objcopy
OBJDUMP := riscv64-unknown-elf-objdump

# ---------- Runtime ----------
CRT := $(IBEX_ROOT)/examples/sw/simple_system/common/crt0.S

COMMON_DIR := $(IBEX_ROOT)/examples/sw/simple_system/common

LINKER := $(COMMON_DIR)/link.ld

# ---------- Architecture ----------
ARCH := rv32imc_zicsr
ABI  := ilp32

# ---------- Compiler ----------
CFLAGS := \
	-march=$(ARCH) \
	-mabi=$(ABI) \
	-static \
	-mcmodel=medany \
	-Wall \
	-g \
	-Os \
	-fvisibility=hidden \
	-ffreestanding \
	-nostdlib \
	-nostartfiles \
	-I$(COMMON_DIR)

# ---------- Linker ----------
LDFLAGS := \
	-T $(LINKER)

# ---------- Sources ----------
SRCS := \
	$(SRC_DIR)/main.c \
	$(COMMON_DIR)/simple_system_common.c \
	$(CRT)

OBJS := \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/simple_system_common.o \
	$(BUILD_DIR)/crt0.o

# ==========================================================
# Build Rules
# ==========================================================

all: $(BUILD_DIR)/$(TARGET).vmem

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/simple_system_common.o: $(COMMON_DIR)/simple_system_common.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/crt0.o: $(CRT) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).vmem: $(BUILD_DIR)/$(TARGET).bin
	srec_cat $< \
		-binary \
		-offset 0x0000 \
		-byte-swap 4 \
		-o $@ \
		-vmem

$(BUILD_DIR)/$(TARGET).dis: $(BUILD_DIR)/$(TARGET).elf
	$(OBJDUMP) -D $< > $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean