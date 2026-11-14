"""Проверяет исходные сигнатуры узкого плагина в EXE, не запуская игру."""
from pathlib import Path
import struct


def validate(executable: Path) -> int:
    data = executable.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    sections = pe + 24 + optional_size
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]

    def read(rva: int, size: int) -> bytes:
        for index in range(count):
            _, address, raw_size, raw = struct.unpack_from("<IIII", data, sections + index * 40 + 8)
            if address <= rva and rva + size <= address + raw_size:
                return data[raw + rva - address:raw + rva - address + size]
        raise ValueError(f"RVA вне секций: {rva:x}")

    sites = {
        0x4B33E0: bytes.fromhex("55 8B EC 83 E4 F8"),
        0x4A79FA: bytes.fromhex("0F 84 BE 00 00 00"),
        0x4A7BC3: bytes.fromhex("0F 84 BD 00 00 00"),
    }
    for vtable, slot, target in (
        (0xA307E8, 6 * 4, 0x4CCB90),
        (0xA307E8, 10 * 4, 0x4CCBF0),
        (0xA2ABA8, 0x24, 0x4814E0),
        (0xA2ABE0, 0x24, 0x481B40),
        (0xA2AC18, 0x24, 0x4822C0),
        (0xA2AC50, 0x24, 0x4826B0),
        (0xA30858, 0x28, 0x4CD930),
        (0xA30890, 0x28, 0x4CE000),
    ):
        sites[vtable + slot] = struct.pack("<I", base + target)
    for rva, expected in sites.items():
        actual = read(rva, len(expected))
        if actual != expected:
            raise ValueError(f"Несовпадение RVA {rva:x}: {actual.hex()} вместо {expected.hex()}")
    return len(sites)


if __name__ == "__main__":
    import sys
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "v2game.exe"
    print(f"Подтверждены исходные сигнатуры: {validate(executable)}")
