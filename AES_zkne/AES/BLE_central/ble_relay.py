# #!/usr/bin/env python3

# import asyncio
# import logging
# from bleak import BleakClient, BleakScanner

# # Nordic UART Service UUIDs
# UART_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
# UART_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
# UART_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

# logging.basicConfig(
#     level=logging.INFO,
#     format="%(asctime)s | %(message)s",
#     datefmt="%H:%M:%S",
# )

# a_to_b_queue = asyncio.Queue()
# b_to_a_queue = asyncio.Queue()

# a_rx_buffer = bytearray()
# b_rx_buffer = bytearray()

# # Total bytes successfully forwarded in each direction (used for
# # diagnostics only).
# stats = {
#     "A->B": 0,
#     "B->A": 0,
# }

# # Conservative default ATT MTU to fall back on if a client hasn't
# # reported a negotiated MTU yet (avoids a zero/negative chunk size).
# DEFAULT_MTU = 23

# # UART baud rate used on the FPGA side of each BLE-UART bridge. Used to
# # pace forwarded writes so the bridge isn't handed bytes faster than it
# # can actually clock them out over the wire at this baud rate.
# UART_BAUD_RATE = 115200
# SECONDS_PER_BYTE = 15 / UART_BAUD_RATE

# MAX_WRITE_RETRIES = 3
# RETRY_BACKOFF_SECONDS = 0.05

# clientA = None
# clientB = None


# async def scan_devices():
#     print("\nScanning for BLE devices...\n")

#     devices = await BleakScanner.discover(timeout=5)

#     devices = [d for d in devices if d.name is not None]

#     if len(devices) < 2:
#         raise RuntimeError("Less than two BLE devices found.")

#     for i, d in enumerate(devices):
#         print(f"{i:2d} : {d.name:25} {d.address}")

#     return devices


# async def get_devices():

#     while True:

#         devices = await scan_devices()

#         print("\nOptions:")
#         print("  r : Scan again")
#         print("  q : Quit\n")

#         a = input("Select FPGA A: ").strip().lower()

#         if a == "r":
#             continue

#         if a == "q":
#             raise SystemExit

#         b = input("Select FPGA B: ").strip().lower()

#         if b == "r":
#             continue

#         if b == "q":
#             raise SystemExit

#         try:

#             a = int(a)
#             b = int(b)

#             if a == b:
#                 print("\nPlease select two different devices.\n")
#                 continue

#             return devices[a], devices[b]

#         except (ValueError, IndexError):

#             print("\nInvalid selection.\n")


# async def connect(device):

#     client = BleakClient(device)

#     await client.connect()

#     logging.info(f"Connected -> {device.name}")

#     return client


# def format_data(data):
#     result = ""

#     for b in data:

#         if b == 0x0D:
#             result += "\\r"

#         elif b == 0x0A:
#             result += "\\n"

#         elif b == 0x09:
#             result += "\\t"

#         elif b == 0x7F:
#             result += "<DEL>"

#         elif 32 <= b <= 126:
#             result += chr(b)

#         else:
#             result += f"\\x{b:02X}"

#     return result


# # async def callback_A(sender, data):
# #     print("\nRAW CALLBACK A")
# #     print("Sender :", sender)
# #     print("Length :", len(data))
# #     print("Data   :", data.hex(" "))
# #     await a_to_b_queue.put(bytes(data))
# async def callback_A(sender, data):

#     global a_rx_buffer

#     print("\nRAW CALLBACK A")
#     print("Sender :", sender)
#     print("Length :", len(data))
#     print("Data   :", data.hex(" "))

#     a_rx_buffer.extend(data)

#     while True:

#         try:
#             end = a_rx_buffer.index(0x0A)
#         except ValueError:
#             break

#         packet = bytes(a_rx_buffer[:end + 1])
#         del a_rx_buffer[:end + 1]

#         await a_to_b_queue.put(packet)


# # async def callback_B(sender, data):
# #     print("\nRAW CALLBACK B")
# #     print("Sender :", sender)
# #     print("Length :", len(data))
# #     print("Data   :", data.hex(" "))

# #     await b_to_a_queue.put(bytes(data))

# async def callback_B(sender, data):

#     global b_rx_buffer

#     print("\nRAW CALLBACK B")
#     print("Sender :", sender)
#     print("Length :", len(data))
#     print("Data   :", data.hex(" "))

#     b_rx_buffer.extend(data)

#     while True:

#         try:
#             end = b_rx_buffer.index(0x0A)
#         except ValueError:
#             break

#         packet = bytes(b_rx_buffer[:end + 1])
#         del b_rx_buffer[:end + 1]

#         await b_to_a_queue.put(packet)


# async def write_chunk_with_retry(client, chunk):
#     """
#     Write a single chunk, retrying a bounded number of times on
#     failure instead of silently dropping it and moving on.
#     Returns True if the chunk was written, False if all retries
#     were exhausted.
#     """
#     last_exc = None

#     for attempt in range(1, MAX_WRITE_RETRIES + 1):
#         try:
#             await client.write_gatt_char(UART_RX_UUID, chunk, response=True)
#             return True

#         except Exception as e:
#             last_exc = e
#             logging.warning(
#                 f"Write attempt {attempt}/{MAX_WRITE_RETRIES} failed "
#                 f"for a {len(chunk)}-byte chunk: {e}"
#             )
#             await asyncio.sleep(RETRY_BACKOFF_SECONDS * attempt)

#     logging.error(f"Giving up on a {len(chunk)}-byte chunk: {last_exc}")
#     return False


# async def writer(queue, direction):

#     global clientA
#     global clientB

#     while True:

#         data = await queue.get()

#         destination_client = clientB if direction == "A->B" else clientA

#         if destination_client is None or not destination_client.is_connected:
#             logging.warning(f"{direction}: Destination not connected.")
#             continue

#         # Fall back to the conservative default ATT MTU if either side
#         # hasn't reported a negotiated MTU yet, so this never ends up
#         # computing a zero or negative chunk size.
#         mtu_a = getattr(clientA, "mtu_size", None) or DEFAULT_MTU
#         mtu_b = getattr(clientB, "mtu_size", None) or DEFAULT_MTU
#         max_chunk = max(min(mtu_a, mtu_b) - 3, 1)

#         bytes_sent = 0
#         complete = True

#         # for i in range(0, len(data), max_chunk):

#         #     chunk = data[i:i + max_chunk]

#         #     if not await write_chunk_with_retry(destination_client, chunk):
#         #         # Retries exhausted on this chunk. Stop forwarding the
#         #         # rest of this notification -- the bytes after this
#         #         # point can't be recovered here, but we now know
#         #         # exactly how much got through instead of guessing.
#         #         complete = False
#         #         break

#         #     bytes_sent += len(chunk)

#         #     # Pace writes so the receiving BLE-UART bridge has time to
#         #     # actually clock bytes out at the FPGA's baud rate, rather
#         #     # than being handed data faster than it can drain its
#         #     # internal buffer.
#         #     await asyncio.sleep(len(chunk) * SECONDS_PER_BYTE)

#         success = await write_chunk_with_retry(destination_client, data)

#         if success:
#             bytes_sent = len(data)
#         else:
#             bytes_sent = 0
#         stats[direction] += bytes_sent

#         print("\n===================================================")
#         print(direction)
#         print(f"Notification length   : {len(data)} bytes")
#         print(f"Data                   : {format_data(data)}")
#         print(f"Bytes forwarded        : {bytes_sent}/{len(data)}")
#         print(f"Total bytes {direction:5} : {stats[direction]}")
#         status = "Forwarded" if complete else "INCOMPLETE (see warnings above)"
#         print(f"Status                 : {status}")
#         print("===================================================\n")
        
#         await asyncio.sleep(0.05)   # 50 ms

# async def monitor_connections(deviceA, deviceB):

#     global clientA
#     global clientB

#     while True:

#         if not clientA.is_connected:

#             logging.warning("Reconnecting Board A...")

#             while True:
#                 try:
#                     clientA = await connect(deviceA)
#                     await clientA.start_notify(UART_TX_UUID, callback_A)
#                     break
#                 except Exception:
#                     await asyncio.sleep(2)

#         if not clientB.is_connected:

#             logging.warning("Reconnecting Board B...")

#             while True:
#                 try:
#                     clientB = await connect(deviceB)
#                     await clientB.start_notify(UART_TX_UUID, callback_B)
#                     break
#                 except Exception:
#                     await asyncio.sleep(2)

#         await asyncio.sleep(2)


# async def main():

#     global clientA
#     global clientB

#     deviceA, deviceB = await get_devices()

#     print(f"\nFPGA A : {deviceA.name}")
#     print(f"FPGA B : {deviceB.name}\n")

#     clientA = await connect(deviceA)
#     clientB = await connect(deviceB)

#     await clientA.start_notify(UART_TX_UUID, callback_A)
#     await clientB.start_notify(UART_TX_UUID, callback_B)

#     logging.info("Relay started")
#     logging.info("---------------------------------------")

#     await asyncio.gather(
#         writer(a_to_b_queue, "A->B"),
#         writer(b_to_a_queue, "B->A"),
#         monitor_connections(deviceA, deviceB),
#     )

# if __name__ == "__main__":

#     asyncio.run(main())


#!/usr/bin/env python3

import asyncio
import logging
from bleak import BleakClient, BleakScanner

# Nordic UART Service UUIDs
UART_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
UART_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
UART_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s | %(message)s",
    datefmt="%H:%M:%S",
)

a_to_b_queue = asyncio.Queue()
b_to_a_queue = asyncio.Queue()

a_rx_buffer = bytearray()
b_rx_buffer = bytearray()

# Total bytes successfully forwarded in each direction (used for
# diagnostics only).
stats = {
    "A->B": 0,
    "B->A": 0,
}

# Conservative default ATT MTU to fall back on if a client hasn't
# reported a negotiated MTU yet (avoids a zero/negative chunk size).
DEFAULT_MTU = 23

# UART baud rate used on the FPGA side of each BLE-UART bridge. Used to
# pace forwarded writes so the bridge isn't handed bytes faster than it
# can actually clock them out over the wire at this baud rate.
UART_BAUD_RATE = 115200
SECONDS_PER_BYTE = 45 / UART_BAUD_RATE

MAX_WRITE_RETRIES = 3
RETRY_BACKOFF_SECONDS = 0.05

clientA = None
clientB = None


async def scan_devices():
    print("\nScanning for BLE devices...\n")

    devices = await BleakScanner.discover(timeout=5)

    devices = [d for d in devices if d.name is not None]

    if len(devices) < 2:
        raise RuntimeError("Less than two BLE devices found.")

    for i, d in enumerate(devices):
        print(f"{i:2d} : {d.name:25} {d.address}")

    return devices


async def get_devices():

    while True:

        devices = await scan_devices()

        print("\nOptions:")
        print("  r : Scan again")
        print("  q : Quit\n")

        a = input("Select FPGA A: ").strip().lower()

        if a == "r":
            continue

        if a == "q":
            raise SystemExit

        b = input("Select FPGA B: ").strip().lower()

        if b == "r":
            continue

        if b == "q":
            raise SystemExit

        try:

            a = int(a)
            b = int(b)

            if a == b:
                print("\nPlease select two different devices.\n")
                continue

            return devices[a], devices[b]

        except (ValueError, IndexError):

            print("\nInvalid selection.\n")


async def connect(device):

    client = BleakClient(device)

    await client.connect()

    logging.info(f"Connected -> {device.name}")

    return client


def format_data(data):
    result = ""

    for b in data:

        if b == 0x0D:
            result += "\\r"

        elif b == 0x0A:
            result += "\\n"

        elif b == 0x09:
            result += "\\t"

        elif b == 0x7F:
            result += "<DEL>"

        elif 32 <= b <= 126:
            result += chr(b)

        else:
            result += f"\\x{b:02X}"

    return result


async def callback_A(sender, data):

    global a_rx_buffer

    print("\nRAW CALLBACK A")
    print("Sender :", sender)
    print("Length :", len(data))
    print("Data   :", data.hex(" "))

    a_rx_buffer.extend(data)

    while True:

        try:
            end = a_rx_buffer.index(0x0A)
        except ValueError:
            break

        packet = bytes(a_rx_buffer[:end + 1])
        del a_rx_buffer[:end + 1]

        await a_to_b_queue.put(packet)


async def callback_B(sender, data):

    global b_rx_buffer

    print("\nRAW CALLBACK B")
    print("Sender :", sender)
    print("Length :", len(data))
    print("Data   :", data.hex(" "))

    b_rx_buffer.extend(data)

    while True:

        try:
            end = b_rx_buffer.index(0x0A)
        except ValueError:
            break

        packet = bytes(b_rx_buffer[:end + 1])
        del b_rx_buffer[:end + 1]

        await b_to_a_queue.put(packet)


async def write_chunk_with_retry(client, chunk):
    """
    Write a single chunk, retrying a bounded number of times on
    failure instead of silently dropping it and moving on.

    This is only safe to retry blindly because the caller guarantees
    `chunk` is <= (negotiated MTU - 3) bytes: a write that size is a
    single atomic ATT Write Request/Response, not a multi-PDU "long
    write". That means an exception here is a trustworthy signal that
    the chunk was NOT applied, so resending it can't silently duplicate
    data on the peripheral. Do not call this with a chunk bigger than
    the MTU allows -- that reintroduces exactly that ambiguity.

    Returns True if the chunk was written, False if all retries
    were exhausted.
    """
    last_exc = None

    for attempt in range(1, MAX_WRITE_RETRIES + 1):
        try:
            await client.write_gatt_char(UART_RX_UUID, chunk, response=True)
            return True

        except Exception as e:
            last_exc = e
            logging.warning(
                f"Write attempt {attempt}/{MAX_WRITE_RETRIES} failed "
                f"for a {len(chunk)}-byte chunk: {e}"
            )
            await asyncio.sleep(RETRY_BACKOFF_SECONDS * attempt)

    logging.error(f"Giving up on a {len(chunk)}-byte chunk: {last_exc}")
    return False


async def writer(queue, direction):

    global clientA
    global clientB

    while True:

        data = await queue.get()

        destination_client = clientB if direction == "A->B" else clientA

        if destination_client is None or not destination_client.is_connected:
            logging.warning(f"{direction}: Destination not connected.")
            continue

        # Fall back to the conservative default ATT MTU if either side
        # hasn't reported a negotiated MTU yet, so this never ends up
        # computing a zero or negative chunk size.
        mtu_a = getattr(clientA, "mtu_size", None) or DEFAULT_MTU
        mtu_b = getattr(clientB, "mtu_size", None) or DEFAULT_MTU
        max_chunk = max(min(mtu_a, mtu_b) - 3, 1)

        bytes_sent = 0
        complete = True

        # IMPORTANT: keep every write <= max_chunk. A write above the
        # negotiated MTU forces a multi-step "long write" (Prepare Write
        # x N + Execute Write) instead of one atomic Write
        # Request/Response. If the final confirmation of a long write
        # times out on the client side even though the peripheral
        # already applied it, the retry below resends the same bytes
        # and the peripheral applies them a second time -- i.e. exactly
        # the duplicate-packet symptom this fixes.
        for i in range(0, len(data), max_chunk):

            chunk = data[i:i + max_chunk]

            if not await write_chunk_with_retry(destination_client, chunk):
                # Retries exhausted on this chunk. Stop forwarding the
                # rest of this packet -- the bytes after this point
                # can't be recovered here, but we now know exactly how
                # much got through instead of guessing.
                complete = False
                break

            bytes_sent += len(chunk)

            # Pace writes so the receiving BLE-UART bridge has time to
            # actually clock bytes out at the FPGA's baud rate, rather
            # than being handed data faster than it can drain its
            # internal buffer.
            await asyncio.sleep(len(chunk) * SECONDS_PER_BYTE)

        stats[direction] += bytes_sent

        print("\n===================================================")
        print(direction)
        print(f"Notification length   : {len(data)} bytes")
        print(f"Data                   : {format_data(data)}")
        print(f"Bytes forwarded        : {bytes_sent}/{len(data)}")
        print(f"Total bytes {direction:5} : {stats[direction]}")
        status = "Forwarded" if complete else "INCOMPLETE (see warnings above)"
        print(f"Status                 : {status}")
        print("===================================================\n")

        await asyncio.sleep(0.05)   # 50 ms


async def safe_teardown(client):
    """
    Best-effort cleanup of a client we're about to replace during a
    reconnect. Without this, a stale client object can be left with its
    notification subscription still active (e.g. if is_connected briefly
    read False without the underlying link/subscription actually being
    torn down). If that happens, the OLD and NEW subscriptions both stay
    live, and every real over-the-air notification fires both callbacks
    -- delivering the same packet to the queue more than once. This is
    the direct cause of packets being forwarded multiple times when a
    reconnect has occurred.
    """
    if client is None:
        return

    try:
        await client.stop_notify(UART_TX_UUID)
    except Exception:
        pass

    try:
        await client.disconnect()
    except Exception:
        pass


async def monitor_connections(deviceA, deviceB):

    global clientA
    global clientB

    while True:

        if not clientA.is_connected:

            logging.warning("Reconnecting Board A...")

            stale = clientA
            clientA = None  # writer() sees None and skips A->B until reconnected
            await safe_teardown(stale)

            while True:
                try:
                    clientA = await connect(deviceA)
                    await clientA.start_notify(UART_TX_UUID, callback_A)
                    break
                except Exception:
                    await asyncio.sleep(2)

        if not clientB.is_connected:

            logging.warning("Reconnecting Board B...")

            stale = clientB
            clientB = None  # writer() sees None and skips B->A until reconnected
            await safe_teardown(stale)

            while True:
                try:
                    clientB = await connect(deviceB)
                    await clientB.start_notify(UART_TX_UUID, callback_B)
                    break
                except Exception:
                    await asyncio.sleep(2)

        await asyncio.sleep(2)


async def main():

    global clientA
    global clientB

    deviceA, deviceB = await get_devices()

    print(f"\nFPGA A : {deviceA.name}")
    print(f"FPGA B : {deviceB.name}\n")

    clientA = await connect(deviceA)
    clientB = await connect(deviceB)

    await clientA.start_notify(UART_TX_UUID, callback_A)
    await clientB.start_notify(UART_TX_UUID, callback_B)

    logging.info("Relay started")
    logging.info("---------------------------------------")

    await asyncio.gather(
        writer(a_to_b_queue, "A->B"),
        writer(b_to_a_queue, "B->A"),
        monitor_connections(deviceA, deviceB),
    )

if __name__ == "__main__":

    asyncio.run(main())