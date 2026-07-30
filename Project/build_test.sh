#!/bin/bash
# Test build script for TOUCH project

CC="/d/arm-gnu-toolchain-15.2/arm-gnu-toolchain-15.2.rel1-mingw-w64-i686-arm-none-eabi/bin/arm-none-eabi-gcc.exe"
CFLAGS="-mcpu=cortex-m4 -mthumb -c -DUSE_HAL_DRIVER -DSTM32F407xx -O0 -Wall"
INCLUDES="-I. -I../Common -I../Main -I../Startup_config -I../STM32F4xx_HAL_Driver/inc -I../USER/LED -I../USER/LCD -I../USER/BEEP -I../USER/KEY -I../USER/TOUCH -I../USER/USART -I../USER/LOG -I../TEST -I../GUI -I../LVGL -I../LVGL/src -I../LVGL/src/drivers/display/ili9341"

cd "$(dirname "$0")"

ERRORS=0
TOTAL=0

echo "=== Compiling user source files ==="
for f in ../Main/main.c ../GUI/gui_driver.c ../USER/LED/led.c ../USER/BEEP/beep.c ../USER/KEY/key.c ../USER/LCD/lcd.c ../USER/USART/usart.c ../USER/LOG/log.c ../USER/TOUCH/touch.c ../USER/TOUCH/ft5426.c ../USER/TOUCH/xpt2046.c ../Common/common.c ../STM32F4xx_HAL_Driver/src/stm32f4xx_hal.c; do
    TOTAL=$((TOTAL+1))
    NAME=$(basename "$f" .c)
    OUTPUT="/tmp/${NAME}.o"
    if $CC $CFLAGS $INCLUDES "$f" -o "$OUTPUT" 2>/tmp/${NAME}.err; then
        echo "✓ $NAME"
    else
        echo "✗ $NAME"
        cat /tmp/${NAME}.err
        ERRORS=$((ERRORS+1))
    fi
done

echo ""
echo "=== Compiling LVGL core files ==="
for f in ../LVGL/src/core/*.c; do
    TOTAL=$((TOTAL+1))
    NAME=$(basename "$f" .c)
    OUTPUT="/tmp/${NAME}.o"
    if $CC $CFLAGS $INCLUDES "$f" -o "$OUTPUT" 2>/tmp/${NAME}.err; then
        echo "✓ $NAME"
    else
        echo "✗ $NAME"
        cat /tmp/${NAME}.err | head -5
        ERRORS=$((ERRORS+1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Total: $TOTAL, Errors: $ERRORS"
