#pragma once

// Driver minimo del tactil GT911 (I2C) para el panel Guition 4848S040.
// La interfaz imita la parte de SensorLib (TouchDrvCST92xx) que usa el
// TamaPoke: begin(Wire, addr, sda, scl), getPoint(&x, &y, n) y
// setMaxCoordinates/setMirrorXY. Protocolo de registros verificado contra el
// driver TAMC_GT911 que usan los proyectos de la placa (Yoradio).
//
// El modulo no saca INT ni RESET: se sondea por I2C (ver TOUCH_POLLING).

#include <Arduino.h>
#include <Wire.h>

class TouchDrvGT911 {
 public:
  bool begin(TwoWire &wire, uint8_t addr, int8_t sda, int8_t scl) {
    _wire = &wire;
    _addr = addr;
    _sda = sda;
    _scl = scl;
    _wire->begin(_sda, _scl);
    // sondeo de presencia: el GT911 debe contestar a su direccion
    _wire->beginTransmission(_addr);
    if (_wire->endTransmission() != 0) return false;
    // leer resolucion configurada en fabrica
    uint8_t res[4] = {0};
    _readBlock(GT911_X_OUTPUT_MAX, res, 4);
    _xMax = res[0] | (res[1] << 8);
    _yMax = res[2] | (res[3] << 8);
    if (_xMax < 8 || _yMax < 8) {  // config en 0: asumir el panel
      _xMax = 480;
      _yMax = 480;
    }
    return true;
  }

  // coordenadas logicas del panel (para mirror en getPoint)
  void setMaxCoordinates(uint16_t x, uint16_t y) { _xMax = x; _yMax = y; }
  void setMirrorXY(bool mirrorX, bool mirrorY) { _mirrorX = mirrorX; _mirrorY = mirrorY; }
  void setSwapXY(bool swap) { _swapXY = swap; }
  uint16_t getMaxX() { return _xMax; }
  uint16_t getMaxY() { return _yMax; }

  // numero de toques de la muestra actual (1 = primer dedo)
  int8_t getPoint(int16_t *x, int16_t *y, uint8_t point = 0) {
    uint8_t info = _readReg(GT911_POINT_INFO);
    int8_t touches = (int8_t)(info & 0x0F);
    bool bufReady = (info >> 7) & 1;
    if (!bufReady || touches <= 0 || touches > 5) {
      _writeReg(GT911_POINT_INFO, 0);  // limpiar el flag aunque no haya datos
      return 0;
    }
    if (point >= (uint8_t)touches) point = 0;
    uint8_t d[7] = {0};
    _readBlock(GT911_POINT_1 + point * 8, d, 7);
    _writeReg(GT911_POINT_INFO, 0);  // ack: sin esto el GT911 deja de reportar

    int16_t px = d[1] | (d[2] << 8);
    int16_t py = d[3] | (d[4] << 8);
    if (_swapXY) {
      int16_t t = px;
      px = py;
      py = t;
    }
    if (_mirrorX) px = (int16_t)_xMax - 1 - px;
    if (_mirrorY) py = (int16_t)_yMax - 1 - py;
    *x = px;
    *y = py;
    return touches;
  }

 private:
  // registros del GT911 (direccion interna de 16 bits)
  static const uint16_t GT911_X_OUTPUT_MAX = 0x8048;  // 0x8048..0x804B (x, y, little endian)
  static const uint16_t GT911_CONFIG_START = 0x8047;
  static const uint16_t GT911_CONFIG_CHKSUM = 0x80FF;
  static const uint16_t GT911_CONFIG_FRESH = 0x8100;
  static const uint16_t GT911_POINT_INFO = 0x814E;
  static const uint16_t GT911_POINT_1 = 0x814F;

  TwoWire *_wire = nullptr;
  uint8_t _addr = 0x5D;
  int8_t _sda = -1, _scl = -1;
  uint16_t _xMax = 480, _yMax = 480;
  bool _mirrorX = false, _mirrorY = false, _swapXY = false;

  uint8_t _readReg(uint16_t reg) {
    _wire->beginTransmission(_addr);
    _wire->write((uint8_t)(reg >> 8));
    _wire->write((uint8_t)(reg & 0xFF));
    if (_wire->endTransmission() != 0) return 0;
    if (_wire->requestFrom(_addr, (uint8_t)1) != 1) return 0;
    return _wire->read();
  }

  void _writeReg(uint16_t reg, uint8_t val) {
    _wire->beginTransmission(_addr);
    _wire->write((uint8_t)(reg >> 8));
    _wire->write((uint8_t)(reg & 0xFF));
    _wire->write(val);
    _wire->endTransmission();
  }

  void _readBlock(uint16_t reg, uint8_t *buf, uint8_t len) {
    _wire->beginTransmission(_addr);
    _wire->write((uint8_t)(reg >> 8));
    _wire->write((uint8_t)(reg & 0xFF));
    if (_wire->endTransmission() != 0) {
      memset(buf, 0, len);
      return;
    }
    _wire->requestFrom(_addr, len);
    for (uint8_t i = 0; i < len; i++) buf[i] = _wire->available() ? _wire->read() : 0;
  }
};
