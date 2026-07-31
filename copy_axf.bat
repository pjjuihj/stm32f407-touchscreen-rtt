@echo off
REM Copy AXF to English path for GDB debugging
mkdir "C:\Users\CMJ\Desktop\build_output" 2>nul
copy "c:\Users\CMJ\Desktop\TEST\触摸屏_usart\Project\OBJ\TOUCH.axf" "C:\Users\CMJ\Desktop\build_output\TOUCH.axf" /Y
echo AXF copied to C:\Users\CMJ\Desktop\build_output\TOUCH.axf
