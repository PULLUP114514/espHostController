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

MAX_CHUNK = 512

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


def send_image(ser, filepath):
    with open(filepath, "rb") as f:
        img = f.read()

    size = len(img)
    print(f"Image size: {size}")

    # --- 1. 发送 HEAD ---
    # uint8 类型 + uint32 size
    # 类型：0=图片
    head_data = struct.pack("<b i", 0, size)
    pkt = build_packet(IMG_DATAHEAD, head_data)
    ser.write(pkt)
    time.sleep(0.05)

    # --- 2. 发送 BODY ---
    offset = 0
    while offset < size:
        chunk = img[offset: offset + MAX_CHUNK]

        body_data = struct.pack("<i", offset) + chunk
        pkt = build_packet(IMG_DATABODY, body_data)

        ser.write(pkt)

        offset += len(chunk)

        # 控制发送速度（很重要）
        time.sleep(0.01)

        print(f"Sent {offset}/{size}")

    # --- 3. 发送 TAIL ---
    pkt = build_packet(IMG_DATATAIL, b"")
    ser.write(pkt)

    print("Send complete")


if __name__ == "__main__":
    ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)

    try:
        send_image(ser, "./data.img")
    finally:
        ser.close()     