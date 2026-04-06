import serial
import struct
import time

# 串口配置
SERIAL_PORT = "/dev/ttyUSB0"   # 根据实际修改
BAUDRATE = 115200

# 协议常量
HEAD0 = 0xAA
HEAD1 = 0x55
TAIL0 = 0x0D
TAIL1 = 0x07

MAX_CHUNK = 128

# 操作码
IMG_DATAHEAD = 2
IMG_DATABODY = 3
IMG_DATATAIL = 4

# CRC16 (MODBUS)
def crc16(data: bytes):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def build_packet(op_code: int, op_data: bytes):
    package_id = 0  # 未使用

    body = struct.pack(
        "<i b h",  # uint32, uint8, uint16
        package_id,
        op_code,
        len(op_data)
    ) + op_data

    crc = crc16(body)

    packet = bytearray()
    packet.append(HEAD0)
    packet.append(HEAD1)
    packet += body
    packet += struct.pack("<H", crc)
    packet.append(TAIL0)
    packet.append(TAIL1)

    return packet
def print_hex(prefix: str, data: bytes):
    hex_str = " ".join(f"{b:02X}" for b in data)
    print(f"{prefix} [{len(data)}]: {hex_str}")

def send_image(ser, filepath):
    with open(filepath, "rb") as f:
        img = f.read()

    size = len(img)
    print(f"Image size: {size}")

    # --- 1. 发送 HEAD ---
    # uint8 类型 + uint32 size
    # 类型：0=图片\
    head_data = struct.pack("<b i", 0, size)
    pkt = build_packet(IMG_DATAHEAD, head_data)
    print_hex("TX", pkt)
    ser.write(pkt)

    if not wait_ack(ser,255):
        print("HEAD failed")
        return

    # --- 2. 发送 BODY ---
    offset = 0
    while offset < size:
        chunk = img[offset: offset + MAX_CHUNK]

        body_data = struct.pack("<i", offset) + chunk
        pkt = build_packet(IMG_DATABODY, body_data)

        print_hex("TX", pkt)
        ser.write(pkt)

        if not wait_ack(ser,255):
            print(f"BODY failed at offset {offset}")
            return

        offset += len(chunk)
    print(f"Sent {offset}/{size}")    # --- 3. 发送 TAIL ---
    pkt = build_packet(IMG_DATATAIL, b"")
    ser.write(pkt)

    print("Send complete")

ACK_VALUE = 255
ACK_TIMEOUT = 2  # 秒
ACK_LEN = 13  # 2+4+1+2+2+2

def wait_ack(ser, expected_op_code):
    # ACK 总长度 = 13
    ack_len = 13
    data = ser.read(ack_len)
    if len(data) < ack_len:
        print("ACK too short:", data)
        return False

    print_rx(data, "ACK")

    # 解析字段
    head0, head1 = data[0], data[1]
    package_id = struct.unpack("<I", data[2:6])[0]  # 4字节占位
    op_code = data[6]
    op_size = struct.unpack("<H", data[7:9])[0]
    crc_recv = struct.unpack("<H", data[9:11])[0]
    tail0, tail1 = data[11], data[12]

    # CRC 校验
    crc_calc = crc16(data[2:9])  # 从 packageID 到 op_size（不含 CRC 和尾）
    if crc_calc != crc_recv:
        print(f"ACK CRC mismatch: {crc_calc:04X} != {crc_recv:04X}")
        return False

    # 校验头尾
    if head0 != 0xAA or head1 != 0xAA or tail0 != 0x0D or tail1 != 0x07:
        print("ACK head/tail error")
        return False

    if op_code != expected_op_code:
        print(f"ACK op_code mismatch: {op_code} != {expected_op_code}")
        return False

    if op_size != 0:
        print(f"ACK op_size not zero: {op_size}")
        return False

    print("ACK OK")
    return True

def print_rx(data: bytes, prefix="RX"):
    hex_str = " ".join(f"{b:02X}" for b in data)
    try:
        ascii_str = data.decode("ascii", errors="replace")
    except:
        ascii_str = ""
    print(f"{prefix} [{len(data)}]: {hex_str} | {ascii_str}")
if __name__ == "__main__":
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)

    try:
        send_image(ser, "./data.jpg")
    finally:
        ser.close()     