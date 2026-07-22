#!/usr/bin/env python3
"""
Generator for IPMI FRU VPD binary test fixtures.

Each fixture is a byte-accurate IPMI Platform Management FRU Information
Storage Definition v1.0 Rev 1.3 image.  Checksums are computed automatically.

Run from the test/ directory:
    python3 gen_ipmi_fru_fixtures.py
"""

import os
import struct

OUT_DIR = os.path.join(os.path.dirname(__file__), "vpd_files")
os.makedirs(OUT_DIR, exist_ok=True)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def zero_checksum(data: bytes) -> int:
    """Return the 2's-complement zero-checksum byte for `data`."""
    return (-sum(data)) & 0xFF


def typelen(type_code: int, data: bytes) -> bytes:
    """Return a type/length byte followed by `data`."""
    assert type_code in (0, 1, 2, 3), f"bad type_code {type_code}"
    assert len(data) <= 63, "field too long"
    return bytes([(type_code << 6) | len(data)]) + data


def ascii8(text: str) -> bytes:
    """8-bit ASCII type/length + data."""
    return typelen(3, text.encode("ascii"))


def binary_field(data: bytes) -> bytes:
    """Binary type/length + data."""
    return typelen(0, data)


def pack6bit(text: str) -> bytes:
    """Encode text as 6-bit packed ASCII and return typelen + data bytes.

    Each character maps to value = ord(c) - 0x20 (0–63).
    Four 6-bit values packed into three bytes (LSb-first).
    The text is padded with spaces to a multiple of 4 chars.
    """
    # Pad to multiple of 4
    while len(text) % 4 != 0:
        text += " "
    data = bytearray()
    for i in range(0, len(text), 4):
        c0 = ord(text[i]) - 0x20
        c1 = ord(text[i + 1]) - 0x20
        c2 = ord(text[i + 2]) - 0x20
        c3 = ord(text[i + 3]) - 0x20
        bits = c0 | (c1 << 6) | (c2 << 12) | (c3 << 18)
        data += bytes([bits & 0xFF, (bits >> 8) & 0xFF, (bits >> 16) & 0xFF])
    return typelen(2, bytes(data))


def bcd_plus(digits: str) -> bytes:
    """Encode a digit string as BCD+ and return typelen + data."""
    MAP = "0123456789 -."
    # Use '.' for index 13 (last slot before 0xE/0xF which are unused)
    nibbles = []
    for ch in digits:
        idx = MAP.index(ch)
        nibbles.append(idx)
    if len(nibbles) % 2:
        nibbles.append(0xF)  # pad with unused nibble
    data = bytearray()
    for i in range(0, len(nibbles), 2):
        data.append((nibbles[i] << 4) | nibbles[i + 1])
    return typelen(1, bytes(data))


SENTINEL = b"\xc1"


def pad_area(data: bytes, unit: int = 8) -> bytes:
    """Pad `data` (which must not yet include checksum) to (N*unit - 1) bytes,
    then append the zero-checksum byte so total length is a multiple of unit.
    """
    needed = unit - (len(data) + 1) % unit
    if needed == unit:
        needed = 0
    data += b"\x00" * needed
    data += bytes([zero_checksum(data)])
    assert len(data) % unit == 0
    return data


def area_len_units(area_bytes: bytes, unit: int = 8) -> int:
    """Return area length in 8-byte units (area_bytes already includes checksum)."""
    assert len(area_bytes) % unit == 0
    return len(area_bytes) // unit


def build_common_header(offsets: dict) -> bytes:
    """
    offsets: dict with optional keys 'internal','chassis','board','product','multirecord'
    All values are byte offsets; converted to 8-byte units here.
    """

    def to_units(off):
        return (off // 8) if off else 0

    hdr = bytes(
        [
            0x01,  # format version
            to_units(offsets.get("internal", 0)),
            to_units(offsets.get("chassis", 0)),
            to_units(offsets.get("board", 0)),
            to_units(offsets.get("product", 0)),
            to_units(offsets.get("multirecord", 0)),
            0x00,  # PAD
        ]
    )
    hdr += bytes([zero_checksum(hdr)])
    assert len(hdr) == 8
    return hdr


def build_chassis_area(
    chassis_type: int, part: str, serial: str, customs=None
) -> bytes:
    body = bytes([0x01])  # format version
    body += bytes([0x00])  # area length (placeholder, filled by pad_area)
    body += bytes([chassis_type])  # chassis type
    body += ascii8(part)
    body += ascii8(serial)
    if customs:
        for c in customs:
            body += ascii8(c)
    body += SENTINEL
    # Patch area-length placeholder at index 1
    area = pad_area(body)
    area = bytes([area[0], area_len_units(area)]) + area[2:]
    # Recompute checksum after patching
    area = area[:-1] + bytes([zero_checksum(area[:-1])])
    return area


def build_board_area(
    lang: int,
    mfg_minutes: int,
    manufacturer: str,
    product: str,
    serial: str,
    part: str,
    fru_file_id: str,
    customs=None,
) -> bytes:
    mfg_bytes = struct.pack("<I", mfg_minutes)[:3]  # 3-byte LE
    body = bytes([0x01])  # format version
    body += bytes([0x00])  # area length placeholder
    body += bytes([lang])
    body += mfg_bytes
    body += ascii8(manufacturer)
    body += ascii8(product)
    body += ascii8(serial)
    body += ascii8(part)
    body += ascii8(fru_file_id)
    if customs:
        for c in customs:
            body += ascii8(c)
    body += SENTINEL
    area = pad_area(body)
    area = bytes([area[0], area_len_units(area)]) + area[2:]
    area = area[:-1] + bytes([zero_checksum(area[:-1])])
    return area


def build_product_area(
    lang: int,
    manufacturer: str,
    product_name: str,
    part_model: str,
    version: str,
    serial: str,
    asset_tag: str,
    fru_file_id: str,
    customs=None,
) -> bytes:
    body = bytes([0x01])
    body += bytes([0x00])
    body += bytes([lang])
    body += ascii8(manufacturer)
    body += ascii8(product_name)
    body += ascii8(part_model)
    body += ascii8(version)
    body += ascii8(serial)
    body += ascii8(asset_tag)
    body += ascii8(fru_file_id)
    if customs:
        for c in customs:
            body += ascii8(c)
    body += SENTINEL
    area = pad_area(body)
    area = bytes([area[0], area_len_units(area)]) + area[2:]
    area = area[:-1] + bytes([zero_checksum(area[:-1])])
    return area


def build_internal_area(payload: bytes) -> bytes:
    """Returns version byte + payload, NO own checksum (opaque blob)."""
    return bytes([0x01]) + payload


def build_multirecord(type_id: int, data: bytes, end_of_list: bool) -> bytes:
    rec_cs = zero_checksum(data) if data else 0x00
    flags = 0x02 | (0x80 if end_of_list else 0x00)
    hdr_raw = bytes([type_id, flags, len(data), rec_cs])
    hdr_cs = zero_checksum(hdr_raw)
    return hdr_raw + bytes([hdr_cs]) + data


def write(filename: str, data: bytes):
    path = os.path.join(OUT_DIR, filename)
    with open(path, "wb") as f:
        f.write(data)
    print(f"  wrote {path}  ({len(data)} bytes)")


def corrupt(data: bytes, index: int, new_byte: int) -> bytes:
    b = bytearray(data)
    b[index] = new_byte
    return bytes(b)


# ---------------------------------------------------------------------------
# Fixture 1: product_only — only Product Info Area present
# ---------------------------------------------------------------------------
def make_product_only():
    prod = build_product_area(
        lang=0,
        manufacturer="Samsung",
        product_name="PM9D3a NVMe",
        part_model="MZWLR7T6HALA",
        version="1.0",
        serial="S6ERNX0T123456",
        asset_tag="ASSET-001",
        fru_file_id="",
    )
    hdr = build_common_header({"product": 8})
    image = hdr + prod
    write("ipmi_fru_product_only.dat", image)
    return image


# ---------------------------------------------------------------------------
# Fixture 2: board_only — only Board Info Area present
# ---------------------------------------------------------------------------
def make_board_only():
    board = build_board_area(
        lang=0,
        mfg_minutes=0x0A4B2C,  # some timestamp
        manufacturer="Samsung",
        product="PM1753 E3.S",
        serial="S7A1NX0T654321",
        part="MZWLJ7T6HALA",
        fru_file_id="",
    )
    hdr = build_common_header({"board": 8})
    image = hdr + board
    write("ipmi_fru_board_only.dat", image)
    return image


# ---------------------------------------------------------------------------
# Fixture 3: all_areas — Internal + Chassis + Board + Product + MultiRecord
# ---------------------------------------------------------------------------
def make_all_areas():
    internal_payload = b"\xde\xad\xbe\xef\x01\x02\x03\x04\x05\x06\x07"
    internal = build_internal_area(
        internal_payload
    )  # 12 bytes (1 ver + 11 data)
    # Pad to 8-byte multiple
    while (len(internal) + 1) % 8 != 0:
        internal += b"\xff"
    # Internal area does NOT have its own checksum in our model; length is
    # derived from next area offset.  Pad to 16 bytes total.
    internal = internal + b"\x00" * (16 - len(internal))

    chassis = build_chassis_area(
        chassis_type=0x17,  # Rack Mount
        part="CHASSIS-PN-001",
        serial="CHASSIS-SN-001",
        customs=["CustomChassis1"],
    )
    board = build_board_area(
        lang=0,
        mfg_minutes=0x12345,
        manufacturer="IBM",
        product="Power10 NVMe Board",
        serial="IBMSN0001234",
        part="IBMPN0001234",
        fru_file_id="01",
        customs=["BoardCustom1", "BoardCustom2"],
    )
    prod = build_product_area(
        lang=0,
        manufacturer="IBM",
        product_name="NVMe E3.S Drive",
        part_model="PM9D3a-7T6",
        version="v2",
        serial="PRODSER123456",
        asset_tag="TAG-001",
        fru_file_id="02",
        customs=["ProdCustom1"],
    )
    mr = build_multirecord(0xC0, b"\x01\x02\x03\x04", end_of_list=False)
    mr += build_multirecord(0xC1, b"\xab\xcd", end_of_list=True)

    # Compute layout
    hdr_size = 8
    internal_off = hdr_size  # 8
    chassis_off = internal_off + len(internal)  # 8 + 16 = 24
    board_off = chassis_off + len(chassis)
    prod_off = board_off + len(board)
    mr_off = prod_off + len(prod)

    hdr = build_common_header(
        {
            "internal": internal_off,
            "chassis": chassis_off,
            "board": board_off,
            "product": prod_off,
            "multirecord": mr_off,
        }
    )
    image = hdr + internal + chassis + board + prod + mr
    write("ipmi_fru_all_areas.dat", image)
    return image


# ---------------------------------------------------------------------------
# Fixture 4: 6bit_ascii — Board area with 6-bit ASCII encoded fields
# ---------------------------------------------------------------------------
def make_6bit_ascii():
    # Build board area manually with 6-bit ASCII fields
    mfg_bytes = struct.pack("<I", 0)[:3]
    body = bytes([0x01, 0x00, 0x00]) + mfg_bytes
    body += pack6bit("SAMSUNG")  # manufacturer  (7 chars → pad to 8 → 6 bytes)
    body += pack6bit("PM9D3A")  # product name  (6 chars → pad to 8 → 6 bytes)
    body += ascii8("SN-001")
    body += ascii8("PN-001")
    body += SENTINEL
    area = pad_area(body)
    area = bytes([area[0], area_len_units(area)]) + area[2:]
    area = area[:-1] + bytes([zero_checksum(area[:-1])])

    hdr = build_common_header({"board": 8})
    image = hdr + area
    write("ipmi_fru_6bit_ascii.dat", image)
    return image


# ---------------------------------------------------------------------------
# Fixture 5: multirecord_only — only MultiRecord Area
# ---------------------------------------------------------------------------
def make_multirecord_only():
    mr = build_multirecord(0xC0, b"\x10\x20\x30", end_of_list=False)
    mr += build_multirecord(0xC1, b"\xaa\xbb\xcc\xdd", end_of_list=False)
    mr += build_multirecord(0xC2, b"", end_of_list=True)

    hdr = build_common_header({"multirecord": 8})
    image = hdr + mr
    write("ipmi_fru_multirecord_only.dat", image)
    return image


# ---------------------------------------------------------------------------
# Invalid fixture A: bad_hdr_checksum — Common Header checksum wrong
# ---------------------------------------------------------------------------
def make_bad_hdr_checksum():
    image = make_product_only()
    image = corrupt(image, 7, (image[7] ^ 0xFF) & 0xFF)
    write("ipmi_fru_bad_hdr_checksum.dat", image)


# ---------------------------------------------------------------------------
# Invalid fixture B: bad_format_version — Common Header format version != 1
# ---------------------------------------------------------------------------
def make_bad_format_version():
    # Build a valid product-only image then change byte[0] to version 0x02
    prod = build_product_area(
        lang=0,
        manufacturer="X",
        product_name="Y",
        part_model="Z",
        version="1",
        serial="S",
        asset_tag="A",
        fru_file_id="",
    )
    # Build header with wrong version
    hdr_raw = bytes(
        [
            0x02,  # wrong version
            0x00,  # no internal
            0x00,  # no chassis
            0x00,  # no board
            0x01,  # product at offset 8
            0x00,  # no multirecord
            0x00,
        ]
    )
    hdr_raw += bytes([zero_checksum(hdr_raw)])
    image = hdr_raw + prod
    write("ipmi_fru_bad_format_version.dat", image)


# ---------------------------------------------------------------------------
# Invalid fixture C: truncated — buffer smaller than 8 bytes
# ---------------------------------------------------------------------------
def make_truncated():
    write("ipmi_fru_truncated.dat", bytes([0x01, 0x00, 0x00]))


# ---------------------------------------------------------------------------
# Invalid fixture D: bad_board_checksum — Board area checksum corrupted
# ---------------------------------------------------------------------------
def make_bad_board_checksum():
    board = build_board_area(
        lang=0,
        mfg_minutes=0,
        manufacturer="IBM",
        product="Board",
        serial="SN1",
        part="PN1",
        fru_file_id="",
    )
    hdr = build_common_header({"board": 8})
    image = bytearray(hdr + board)
    # Flip the last byte of the board area (checksum)
    board_start = 8
    board_end = board_start + len(board) - 1
    image[board_end] ^= 0xFF
    write("ipmi_fru_bad_board_checksum.dat", bytes(image))


# ---------------------------------------------------------------------------
# Invalid fixture E: bad_product_checksum — Product area checksum corrupted
# ---------------------------------------------------------------------------
def make_bad_product_checksum():
    prod = build_product_area(
        lang=0,
        manufacturer="X",
        product_name="Y",
        part_model="Z",
        version="1",
        serial="S",
        asset_tag="A",
        fru_file_id="",
    )
    hdr = build_common_header({"product": 8})
    image = bytearray(hdr + prod)
    prod_start = 8
    prod_end = prod_start + len(prod) - 1
    image[prod_end] ^= 0xFF
    write("ipmi_fru_bad_product_checksum.dat", bytes(image))


# ---------------------------------------------------------------------------
# Invalid fixture F: field_overrun — a field length byte claims more bytes
#                   than remain in the area
# ---------------------------------------------------------------------------
def make_field_overrun():
    prod = build_product_area(
        lang=0,
        manufacturer="IBM",
        product_name="Drive",
        part_model="PM9",
        version="1",
        serial="SN001",
        asset_tag="A",
        fru_file_id="",
    )
    hdr = build_common_header({"product": 8})
    image = bytearray(hdr + prod)
    # The first type/length byte of Product area fields is at offset 8+3 = 11
    # (bytes [0]=version, [1]=len, [2]=lang)
    # Inflate its length to 0x3F (63 bytes) to overflow the area
    image[11] = 0xC0 | 0x3F  # type=3 (8-bit ASCII), length=63
    # Invalidate the area checksum so we can leave a bad length without
    # triggering the checksum check first (force checksum to match new content)
    prod_start = 8
    prod_len_units = image[prod_start + 1]
    prod_len_bytes = prod_len_units * 8
    new_cs = zero_checksum(
        bytes(image[prod_start : prod_start + prod_len_bytes - 1])
    )
    image[prod_start + prod_len_bytes - 1] = new_cs
    write("ipmi_fru_field_overrun.dat", bytes(image))


# ---------------------------------------------------------------------------
# Invalid fixture G: bad_multirecord_hdr_checksum
# ---------------------------------------------------------------------------
def make_bad_multirecord_hdr_checksum():
    mr = build_multirecord(0xC0, b"\x01\x02\x03", end_of_list=True)
    hdr = build_common_header({"multirecord": 8})
    image = bytearray(hdr + mr)
    # Corrupt the MultiRecord header checksum (byte index 4 of the record = offset 12)
    image[8 + 4] ^= 0xFF
    write("ipmi_fru_bad_multirecord_hdr_checksum.dat", bytes(image))


# ---------------------------------------------------------------------------
# Fixture for write-back test: writeable_product — single Product area,
# writable from a temp file (the test copies this to a tmpfile)
# ---------------------------------------------------------------------------
def make_writeable_product():
    prod = build_product_area(
        lang=0,
        manufacturer="IBM",
        product_name="NVMe Drive",
        part_model="PM9D3a",
        version="A1",
        serial="OLDSERIAL001",
        asset_tag="TAG-OLD",
        fru_file_id="",
    )
    hdr = build_common_header({"product": 8})
    image = hdr + prod
    write("ipmi_fru_writeable_product.dat", image)
    return image


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    print("Generating IPMI FRU VPD test fixtures...")
    make_product_only()
    make_board_only()
    make_all_areas()
    make_6bit_ascii()
    make_multirecord_only()
    make_bad_hdr_checksum()
    make_bad_format_version()
    make_truncated()
    make_bad_board_checksum()
    make_bad_product_checksum()
    make_field_overrun()
    make_bad_multirecord_hdr_checksum()
    make_writeable_product()
    print("Done.")
