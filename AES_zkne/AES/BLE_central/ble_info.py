import asyncio
from bleak import BleakScanner, BleakClient

async def main():

    print("Scanning...")

    devices = await BleakScanner.discover(timeout=5)

    for i, d in enumerate(devices):

        print(f"{i}: {d.name}  {d.address}")

    idx = int(input("Select device: "))

    device = devices[idx]

    async with BleakClient(device.address) as client:

        print("Connected")

        for service in client.services:

            print(f"\nSERVICE {service.uuid}")

            for ch in service.characteristics:

                print("  ", ch.uuid)
                print("     ", ch.properties)

asyncio.run(main())