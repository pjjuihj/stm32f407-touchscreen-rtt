#!/usr/bin/env python3
"""
串口监控脚本 - 监控 STM32 串口输出
"""

import serial
import time
import sys

def monitor_serial(port='COM4', baudrate=115200, timeout=30):
    """
    监控串口输出

    Args:
        port: 串口端口
        baudrate: 波特率
        timeout: 监控超时时间（秒）
    """
    try:
        print(f'=== 串口监控 ===')
        print(f'端口: {port}, 波特率: {baudrate}')
        print(f'监控时间: {timeout} 秒')
        print(f'按 Ctrl+C 停止监控')
        print()

        ser = serial.Serial(port, baudrate, timeout=1)
        print('串口已打开')

        # 清空缓冲区
        ser.reset_input_buffer()

        # 发送复位信号
        print('发送复位信号...')
        ser.dtr = False
        time.sleep(0.1)
        ser.dtr = True
        time.sleep(0.1)

        # 监控串口输出
        print('开始监控...')
        print('-' * 50)

        start_time = time.time()
        log_count = 0

        try:
            while time.time() - start_time < timeout:
                if ser.in_waiting:
                    data = ser.readline()
                    if data:
                        try:
                            text = data.decode('utf-8', errors='replace')
                            if text.strip():
                                elapsed = time.time() - start_time
                                print(f'[{elapsed:.1f}s] {text}', end='')
                                log_count += 1
                        except:
                            pass
                time.sleep(0.01)
        except KeyboardInterrupt:
            print()

        print('-' * 50)
        print(f'监控结束，共收到 {log_count} 条日志')

        ser.close()
        print('串口已关闭')

        return log_count

    except serial.SerialException as e:
        print(f'串口错误: {e}')
        return 0
    except Exception as e:
        print(f'错误: {e}')
        return 0

def send_command(port='COM4', baudrate=115200, command='STATUS'):
    """
    发送命令并读取响应

    Args:
        port: 串口端口
        baudrate: 波特率
        command: 要发送的命令
    """
    try:
        print(f'=== 发送命令 ===')
        print(f'命令: {command}')

        ser = serial.Serial(port, baudrate, timeout=1)
        print('串口已打开')

        # 清空缓冲区
        ser.reset_input_buffer()

        # 发送命令
        ser.write(f'{command}\r\n'.encode())
        print(f'已发送: {command}')

        # 等待响应
        time.sleep(0.5)

        # 读取响应
        response = b''
        while ser.in_waiting:
            data = ser.read(ser.in_waiting)
            response += data

        if response:
            print(f'收到响应:')
            print(response.decode('utf-8', errors='replace'))
        else:
            print('无响应')

        ser.close()
        print('串口已关闭')

        return response

    except Exception as e:
        print(f'错误: {e}')
        return b''

if __name__ == '__main__':
    if len(sys.argv) > 1:
        if sys.argv[1] == 'monitor':
            timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 30
            monitor_serial(timeout=timeout)
        elif sys.argv[1] == 'send':
            command = sys.argv[2] if len(sys.argv) > 2 else 'STATUS'
            send_command(command=command)
        else:
            print('用法:')
            print('  python serial_monitor.py monitor [超时时间]')
            print('  python serial_monitor.py send [命令]')
    else:
        print('用法:')
        print('  python serial_monitor.py monitor [超时时间]')
        print('  python serial_monitor.py send [命令]')
