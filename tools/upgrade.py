"""通过 USART3 发送原始 App BIN；设备复位和 SWD 下载由调试流程负责。"""

import argparse
from pathlib import Path
import struct
import time
import zlib


APP_ADDRESS = 0x08010000
APP_MAX_LENGTH = 448 * 1024
BLOCK_LENGTH = 1024


def check_image(image):
    if not 8 <= len(image) <= APP_MAX_LENGTH:
        raise ValueError(f"BIN length must be 8..{APP_MAX_LENGTH} bytes")
    stack, entry = struct.unpack_from("<II", image)
    if not 0x20000000 < stack <= 0x20020000 or stack % 8:
        raise ValueError("Initial stack must be aligned in the configured SRAM")
    if not entry & 1 or not APP_ADDRESS + 8 <= (entry & ~1) <= APP_ADDRESS + len(image) - 2:
        raise ValueError("Reset vector must be a Thumb entry inside this App BIN")
    return zlib.crc32(image)


def write_data(port, data):
    if port.write(data) != len(data):
        raise RuntimeError("Serial write was incomplete; restart the upgrade")


def read_response(port, expected, timeout, allow_error=False):
    deadline = time.monotonic() + timeout
    line = bytearray()
    while time.monotonic() < deadline:
        data = port.read(1)
        if not data:
            continue
        line.extend(data)
        if len(line) > 256:
            raise RuntimeError("Serial response is too long; check the upgrade port")
        if data != b"\n":
            continue
        message = line.decode("ascii", errors="replace").strip()
        line.clear()
        if message == expected:
            return
        if message.startswith("[BOOT] ERROR") and not allow_error:
            raise RuntimeError(message)
    raise TimeoutError(f"Timed out waiting for {expected!r}")


def upgrade(port, image, boot=False, progress=print):
    crc = check_image(image)  # 先检查镜像，再发送会触发擦除的命令。
    port.reset_input_buffer()
    write_data(port, b"?")
    # 上一次接收中断后，设备可能先等待超时，再重新返回 READY。
    read_response(port, "[BOOT] READY", 10, allow_error=True)

    start = time.monotonic()
    write_data(port, b"u")
    read_response(port, "[BOOT] HEADER", 5)
    write_data(port, f"SIZE={len(image)} CRC={crc:08X}\n".encode("ascii"))
    # 描述头和最多两个 App 扇区的擦除使用单独的等待时间。
    read_response(port, f"[BOOT] DATA {len(image)} {BLOCK_LENGTH}", 30)

    for offset in range(0, len(image), BLOCK_LENGTH):
        block = image[offset:offset + BLOCK_LENGTH]
        write_data(port, block)
        completed = offset + len(block)
        read_response(port, f"[BOOT] ACK {completed}", 5)
        progress(f"{completed}/{len(image)} bytes written and read back")

    read_response(port, f"[BOOT] DONE {len(image)} {crc:08X}", 10)
    elapsed = time.monotonic() - start
    if elapsed > 0:
        progress(f"Verified {len(image)} bytes, CRC32={crc:08X}, {elapsed:.3f} s, "
                 f"{len(image) / elapsed:.1f} bytes/s (erase + transfer + verification)")
    else:
        progress(f"Verified {len(image)} bytes, CRC32={crc:08X}, elapsed time below clock resolution")
    if boot:
        write_data(port, b"g")
        read_response(port, "[BOOT] BOOTING", 10)
        progress("Boot command accepted; confirm App version and LED on USART1/the board")
    return elapsed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="raw BIN linked at 0x08010000")
    parser.add_argument("--port", help="USART3 adapter, e.g. COM12; use the actual port")
    parser.add_argument("--boot", action="store_true", help="send g after verification")
    parser.add_argument("--check-only", action="store_true", help="inspect BIN without serial access")
    args = parser.parse_args()
    try:
        image = args.image.read_bytes()
        crc = check_image(image)
        if args.check_only:
            print(f"BIN valid: {len(image)} bytes, CRC32={crc:08X}")
            return 0
        if not args.port:
            parser.error("--port is required unless --check-only is used")
        try:
            import serial
        except ImportError:
            raise RuntimeError("Install sender dependency with: python -m pip install pyserial") from None
        with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=5) as port:
            upgrade(port, image, boot=args.boot)
        return 0
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(f"Upgrade stopped: {error}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
