#!/usr/bin/env python3
import sys
import time
import serial

def main():
    port = "/dev/ttyUSB0"
    baud = 115200
    
    print(f"[PC] Connecting to {port} at {baud}...")
    try:
        ser = serial.Serial(port, baud, timeout=0.5)
    except Exception as e:
        print(f"[ERROR] Failed to open port: {e}")
        return

    print("[PC] Waiting for STM32 countdown...")
    ser.reset_input_buffer()
    
    # Wait for the countdown message
    buff = ""
    start_time = time.time()
    detected = False
    while time.time() - start_time < 10.0:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            sys.stdout.write(data)
            sys.stdout.flush()
            buff += data
            if "Starting 3s FOTA countdown" in buff:
                detected = True
                break
        time.sleep(0.01)
        
    if not detected:
        print("\n[WARN] Countdown not detected. Sending anyway...")
        
    # Wait 0.1s for UART to stabilize
    time.sleep(0.1)
    
    print("\n[PC] Sending 'HELLO' string...")
    ser.write(b"HELLO")
    ser.flush()
    
    # Read the response (which should print [RX] 0xXX for each byte)
    print("[PC] Reading response for 3 seconds...")
    start_time = time.time()
    while time.time() - start_time < 3.0:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            sys.stdout.write(data)
            sys.stdout.flush()
        time.sleep(0.01)
        
    ser.close()
    print("\n[PC] Done.")

if __name__ == "__main__":
    main()
