import time
from smbus2 import SMBus

MODE1, MODE2, PRESCALE, LED0_ON_L = 0x00, 0x01, 0xFE, 0x06
OSC_HZ = 25_000_000


class PCA9685:
    def __init__(self, bus=1, addr=0x40, freq_hz=50):
        self.addr = addr
        self.bus = SMBus(bus)
        prescale = round(OSC_HZ / (4096 * freq_hz)) - 1          # 50 Hz -> 121
        self.bus.write_byte_data(addr, MODE1, 0x10)                # sleep
        self.bus.write_byte_data(addr, PRESCALE, prescale)
        self.bus.write_byte_data(addr, MODE1, 0x00)                # wake
        time.sleep(0.005)
        self.bus.write_byte_data(addr, MODE1, 0xA0)                # restart + auto-increment
        self.bus.write_byte_data(addr, MODE2, 0x04)                # salida totem-pole

    def set_pwm(self, canal, cuentas_off, cuentas_on=0):
        """Pulso en alto de cuentas_on a cuentas_off (0..4095) dentro del periodo de 20 ms."""
        reg = LED0_ON_L + 4 * canal
        self.bus.write_i2c_block_data(
            self.addr, reg,
            [cuentas_on & 0xFF, cuentas_on >> 8, cuentas_off & 0xFF, cuentas_off >> 8],
        )

    def apagar(self, canal):
        self.set_pwm(canal, 0)

    def close(self):
        self.bus.close()
