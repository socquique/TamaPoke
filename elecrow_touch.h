#pragma once
// Driver minimo para el tactil CST816D de la Elecrow CrowPanel 1.28".
// Lectura de registros por I2C tal cual la documenta Elecrow/Makerguides;
// getPoint() imita la firma que TamaPoke.ino ya usaba con SensorLib
// (TouchDrvCST92xx) para no tocar handleTouch().
#include <Wire.h>
#include "pin_config.h"

#define CST816D_ADDR 0x15

class ElecrowTouch {
public:
  void setPins(int8_t rst, int8_t irq) { _rst = rst; _irq = irq; }

  bool begin(TwoWire &wire, uint8_t addr, int sda, int scl) {
    _wire = &wire;
    pinMode(_irq, INPUT_PULLUP);
    pinMode(_rst, OUTPUT);
    reset();
    _wire->begin(sda, scl);
    return true;  // el CST816D no tiene registro de identidad facil de sondear
  }

  void reset() {
    digitalWrite(_rst, LOW);
    delay(5);
    digitalWrite(_rst, HIGH);
    delay(50);
  }

  void setMaxCoordinates(int, int) {}  // el chip ya reporta en coords de panel
  void setMirrorXY(bool, bool) {}      // sin espejado en este panel

  // firma compatible con touch.getPoint(&x, &y, 1): devuelve 1 si hay toque.
  // El chip reporta en coordenadas fisicas del panel (0..TFT_WIDTH/HEIGHT),
  // pero TamaPoke.ino dibuja en un canvas logico de LCD_WIDTH/HEIGHT (466):
  // ver flushScaled() en TamaPoke.ino. Reescalamos aqui para que
  // handleTouch() siga comparando contra sus zonas/umbrales en 466-space
  // sin tocar ese codigo.
  int getPoint(int16_t *x, int16_t *y, int) {
    uint8_t raw[7] = {0};
    if (!i2cRead(0x02, raw, sizeof(raw))) return 0;
    int event = raw[1] >> 6;  // 0=down, 1=up, 2=contact
    if (event != 2 && event != 0) return 0;
    int16_t px = (int16_t)raw[2] + (int16_t)(raw[1] & 0x0F) * 256;
    int16_t py = (int16_t)raw[4] + (int16_t)(raw[3] & 0x0F) * 256;
    *x = (int16_t)((int32_t)px * LCD_WIDTH / TFT_WIDTH);
    *y = (int16_t)((int32_t)py * LCD_HEIGHT / TFT_HEIGHT);
    return 1;
  }

private:
  TwoWire *_wire = nullptr;
  int8_t _rst = -1, _irq = -1;

  bool i2cRead(uint8_t reg, uint8_t *data, uint8_t len) {
    _wire->beginTransmission(CST816D_ADDR);
    _wire->write(reg);
    if (_wire->endTransmission(true) != 0) return false;
    if (_wire->requestFrom((int)CST816D_ADDR, (int)len, (int)true) != len) return false;
    for (uint8_t i = 0; i < len && _wire->available(); i++) data[i] = _wire->read();
    return true;
  }
};
