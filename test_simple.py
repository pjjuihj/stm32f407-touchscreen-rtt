#!/usr/bin/env python3
"""Simple test for current implementation"""
import serial
import time

PORT = 'COM3'
BAUD = 115200

print(f"Testing serial log system on {PORT} @ {BAUD} bps")
print("=" * 50)

try:
    ser = serial.Serial(PORT, BAUD, timeout=2)
    ser.reset_input_buffer()

    # Wait for boot message
    time.sleep(1)
    boot_msg = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Boot: {boot_msg.strip()}")

    # Test 1: HELP
    print("\n--- Test 1: HELP ---")
    ser.write(b'HELP\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()[:100]}...")
    test1 = "Commands" in resp
    print(f"Result: {'PASS' if test1 else 'FAIL'}")

    # Test 2: STATUS
    print("\n--- Test 2: STATUS ---")
    ser.write(b'STATUS\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()}")
    test2 = "TOUCH: ON" in resp and "LCD: ON" in resp
    print(f"Result: {'PASS' if test2 else 'FAIL'}")

    # Test 3: TOUCH OFF
    print("\n--- Test 3: TOUCH OFF ---")
    ser.write(b'TOUCH OFF\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()}")
    test3 = "Touch log: OFF" in resp
    print(f"Result: {'PASS' if test3 else 'FAIL'}")

    # Test 4: Verify TOUCH OFF
    print("\n--- Test 4: Verify TOUCH OFF ---")
    ser.write(b'STATUS\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()}")
    test4 = "TOUCH: OFF" in resp
    print(f"Result: {'PASS' if test4 else 'FAIL'}")

    # Test 5: TOUCH ON
    print("\n--- Test 5: TOUCH ON ---")
    ser.write(b'TOUCH ON\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()}")
    test5 = "Touch log: ON" in resp
    print(f"Result: {'PASS' if test5 else 'FAIL'}")

    # Test 6: DUMP
    print("\n--- Test 6: DUMP ---")
    ser.write(b'DUMP\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()[:100]}...")
    test6 = "Log Dump" in resp and "End" in resp
    print(f"Result: {'PASS' if test6 else 'FAIL'}")

    # Test 7: CLEAR
    print("\n--- Test 7: CLEAR ---")
    ser.write(b'CLEAR\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()}")
    test7 = "Log cleared" in resp
    print(f"Result: {'PASS' if test7 else 'FAIL'}")

    # Test 8: Echo
    print("\n--- Test 8: Echo ---")
    ser.write(b'12345\r\n')
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
    print(f"Response: {resp.strip()}")
    test8 = "12345" in resp
    print(f"Result: {'PASS' if test8 else 'FAIL'}")

    # Summary
    print("\n" + "=" * 50)
    tests = [test1, test2, test3, test4, test5, test6, test7, test8]
    passed = sum(tests)
    print(f"Total: {len(tests)}  Passed: {passed}  Failed: {len(tests)-passed}")
    print(f"Pass rate: {passed/len(tests)*100:.1f}%")

    ser.close()

except Exception as e:
    print(f"Error: {e}")
