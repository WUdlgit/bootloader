import struct
import sys
from pathlib import Path
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import upgrade


def image_of(length):
    image = bytearray((i % 256 for i in range(length)))
    struct.pack_into("<II", image, 0, 0x20020000, upgrade.APP_ADDRESS + 9)
    return bytes(image)


class FakePort:
    """独立模拟设备应答，上一条响应尚未读完时禁止继续发送。"""

    def __init__(self, image, failure=None):
        self.image = image
        self.failure = failure
        self.pending = bytearray()
        self.state = "idle"
        self.completed = 0
        self.blocks = []
        self.booted = False
        self.writes = []

    def reset_input_buffer(self):
        self.pending.clear()

    def reply(self, text):
        self.pending.extend((text + "\r\n").encode("ascii"))

    def read(self, count):
        data = self.pending[:count]
        del self.pending[:count]
        return bytes(data)

    def write(self, data):
        if self.pending:
            raise AssertionError("Sender wrote before consuming the device response")
        self.writes.append(data)
        if self.state == "idle":
            if data == b"?":
                self.reply("[BOOT] READY")
            elif data == b"u":
                self.reply("[BOOT] HEADER")
                self.state = "header"
            else:
                raise AssertionError(data)
        elif self.state == "header":
            expected = f"SIZE={len(self.image)} CRC={zlib.crc32(self.image):08X}\n".encode()
            if data != expected:
                raise AssertionError(data)
            if self.failure == "erase":
                self.reply("[BOOT] ERROR erase")
            else:
                self.reply(f"[BOOT] DATA {len(self.image)} 1024")
                self.state = "body"
        elif self.state == "body":
            expected_length = min(1024, len(self.image) - self.completed)
            if len(data) != expected_length or data != self.image[self.completed:self.completed + len(data)]:
                raise AssertionError("Wrong payload or tail length")
            self.blocks.append(data)
            if self.failure == "write":
                self.reply("[BOOT] ERROR write 0")
            else:
                self.completed += len(data)
                self.reply(f"[BOOT] ACK {self.completed}")
                if self.completed == len(self.image):
                    if self.failure == "crc":
                        self.reply("[BOOT] ERROR verify_or_commit")
                    else:
                        self.reply(f"[BOOT] DONE {len(self.image)} {zlib.crc32(self.image):08X}")
                        self.state = "done"
        elif self.state == "done" and data == b"g":
            self.booted = True
            self.reply("[BOOT] BOOTING")
        else:
            raise AssertionError(data)
        return len(data)


class UpgradeTests(unittest.TestCase):
    def test_multiblock_tail_and_sector_boundary(self):
        for length in (1023, 1024, 1025, 2049, 65535, 65536, 65537, 196607, 196608, 196609, 327679, 327680, 327681, 458752):
            with self.subTest(length=length):
                image = image_of(length)
                port = FakePort(image)
                upgrade.upgrade(port, image, boot=True, progress=lambda text: None)
                self.assertEqual(b"".join(port.blocks), image)
                self.assertTrue(port.booted)

    def test_failure_stops_without_boot_or_next_block(self):
        for failure in ("erase", "write", "crc"):
            with self.subTest(failure=failure):
                image = image_of(2049)
                port = FakePort(image, failure)
                with self.assertRaises(RuntimeError):
                    upgrade.upgrade(port, image, boot=True, progress=lambda text: None)
                self.assertFalse(port.booted)
                self.assertEqual(len(port.blocks), {"erase": 0, "write": 1, "crc": 3}[failure])

    def test_invalid_image_sends_nothing(self):
        images = [b"", b"x" * 7, b"x" * 458753, b"x" * 1024]
        for stack, entry in ((0x20000000, 0x08010009), (0x20020004, 0x08010009),
                             (0x20020000, 0x08010008), (0x20020000, 0x08040401)):
            image = bytearray(image_of(1024))
            struct.pack_into("<II", image, 0, stack, entry)
            images.append(bytes(image))
        for image in images:
            port = FakePort(image)
            with self.assertRaises(ValueError):
                upgrade.upgrade(port, image)
            self.assertEqual(port.writes, [])

    def test_response_timeout(self):
        class EmptyPort:
            def read(self, count):
                return b""
        with self.assertRaises(TimeoutError):
            upgrade.read_response(EmptyPort(), "[BOOT] READY", 0.001)

    def test_short_write_is_not_replayed(self):
        class ShortPort:
            def write(self, data):
                return len(data) - 1
        with self.assertRaises(RuntimeError):
            upgrade.write_data(ShortPort(), b"payload")


if __name__ == "__main__":
    unittest.main()
