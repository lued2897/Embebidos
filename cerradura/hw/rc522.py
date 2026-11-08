"""Driver mínimo del lector RFID MFRC522 (13.56 MHz) por SPI con spidev.

Solo lee el UID de la tarjeta. El RST del módulo va a 3.3 V y el chip se
reinicia por software (SoftReset) al crear el objeto.
"""
import time
import spidev

CommandReg, CommIEnReg, CommIrqReg, ErrorReg = 0x01, 0x02, 0x04, 0x06
FIFODataReg, FIFOLevelReg, ControlReg, BitFramingReg = 0x09, 0x0A, 0x0C, 0x0D
ModeReg, TxControlReg, TxASKReg = 0x11, 0x14, 0x15
TModeReg, TPrescalerReg, TReloadRegH, TReloadRegL = 0x2A, 0x2B, 0x2C, 0x2D

PCD_IDLE, PCD_TRANSCEIVE, PCD_SOFTRESET = 0x00, 0x0C, 0x0F
PICC_REQIDL, PICC_ANTICOLL = 0x26, 0x93

OK, NOTAG, ERR = 0, 1, 2


class RC522:
    def __init__(self, bus=0, dev=0, velocidad=1_000_000):
        self.spi = spidev.SpiDev()
        self.spi.open(bus, dev)
        self.spi.max_speed_hz = velocidad
        self.spi.mode = 0
        self._w(CommandReg, PCD_SOFTRESET)
        time.sleep(0.05)
        self._w(TModeReg, 0x8D)
        self._w(TPrescalerReg, 0x3E)
        self._w(TReloadRegL, 30)
        self._w(TReloadRegH, 0)
        self._w(TxASKReg, 0x40)
        self._w(ModeReg, 0x3D)
        if not (self._r(TxControlReg) & 0x03):       # antena encendida
            self._set(TxControlReg, 0x03)

    # --- registros ---
    def _w(self, reg, val):
        self.spi.xfer2([(reg << 1) & 0x7E, val])

    def _r(self, reg):
        return self.spi.xfer2([((reg << 1) & 0x7E) | 0x80, 0])[1]

    def _set(self, reg, mask):
        self._w(reg, self._r(reg) | mask)

    def _clr(self, reg, mask):
        self._w(reg, self._r(reg) & ~mask)

    def _transceive(self, datos):
        self._w(CommIEnReg, 0x77 | 0x80)
        self._clr(CommIrqReg, 0x80)
        self._set(FIFOLevelReg, 0x80)                 # vacía FIFO
        self._w(CommandReg, PCD_IDLE)
        for b in datos:
            self._w(FIFODataReg, b)
        self._w(CommandReg, PCD_TRANSCEIVE)
        self._set(BitFramingReg, 0x80)                # StartSend
        n = 0
        for _ in range(2000):
            n = self._r(CommIrqReg)
            if n & 0x01 or n & 0x30:
                break
        else:
            self._clr(BitFramingReg, 0x80)
            return ERR, [], 0
        self._clr(BitFramingReg, 0x80)
        if self._r(ErrorReg) & 0x1B:
            return ERR, [], 0
        status = NOTAG if (n & 0x77 & 0x01) else OK
        nivel = self._r(FIFOLevelReg)
        ultimos = self._r(ControlReg) & 0x07
        bits = (nivel - 1) * 8 + ultimos if ultimos else nivel * 8
        nivel = max(1, min(nivel, 16))
        return status, [self._r(FIFODataReg) for _ in range(nivel)], bits

    def version(self):
        """VersionReg (0x37): 0x91/0x92 en el chip original, 0xB2 en clones; 0x00 o 0xFF = sin respuesta."""
        return self._r(0x37)

    def leer_uid(self):
        """Devuelve el UID como texto hex ('A1B2C3D4') o None si no hay tarjeta."""
        self._w(BitFramingReg, 0x07)
        st, _, bits = self._transceive([PICC_REQIDL])
        if st != OK or bits != 0x10:
            return None
        self._w(BitFramingReg, 0x00)
        st, uid, _ = self._transceive([PICC_ANTICOLL, 0x20])
        if st != OK or len(uid) < 5:
            return None
        if uid[0] ^ uid[1] ^ uid[2] ^ uid[3] != uid[4]:
            return None
        return "".join(f"{b:02X}" for b in uid[:4])

    def close(self):
        self.spi.close()
