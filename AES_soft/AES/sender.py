#!/usr/bin/env python3

import serial
import serial.tools.list_ports
import os

DEFAULT_BAUD = 115200

import struct

MAGIC = b"FPGA"

TYPE_TEXT  = 1
TYPE_IMAGE = 2
TYPE_PDF   = 3
TYPE_ZIP   = 4
TYPE_BIN   = 5

def get_file_type(path):
    ext = os.path.splitext(path)[1].lower()

    if ext in [".jpg", ".jpeg", ".png", ".bmp", ".gif"]:
        return TYPE_IMAGE

    if ext == ".pdf":
        return TYPE_PDF

    if ext == ".zip":
        return TYPE_ZIP

    return TYPE_BIN

def build_header(path):
    size = os.path.getsize(path)

    filename = os.path.basename(path).encode("utf-8")
    filename_len = len(filename)

    file_type = get_file_type(path)

    header = struct.pack(
        "<4sBHQ",
        MAGIC,
        file_type,
        filename_len,
        size
    )

    return header, filename, size

def list_serial_ports():
    """Return a list of available serial ports."""
    return list(serial.tools.list_ports.comports())

def read_exact(ser, size):

    data = bytearray()

    while len(data) < size:
        chunk = ser.read(size - len(data))

        if not chunk:
            raise RuntimeError("Serial connection closed")

        data.extend(chunk)

    return bytes(data)

def wait_for_magic(ser):

    window = bytearray()

    while True:

        b = ser.read(1)

        if not b:
            continue

        window += b

        if len(window) > 4:
            window.pop(0)

        if bytes(window) == MAGIC:
            return

def select_port():
    ports = list_serial_ports()

    if not ports:
        print("No serial ports found.")
        return None

    print("\nAvailable Serial Ports")
    print("----------------------")

    for i, port in enumerate(ports, start=1):
        print(f"{i}. {port.device} ({port.description})")

    while True:
        try:
            choice = int(input("\nSelect port: "))

            if 1 <= choice <= len(ports):
                return ports[choice - 1].device

            print("Invalid selection.")

        except ValueError:
            print("Enter a number.")


def open_serial(port_name):
    try:
        ser = serial.Serial(
            port=port_name,
            baudrate=DEFAULT_BAUD,
            timeout=1
        )

        print(f"\nConnected to {port_name}")
        return ser

    except serial.SerialException as e:
        print(f"\nFailed to open port:\n{e}")
        return None

def send_text(ser):
    text = input("\nEnter text: ")
    data = text.encode("utf-8")

    # Build a proper host_header_t-compatible header so the FPGA's
    # send_mode() (which always expects magic+type+filename_len+size
    # first) parses this the same way it parses a file transfer.
    filename_bytes = b"message.txt"

    header = struct.pack(
        "<4sBHQ",
        MAGIC,
        TYPE_TEXT,
        len(filename_bytes),
        len(data)
    )

    print("Sending:", data.hex(" "))

    ser.write(header)
    ser.write(filename_bytes)
    ser.write(data)
    ser.flush()

    print("Bytes waiting:", ser.out_waiting)

def send_file(ser):
    path = input("\nEnter file path: ").strip()

    if not os.path.isfile(path):
        print("File not found.")
        return

    header, filename, size = build_header(path)

    print(f"\nFile : {os.path.basename(path)}")
    print(f"Size : {size} bytes")
    print("Sending...\n")

    sent = 0
    ser.write(header)
    ser.write(filename)

    with open(path, "rb") as f:
        while True:
            chunk = f.read(1024)

            if not chunk:
                break

            ser.write(chunk)
            sent += len(chunk)

            percent = sent * 100 / size
            bar_length = 40
            filled = int(bar_length * sent / size)

            bar = "█" * filled + "-" * (bar_length - filled)

            print(
                f"\r[{bar}] {percent:6.2f}% ({sent}/{size} bytes)",
                end="",
                flush=True
            )

    ser.flush()

    print("\nDone.")

def menu(ser):

    while True:

        print("\n========================")
        print("1. Send text")
        print("2. Send file")
        print("3. Exit")
        print("========================")

        choice = input("Choice: ").strip()

        if choice == "1":
            send_text(ser)

        elif choice == "2":
            send_file(ser)

        elif choice == "3":
            break

        else:
            print("Invalid choice.")


def main():

    port = select_port()

    if port is None:
        return

    ser = open_serial(port)

    if ser is None:
        return

    print("\nConnection successful.")

    try:
        menu(ser)
    finally:
        ser.close()


if __name__ == "__main__":
    main()