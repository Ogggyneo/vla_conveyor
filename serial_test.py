"""Jog the axis once to verify wiring/direction and measure SPEED_TEST_* values.

Usage: python serial_test.py [PORT] [LEFT|RIGHT] [DURATION_MS]
Measure how far the carriage moved, then put that distance in
SPEED_TEST_DISTANCE_CM and DURATION_MS/1000 in SPEED_TEST_TIME_S in .env.
"""

import sys
import time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
DIRECTION = sys.argv[2].upper() if len(sys.argv) > 2 else "RIGHT"
DURATION_MS = int(sys.argv[3]) if len(sys.argv) > 3 else 1000
BAUD = 115200


def wait_for(board, expected, timeout_s):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        line = board.readline().decode(errors="ignore").strip()
        if line:
            print("YoloUNO ->", line)
        if line.startswith(expected):
            return line
    return None


board = serial.Serial(PORT, BAUD, timeout=0.25, write_timeout=1)
time.sleep(2.5)
board.reset_input_buffer()

board.write(b"PING\n")
board.flush()
if not wait_for(board, "PONG", 2):
    board.close()
    raise RuntimeError("Không nhận được PONG.")

board.write(b"POS\n")
wait_for(board, "POS,", 1)

command = f"MOVE,{DIRECTION},{DURATION_MS}\n"
board.write(command.encode("ascii"))
board.flush()
print("TX ->", command.strip())

if not wait_for(board, "DONE", DURATION_MS / 1000 + 3):
    board.write(b"STOP\n")
    raise RuntimeError("Không nhận được DONE.")

board.write(b"POS\n")
wait_for(board, "POS,", 1)
board.close()
print("Serial và motor test hoàn tất.")
