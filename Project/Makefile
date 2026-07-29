# TOUCH Project Makefile for GCC ARM

CC = /d/arm-gnu-toolchain-15.2/arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gcc.exe
OBJCOPY = /d/arm-gnu-toolchain-15.2/arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-objcopy.exe
SIZE = /d/arm-gnu-toolchain-15.2/arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-size.exe

TARGET = TOUCH
CPU = -mcpu=cortex-m4 -mthumb
FPU = -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS = $(CPU) $(FPU) -c -DUSE_HAL_DRIVER -DSTM32F407xx -O0 -Wall -Wno-unused-variable -Wno-unused-but-set-variable

INCLUDES = \
	-I. \
	-I../Common \
	-I../Main \
	-I../Startup_config \
	-I../STM32F4xx_HAL_Driver/inc \
	-I../USER/LED \
	-I../USER/LCD \
	-I../USER/BEEP \
	-I../USER/KEY \
	-I../USER/TOUCH \
	-I../USER/USART \
	-I../USER/LOG \
	-I../TEST \
	-I../GUI \
	-I../LVGL \
	-I../LVGL/src \
	-I../LVGL/src/drivers/display/ili9341

LDSCRIPT = STM32F407VGTx_FLASH.ld
LDFLAGS = $(CPU) $(FPU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=$(TARGET).map

BUILD_DIR = build

# User sources
USER_SRC = ../Main/main.c ../Main/stm32f4xx_it.c ../GUI/gui_driver.c \
	../USER/LED/led.c ../USER/BEEP/beep.c ../USER/KEY/key.c \
	../USER/LCD/lcd.c ../USER/TOUCH/touch.c ../USER/TOUCH/ft5426.c \
	../USER/TOUCH/xpt2046.c ../USER/USART/usart.c ../USER/LOG/log.c \
	../TEST/test_log.c ../Common/common.c \
	../Startup_config/system_stm32f4xx.c ../Startup_config/stm32f4xx_hal_msp.c

# Startup
STARTUP_SRC = ../Startup_config/startup_stm32f407xx_gcc.s

# HAL (only needed ones)
HAL_SRC = \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_cortex.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_gpio.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_pwr.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_pwr_ex.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_rcc.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_rcc_ex.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_dma.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_dma_ex.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_uart.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_sram.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_ll_fsmc.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_flash.c \
	../STM32F4xx_HAL_Driver/src/stm32f4xx_hal_flash_ex.c

# LVGL sources - collect all .c files, exclude tests/debugging/platform-specific
LVGL_SRC = $(shell find ../LVGL/src -name "*.c" \
	-not -path "*/test/*" \
	-not -path "*/examples/*" \
	-not -path "*/debugging/*" \
	-not -path "*/sdl/*" \
	-not -path "*/wayland/*" \
	-not -path "*/x11/*" \
	-not -path "*/windows/*" \
	-not -path "*/opengles/*" \
	-not -path "*/qnx/*" \
	-not -path "*/uefi/*" \
	-not -path "*/nuttx/*" \
	-not -path "*/evdev/*" \
	-not -path "*/libinput/*" \
	-not -path "*/drm/*" \
	-not -path "*/fb/*" \
	-not -path "*/ft81x/*" \
	-not -path "*/nv3007/*" \
	-not -path "*/nxp_elcdif/*" \
	-not -path "*/renesas_glcdc/*" \
	-not -path "*/st_ltdc/*" \
	-not -path "*/eve/*" \
	-not -path "*/vg_lite/*" \
	-not -path "*/nema_gfx/*" \
	-not -path "*/nanovg/*" \
	-not -path "*/espressif/*" \
	-not -path "*/nxp/g2d/*" \
	-not -path "*/nxp/pxp/*" \
	-not -path "*/renesas/dave2d/*" \
	-not -path "*/helium/*" \
	-not -path "*/neon/*" \
	-not -path "*/riscv_v/*" \
	-not -path "*/gltf/*" \
	-not -path "*/ffmpeg/*" \
	-not -path "*/freetype/*" \
	-not -path "*/gstreamer/*" \
	-not -path "*/libjpeg_turbo/*" \
	-not -path "*/libpng/*" \
	-not -path "*/libwebp/*" \
	-not -path "*/rlottie/*" \
	-not -path "*/svg/*" \
	-not -path "*/lottie/*" \
	-not -path "*/gif/*" \
	-not -path "*/qrcode/*" \
	-not -path "*/lz4/*" \
	-not -path "*/frogfs/*" \
	-not -path "*/FT800*/*" \
	-not -path "*/vg_lite_driver/*" \
	-not -path "*/objx_templ/*" \
	-not -path "*/micropython/*" \
	-not -path "*/rtthread/*" \
	-not -path "*/font_manager/*" \
	-not -path "*/imgfont/*")

# Object files
USER_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(USER_SRC))
HAL_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(HAL_SRC))
LVGL_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(LVGL_SRC))
ASM_OBJ = $(BUILD_DIR)/$(STARTUP_SRC:.s=.o)

ALL_OBJ = $(USER_OBJ) $(HAL_OBJ) $(LVGL_OBJ) $(ASM_OBJ)

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex
	@echo ""
	@echo "=== Build complete ==="
	@$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC $(notdir $<)"
	@$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "AS $(notdir $<)"
	@$(CC) $(CPU) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(ALL_OBJ)
	@echo "LD $@"
	@$(CC) $(ALL_OBJ) $(LDFLAGS) -o $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	@$(OBJCOPY) -O ihex $< $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
