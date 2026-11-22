# #!/usr/bin/env python3

# import struct
# import serial
# import serial.tools.list_ports


# DEFAULT_BAUD = 115200

# # ---------------------------------------------------------------------------
# # Host transfer protocol
# # ---------------------------------------------------------------------------

# # FPGA sends this immediately before the binary host header.
# HOST_START = b"\xD3\x91\x7E\x42"

# # Header sent by FPGA:
# #
# #   4 bytes  magic       "FPGA"
# #   1 byte   file type
# #   2 bytes  filename length
# #   8 bytes  file size
# #
# # Total = 15 bytes
# HEADER_FORMAT = "<4sBHQ"
# HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

# MAGIC = b"FPGA"


# # ---------------------------------------------------------------------------
# # File types
# # ---------------------------------------------------------------------------

# TYPE_TEXT  = 1
# TYPE_IMAGE = 2
# TYPE_PDF   = 3
# TYPE_ZIP   = 4
# TYPE_BIN   = 5


# # ---------------------------------------------------------------------------
# # Serial port
# # ---------------------------------------------------------------------------

# def list_serial_ports():
#     return list(serial.tools.list_ports.comports())


# def select_port():

#     ports = list_serial_ports()

#     if not ports:
#         print("No serial ports found.")
#         return None

#     print("\nAvailable Serial Ports")
#     print("----------------------")

#     for i, port in enumerate(ports, start=1):
#         print(
#             f"{i}. {port.device} "
#             f"({port.description})"
#         )

#     while True:

#         try:
#             choice = int(input("\nSelect port: "))

#             if 1 <= choice <= len(ports):
#                 return ports[choice - 1].device

#             print("Invalid selection.")

#         except ValueError:
#             print("Enter a number.")


# def open_serial(port_name):

#     try:

#         ser = serial.Serial(
#             port=port_name,
#             baudrate=DEFAULT_BAUD,
#             timeout=1
#         )

#         print(f"\nConnected to {port_name}")

#         return ser

#     except serial.SerialException as e:

#         print("\nFailed to open port:")
#         print(e)

#         return None


# # ---------------------------------------------------------------------------
# # Serial reading helpers
# # ---------------------------------------------------------------------------

# def read_exact(ser, size):
#     """
#     Read exactly 'size' bytes.

#     Timeout is allowed because the FPGA may take some time between
#     transmissions. We simply continue waiting.
#     """

#     data = bytearray()

#     while len(data) < size:

#         try:
#             chunk = ser.read(size - len(data))

#         except serial.SerialException as e:
#             raise RuntimeError(
#                 f"Serial connection error: {e}"
#             )

#         if chunk:
#             data.extend(chunk)

#     return bytes(data)


# def wait_for_start(ser):
#     """
#     Ignore EVERYTHING coming from UART0 until the FPGA sends:

#         D3 91 7E 42

#     This means terminal/debug output can freely appear on UART0.
#     """

#     window = bytearray()

#     while True:

#         try:
#             b = ser.read(1)

#         except serial.SerialException as e:
#             raise RuntimeError(
#                 f"Serial connection error: {e}"
#             )

#         if not b:
#             continue

#         window.append(b[0])

#         if len(window) > len(HOST_START):
#             window.pop(0)

#         if bytes(window) == HOST_START:
#             return


# # ---------------------------------------------------------------------------
# # Header
# # ---------------------------------------------------------------------------

# def receive_header(ser):

#     # HOST_START has already been consumed.

#     header = read_exact(
#         ser,
#         HEADER_SIZE
#     )

#     magic, file_type, filename_len, file_size = struct.unpack(
#         HEADER_FORMAT,
#         header
#     )

#     # The start marker tells us that a transfer has begun.
#     # The actual header must still contain "FPGA".
#     if magic != MAGIC:

#         raise RuntimeError(
#             f"Invalid header magic: {magic!r}"
#         )

#     return (
#         file_type,
#         filename_len,
#         file_size
#     )


# # ---------------------------------------------------------------------------
# # Filename
# # ---------------------------------------------------------------------------

# def filename_for_type(file_type):

#     if file_type == TYPE_TEXT:
#         return "received_msg.txt"

#     if file_type == TYPE_IMAGE:
#         return "received_img.png"

#     if file_type == TYPE_PDF:
#         return "received_file.pdf"

#     if file_type == TYPE_ZIP:
#         return "received_file.zip"

#     if file_type == TYPE_BIN:
#         return "received_file.bin"

#     return "received_file.bin"


# # ---------------------------------------------------------------------------
# # File reception
# # ---------------------------------------------------------------------------

# def receive_file(ser):

#     print("\nWaiting for FPGA data...")

#     # ---------------------------------------------------------------
#     # 1. Ignore all UART0 terminal/debug output
#     # ---------------------------------------------------------------

#     wait_for_start(ser)

#     print("Transfer start detected.")

#     # ---------------------------------------------------------------
#     # 2. Read binary header
#     # ---------------------------------------------------------------

#     file_type, filename_len, file_size = receive_header(ser)

#     print(f"File type : {file_type}")
#     print(f"File size : {file_size} bytes")

#     # ---------------------------------------------------------------
#     # 3. Sanity check
#     # ---------------------------------------------------------------

#     if file_size == 0:

#         print("Warning: received file has zero length.")

#     # Prevent obviously corrupted headers from causing the receiver
#     # to try to allocate/read an absurd amount of data.
#     #
#     # Adjust this if you want to support larger files.

#     MAX_FILE_SIZE = 100 * 1024 * 1024

#     if file_size > MAX_FILE_SIZE:

#         raise RuntimeError(
#             f"Invalid file size: {file_size} bytes"
#         )

#     # ---------------------------------------------------------------
#     # 4. Read ONLY the file payload
#     # ---------------------------------------------------------------

#     print(
#         f"Receiving {file_size} bytes..."
#     )

#     data = read_exact(
#         ser,
#         file_size
#     )

#     # ---------------------------------------------------------------
#     # 5. Save file
#     # ---------------------------------------------------------------

#     out_name = filename_for_type(file_type)

#     with open(out_name, "wb") as f:
#         f.write(data)

#     print(
#         f"Saved as: {out_name}"
#     )

#     print(
#         f"Received: {len(data)} bytes"
#     )

#     return out_name


# # ---------------------------------------------------------------------------
# # Main
# # ---------------------------------------------------------------------------

# def main():

#     port = select_port()

#     if port is None:
#         return

#     ser = open_serial(port)

#     if ser is None:
#         return

#     print("\nReceiver started.")
#     print("Ignoring UART0 terminal output.")
#     print(
#         "Waiting for FPGA transfer marker "
#         "(D3 91 7E 42)..."
#     )

#     try:

#         while True:

#             try:

#                 filename = receive_file(ser)

#                 print(
#                     f"\nTransfer complete: {filename}"
#                 )

#                 print(
#                     "Ready for the next transfer."
#                 )

#             except RuntimeError as e:

#                 print(
#                     f"\nReceive error: {e}"
#                 )

#                 print(
#                     "Returning to transfer detection..."
#                 )

#     except KeyboardInterrupt:

#         print("\nStopped.")

#     finally:

#         ser.close()


# if __name__ == "__main__":
#     main()

#!/usr/bin/env python3

import struct
import serial
import serial.tools.list_ports
import sys


DEFAULT_BAUD = 115200

# ---------------------------------------------------------------------------
# Host transfer protocol
# ---------------------------------------------------------------------------

HOST_START = b"\xD3\x91\x7E\x42"

# FPGA host_header_t:
#
#   4 bytes  magic
#   1 byte   type
#   2 bytes  filename_len
#   8 bytes  file_size
#
# Total = 15 bytes

HEADER_FORMAT = "<4sBHQ"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

MAGIC = b"FPGA"


# ---------------------------------------------------------------------------
# File types
# ---------------------------------------------------------------------------

TYPE_TEXT  = 1
TYPE_IMAGE = 2
TYPE_PDF   = 3
TYPE_ZIP   = 4
TYPE_BIN   = 5


# ---------------------------------------------------------------------------
# Serial port
# ---------------------------------------------------------------------------

def list_serial_ports():
    return list(serial.tools.list_ports.comports())


def select_port():

    ports = list_serial_ports()

    if not ports:
        print("No serial ports found.")
        return None

    print("\nAvailable Serial Ports")
    print("----------------------")

    for i, port in enumerate(ports, start=1):
        print(
            f"{i}. {port.device} "
            f"({port.description})"
        )

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
            timeout=0.1
        )

        print(f"\nConnected to {port_name}")

        return ser

    except serial.SerialException as e:

        print("\nFailed to open port:")
        print(e)

        return None


# ---------------------------------------------------------------------------
# Terminal output
# ---------------------------------------------------------------------------

def print_terminal_byte(b):
    """
    Print one normal UART0 byte.

    This is intended to behave similarly to a terminal:
    normal ASCII text is displayed directly.
    """

    if b == 0x0D:
        # CR
        sys.stdout.write("\r")

    elif b == 0x0A:
        # LF
        sys.stdout.write("\n")

    elif 0x20 <= b <= 0x7E:
        # Printable ASCII
        sys.stdout.write(chr(b))

    elif b == 0x09:
        # TAB
        sys.stdout.write("\t")

    else:
        # For non-printable bytes outside a file transfer,
        # show them rather than silently losing them.
        sys.stdout.write(
            f"<0x{b:02X}>"
        )

    sys.stdout.flush()


# ---------------------------------------------------------------------------
# File helpers
# ---------------------------------------------------------------------------

def read_exact(ser, size):
    """
    Read exactly 'size' bytes.
    """

    data = bytearray()

    while len(data) < size:

        try:
            chunk = ser.read(size - len(data))

        except serial.SerialException as e:
            raise RuntimeError(
                f"Serial connection error: {e}"
            )

        if chunk:
            data.extend(chunk)

    return bytes(data)


def filename_for_type(file_type):

    if file_type == TYPE_TEXT:
        return "received_msg.txt"

    if file_type == TYPE_IMAGE:
        return "received_img.png"

    if file_type == TYPE_PDF:
        return "received_file.pdf"

    if file_type == TYPE_ZIP:
        return "received_file.zip"

    if file_type == TYPE_BIN:
        return "received_file.bin"

    return "received_file.bin"


# ---------------------------------------------------------------------------
# Host transfer
# ---------------------------------------------------------------------------

# def receive_file_after_marker(ser):
#     """
#     HOST_START has already been consumed.

#     Read:
#         host_header_t
#         file payload

#     Then save the payload.
#     """

#     print("\n\n[FPGA FILE TRANSFER START]")

#     # ---------------------------------------------------------------
#     # Header
#     # ---------------------------------------------------------------

#     header = read_exact(
#         ser,
#         HEADER_SIZE
#     )

#     magic, file_type, filename_len, file_size = struct.unpack(
#         HEADER_FORMAT,
#         header
#     )

#     if magic != MAGIC:
#         raise RuntimeError(
#             f"Invalid header magic: {magic!r}"
#         )

#     print(f"File type : {file_type}")
#     print(f"File size : {file_size} bytes")

#     # ---------------------------------------------------------------
#     # Sanity check
#     # ---------------------------------------------------------------

#     MAX_FILE_SIZE = 100 * 1024 * 1024

#     if file_size > MAX_FILE_SIZE:
#         raise RuntimeError(
#             f"Invalid file size: {file_size} bytes"
#         )

#     # ---------------------------------------------------------------
#     # Filename
#     #
#     # Current FPGA send_host_file() sets:
#     #
#     #     filename_len = 0
#     #
#     # so there is currently no filename transmitted.
#     # ---------------------------------------------------------------

#     if filename_len != 0:
#         filename = read_exact(
#             ser,
#             filename_len
#         )

#         print(
#             f"Filename   : {filename.decode(errors='replace')}"
#         )

#     # ---------------------------------------------------------------
#     # File payload
#     # ---------------------------------------------------------------

#     print(
#         f"Receiving {file_size} bytes..."
#     )

#     data = read_exact(
#         ser,
#         file_size
#     )

#     # ---------------------------------------------------------------
#     # Save
#     # ---------------------------------------------------------------

#     out_name = filename_for_type(file_type)

#     with open(out_name, "wb") as f:
#         f.write(data)

#     print(
#         f"Saved as: {out_name}"
#     )

#     print(
#         f"Received: {len(data)} bytes"
#     )

#     print(
#         "[FPGA FILE TRANSFER COMPLETE]\n"
#     )

#     return out_name

def receive_file_after_marker(ser):

    print("\n[FPGA FILE TRANSFER START]")
    print("Waiting for header...")

    header = read_exact(
        ser,
        HEADER_SIZE
    )

    print("Header received:")
    print(" ".join(f"{b:02X}" for b in header))

    magic, file_type, filename_len, file_size = struct.unpack(
        HEADER_FORMAT,
        header
    )

    if magic != MAGIC:
        raise RuntimeError(
            f"Invalid header magic: {magic!r}"
        )

    print(f"File type    : {file_type}")
    print(f"Filename len : {filename_len}")
    print(f"File size    : {file_size} bytes")

    if file_size > 100 * 1024 * 1024:
        raise RuntimeError(
            f"Invalid file size: {file_size}"
        )

    if filename_len:
        filename = read_exact(
            ser,
            filename_len
        )

        print(
            f"Filename     : "
            f"{filename.decode(errors='replace')}"
        )

    print(
        f"Receiving {file_size} bytes..."
    )

    data = read_exact(
        ser,
        file_size
    )

    out_name = filename_for_type(file_type)

    with open(out_name, "wb") as f:
        f.write(data)

    print(f"Saved as: {out_name}")
    print(f"Received: {len(data)} bytes")

    print("[FPGA FILE TRANSFER COMPLETE]\n")

    return out_name

# ---------------------------------------------------------------------------
# Main UART loop
# ---------------------------------------------------------------------------

# def main():

#     port = select_port()

#     if port is None:
#         return

#     ser = open_serial(port)

#     if ser is None:
#         return

#     print("\nReceiver started.")
#     print("Displaying FPGA UART0 terminal output.")
#     print(
#         "Watching for FPGA transfer marker "
#         "(D3 91 7E 42)..."
#     )
#     print()

#     # Bytes currently being checked for HOST_START.
#     marker_window = bytearray()

#     try:

#         while True:

#             try:
#                 data = ser.read(256)

#             except serial.SerialException as e:

#                 print(
#                     f"\nSerial connection error: {e}"
#                 )

#                 break

#             if not data:
#                 continue

#             for b in data:

#                 # ---------------------------------------------------
#                 # Look for HOST_START while displaying terminal data
#                 # ---------------------------------------------------

#                 marker_window.append(b)

#                 if len(marker_window) > len(HOST_START):
#                     marker_window.pop(0)

#                 # ---------------------------------------------------
#                 # Marker detected
#                 # ---------------------------------------------------

#                 if bytes(marker_window) == HOST_START:

#                     # Remove marker from terminal output.
#                     marker_window.clear()

#                     try:

#                         receive_file_after_marker(ser)

#                     except RuntimeError as e:

#                         print(
#                             f"\nFile transfer error: {e}"
#                         )

#                         print(
#                             "Returning to terminal mode..."
#                         )

#                     break

#                 # ---------------------------------------------------
#                 # Normal FPGA terminal output
#                 # ---------------------------------------------------

#                 # We delay printing the last few bytes because they
#                 # may turn out to be the beginning of HOST_START.
#                 #
#                 # To keep this simple and avoid printing the marker,
#                 # print the oldest byte once the window is full.
#                 # ---------------------------------------------------

#                 if len(marker_window) == len(HOST_START):

#                     old_byte = marker_window[0]

#                     # Only print it if the current window isn't
#                     # actually the marker.
#                     if bytes(marker_window) != HOST_START:
#                         print_terminal_byte(old_byte)

#                     marker_window.pop(0)

#     except KeyboardInterrupt:

#         print("\nStopped.")

#     finally:

#         ser.close()

def main():

    port = select_port()

    if port is None:
        return

    ser = open_serial(port)

    if ser is None:
        return

    print("\nReceiver started.")
    print("Displaying FPGA UART0 terminal output.")
    print(
        "Watching for FPGA transfer marker "
        "(D3 91 7E 42)..."
    )
    print()

    marker_window = bytearray()

    try:

        while True:

            # Read exactly ONE byte.
            b = ser.read(1)

            if not b:
                continue

            value = b[0]

            # -------------------------------------------------------
            # Add byte to marker detection window
            # -------------------------------------------------------

            marker_window.append(value)

            if len(marker_window) > len(HOST_START):
                old = marker_window.pop(0)

                # The oldest byte cannot be part of a future
                # marker anymore, so display it as terminal output.
                print_terminal_byte(old)

            # -------------------------------------------------------
            # Full marker detected
            # -------------------------------------------------------

            if bytes(marker_window) == HOST_START:

                # Do not print the marker as terminal output.
                marker_window.clear()

                try:

                    receive_file_after_marker(ser)

                except RuntimeError as e:

                    print(
                        f"\nFile transfer error: {e}"
                    )

                    print(
                        "Returning to terminal mode..."
                    )

    except KeyboardInterrupt:

        print("\nStopped.")

    finally:

        ser.close()

if __name__ == "__main__":
    main()