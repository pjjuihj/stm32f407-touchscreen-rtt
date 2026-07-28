#!/usr/bin/env python3
"""Final test for serial log system"""
import serial
import time

PORT = 'COM3'
BAUD = 115200

print(f"Final test on {PORT} @ {BAUD} bps")
print("=" * 50)

try:
    ser = serial.Serial(PORT, BAUD, timeout=2)
    ser.reset_input_buffer()
    time.sleep(1)

    def send_cmd(cmd):
        for ch in cmd:
            ser.write(ch.encode())
            time.sleep(0.02)
        ser.write(b'\r\n')
        time.sleep(0.5)
        return ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')

    # Test all commands
    tests = [
        ('HELP', 'Commands'),
        ('STATUS', 'TOUCH:'),
        ('TOUCH OFF', 'Touch log: OFF'),
        ('TOUCH ON', 'Touch log: ON'),
        ('LCD OFF', 'LCD log: OFF'),
        ('LCD ON', 'LCD log: ON'),
        ('SYSTEM OFF', 'System log: OFF'),
        ('SYSTEM ON', 'System log: ON'),
        ('LEVEL TOUCH 4', 'Touch level: DEBUG'),
        ('LEVEL TOUCH 3', 'Touch level: INFO'),
        ('LEVEL LCD 2', 'LCD level: WARNING'),
        ('LEVEL LCD 1', 'LCD level: ERROR'),
        ('LEVEL SYSTEM 3', 'System level: INFO'),
        ('LEVEL SYSTEM 4', 'System level: DEBUG'),
        ('CLEAR', 'Log cleared'),
        ('TEST RUN', '[PASS]'),
        ('DUMP', 'Log Dump'),
    ]

    print("\n=== Test Results ===")
    passed = 0
    failed = 0

    for cmd, expected in tests:
        resp = send_cmd(cmd)
        if expected in resp:
            print(f'[PASS] {cmd}')
            passed += 1
        else:
            print(f'[FAIL] {cmd}')
            failed += 1

    print(f'\n=== Summary ===')
    print(f'Total: {passed + failed}  Passed: {passed}  Failed: {failed}')
    print(f'Pass rate: {passed/(passed+failed)*100:.1f}%')

    ser.close()

except Exception as e:
    print(f"Error: {e}")
