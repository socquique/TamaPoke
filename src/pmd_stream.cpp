#include <M5Cardputer.h>
#include <cstring>
#include <algorithm>
#include "pmd_stream.h"

PmdStream::PmdStream() {}
PmdStream::~PmdStream() { unload(); }

void PmdStream::unload() {
  if (_file) _file.close();
  if (_frameBuf) {
    free(_frameBuf);
    _frameBuf = nullptr;
  }
  _frameCapacity = 0;
  _loaded = false;
  _palCount = 0;
  _cachedAction = -1;
  _cachedFrame = -1;
  for (auto &a : _act) a = ActionMeta();
}

bool PmdStream::has(uint8_t action) const {
  return _loaded && action < PMD_NACTS && _act[action].present;
}

uint8_t PmdStream::resolvedAction(uint8_t requested) const {
  if (requested < PMD_NACTS && _act[requested].present) return requested;
  if (_act[PMD_IDLE].present) return PMD_IDLE;
  for (uint8_t i = 0; i < PMD_NACTS; ++i)
    if (_act[i].present) return i;
  return PMD_IDLE;
}

bool PmdStream::load(uint16_t dex, bool shiny) {
  unload();
  if (dex < 1 || dex > 151) return false;

  char path[28];
  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", dex);
  _file = SD.open(path, FILE_READ);

  if (!_file && shiny) {
    snprintf(path, sizeof(path), "/mons/p%03u.bin", dex);
    _file = SD.open(path, FILE_READ);
  }
  if (!_file) return false;

  char magic[4];
  if (_file.read((uint8_t*)magic, 4) != 4 || memcmp(magic, "TPK2", 4) != 0) {
    unload();
    return false;
  }

  int nActs = _file.read();
  if (nActs <= 0 || nActs > 32) {
    unload();
    return false;
  }

  uint8_t pc[2];
  if (_file.read(pc, 2) != 2) {
    unload();
    return false;
  }
  _palCount = (uint16_t)pc[0] | ((uint16_t)pc[1] << 8);
  if (_palCount == 0 || _palCount > 256) {
    unload();
    return false;
  }
  if (_file.read((uint8_t*)_pal, _palCount * 2) != _palCount * 2) {
    unload();
    return false;
  }

  uint32_t maxFrame = 0;

  for (int a = 0; a < nActs; ++a) {
    uint8_t hdr[4];
    if (_file.read(hdr, 4) != 4) {
      unload();
      return false;
    }

    const uint8_t id = hdr[0];
    const uint8_t w = hdr[1];
    const uint8_t h = hdr[2];
    const uint8_t nf = hdr[3];

    if (w == 0 || h == 0 || nf == 0 || nf > 24) {
      unload();
      return false;
    }

    uint16_t durations[24] = {0};
    for (uint8_t i = 0; i < nf; ++i) {
      uint8_t b[2];
      if (_file.read(b, 2) != 2) {
        unload();
        return false;
      }
      durations[i] = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
      if (durations[i] == 0) durations[i] = 100;
    }

    const uint32_t dataOffset = _file.position();
    const uint32_t frameBytes = (uint32_t)w * h;
    const uint32_t dataBytes = frameBytes * nf;

    if (id < PMD_NACTS) {
      ActionMeta &m = _act[id];
      m.present = true;
      m.w = w;
      m.h = h;
      m.frames = nf;
      m.dataOffset = dataOffset;
      memcpy(m.dur, durations, sizeof(uint16_t) * nf);
      if (frameBytes > maxFrame) maxFrame = frameBytes;
    }

    if (!_file.seek(dataOffset + dataBytes)) {
      unload();
      return false;
    }
  }

  if (maxFrame == 0) {
    unload();
    return false;
  }

  _frameBuf = (uint8_t*)malloc(maxFrame);
  if (!_frameBuf) {
    Serial.printf("PMD: no RAM for %lu-byte frame buffer\n", (unsigned long)maxFrame);
    unload();
    return false;
  }
  _frameCapacity = maxFrame;
  _loaded = true;

  for (uint8_t id = 0; id < PMD_NACTS; ++id) {
    if (_act[id].present) computeBase(id);
  }

  _cachedAction = -1;
  _cachedFrame = -1;

  uint8_t count = 0;
  for (uint8_t id = 0; id < PMD_NACTS; ++id) if (_act[id].present) ++count;
  Serial.printf("PMD stream: %s, %u actions, frame RAM=%lu bytes\n",
                path, count, (unsigned long)maxFrame);
  return true;
}

int PmdStream::frameAt(uint8_t action, uint32_t nowMs) const {
  action = resolvedAction(action);
  const ActionMeta &m = _act[action];
  if (!m.present || m.frames == 0) return 0;

  uint32_t total = 0;
  for (uint8_t i = 0; i < m.frames; ++i)
    total += m.dur[i] ? m.dur[i] : 100;
  if (total == 0) return 0;

  uint32_t t = nowMs % total;
  uint32_t acc = 0;
  for (uint8_t i = 0; i < m.frames; ++i) {
    acc += m.dur[i] ? m.dur[i] : 100;
    if (t < acc) return i;
  }
  return m.frames - 1;
}

bool PmdStream::readFrame(uint8_t action, int frame) {
  action = resolvedAction(action);
  const ActionMeta &m = _act[action];
  if (!m.present || !_frameBuf || frame < 0 || frame >= m.frames) return false;
  if (_cachedAction == (int8_t)action && _cachedFrame == frame) return true;

  const uint32_t frameBytes = (uint32_t)m.w * m.h;
  if (frameBytes > _frameCapacity) return false;

  const uint32_t pos = m.dataOffset + frameBytes * frame;
  if (!_file.seek(pos)) return false;
  if (_file.read(_frameBuf, frameBytes) != frameBytes) return false;

  _cachedAction = (int8_t)action;
  _cachedFrame = frame;
  return true;
}

bool PmdStream::computeBase(uint8_t action) {
  if (action >= PMD_NACTS || !_act[action].present) return false;
  ActionMeta &m = _act[action];

  uint8_t best = 1;
  for (uint8_t f = 0; f < m.frames; ++f) {
    if (!readFrame(action, f)) return false;
    for (int r = m.h - 1; r >= 0; --r) {
      bool any = false;
      for (int c = 0; c < m.w; ++c) {
        if (_frameBuf[(uint32_t)r * m.w + c] != 0xFF) {
          any = true;
          break;
        }
      }
      if (any) {
        if (r + 1 > best) best = r + 1;
        break;
      }
    }
  }
  m.base = best;
  return true;
}

void PmdStream::draw(M5Canvas &target, uint8_t requested, int16_t cx,
                     int16_t groundY, uint32_t nowMs, int8_t forcedScale,
                     bool silhouette, uint16_t silhouetteColor) {
  if (!_loaded) return;

  const uint8_t action = resolvedAction(requested);
  const ActionMeta &m = _act[action];
  int fi = frameAt(action, nowMs);
  if (!readFrame(action, fi)) return;

  int baseScale = forcedScale > 0 ? forcedScale : 0;
  if (baseScale <= 0) {
    int scaleX = 118 / std::max<int>(1, m.w);
    int scaleY = 72 / std::max<int>(1, m.h);
    baseScale = std::min(scaleX, scaleY);
    if (baseScale < 1) baseScale = 1;
    if (baseScale > 3) baseScale = 3;
  }

  // forcedScale == -1 is used only by the home habitat. It enlarges the
  // normal integer pixel-art scale by 25% using nearest-neighbor integer
  // boundaries. No bilinear filtering is used, so the Pokemon stays crisp.
  const int scaleNum = (forcedScale == -1) ? baseScale * 5 : baseScale;
  const int scaleDen = (forcedScale == -1) ? 4 : 1;
  const int destW = ((int)m.w * scaleNum + scaleDen - 1) / scaleDen;
  const uint8_t base = m.base ? m.base : m.h;
  const int basePixels = ((int)base * scaleNum + scaleDen - 1) / scaleDen;
  const int16_t x0 = cx - destW / 2;
  const int16_t y0 = groundY - basePixels;

  for (uint8_t y = 0; y < m.h; ++y) {
    int py0 = y0 + ((int)y * scaleNum) / scaleDen;
    int py1 = y0 + ((int)(y + 1) * scaleNum) / scaleDen;
    int ph = std::max(1, py1 - py0);
    for (uint8_t x = 0; x < m.w; ++x) {
      uint8_t idx = _frameBuf[(uint32_t)y * m.w + x];
      if (idx == 0xFF || idx >= _palCount) continue;
      int px0 = x0 + ((int)x * scaleNum) / scaleDen;
      int px1 = x0 + ((int)(x + 1) * scaleNum) / scaleDen;
      int pw = std::max(1, px1 - px0);
      uint16_t col = silhouette ? silhouetteColor : _pal[idx];
      target.fillRect(px0, py0, pw, ph, col);
    }
  }
}

static bool openPmdPath(File &f, uint16_t dex, bool shiny) {
  char path[28];
  snprintf(path, sizeof(path), "/mons/p%s%03u.bin", shiny ? "s" : "", dex);
  f = SD.open(path, FILE_READ);
  if (!f && shiny) {
    snprintf(path, sizeof(path), "/mons/p%03u.bin", dex);
    f = SD.open(path, FILE_READ);
  }
  return (bool)f;
}

bool drawPmdPreview(M5Canvas &target, uint16_t dex, bool shiny,
                    int16_t boxX, int16_t boxY, int16_t boxW, int16_t boxH,
                    bool silhouette, uint16_t silhouetteColor) {
  if (dex < 1 || dex > 151 || boxW < 2 || boxH < 2) return false;

  File f;
  if (!openPmdPath(f, dex, shiny)) return false;

  char magic[4];
  if (f.read((uint8_t*)magic, 4) != 4 || memcmp(magic, "TPK2", 4) != 0) {
    f.close();
    return false;
  }
  int nActs = f.read();
  uint8_t pc[2];
  if (nActs <= 0 || nActs > 32 || f.read(pc, 2) != 2) {
    f.close();
    return false;
  }

  uint16_t palCount = (uint16_t)pc[0] | ((uint16_t)pc[1] << 8);
  if (palCount == 0 || palCount > 256) {
    f.close();
    return false;
  }
  uint16_t pal[256];
  if (f.read((uint8_t*)pal, palCount * 2) != palCount * 2) {
    f.close();
    return false;
  }

  uint8_t *frame = nullptr;
  uint8_t fw = 0, fh = 0;

  for (int a = 0; a < nActs; ++a) {
    uint8_t hdr[4];
    if (f.read(hdr, 4) != 4) break;
    uint8_t id = hdr[0], w = hdr[1], h = hdr[2], nf = hdr[3];
    if (!w || !h || !nf || nf > 24) break;

    if (!f.seek(f.position() + (uint32_t)nf * 2)) break; // skip durations
    uint32_t dataPos = f.position();
    uint32_t frameBytes = (uint32_t)w * h;

    if (id == PMD_IDLE) {
      frame = (uint8_t*)malloc(frameBytes);
      if (!frame) break;
      if (f.read(frame, frameBytes) != frameBytes) {
        free(frame);
        frame = nullptr;
        break;
      }
      fw = w;
      fh = h;
      break;
    }

    if (!f.seek(dataPos + frameBytes * nf)) break;
  }

  f.close();
  if (!frame || !fw || !fh) {
    if (frame) free(frame);
    return false;
  }

  int downX = (fw + boxW - 1) / boxW;
  int downY = (fh + boxH - 1) / boxH;
  int down = std::max(1, std::max(downX, downY));

  int outW = (fw + down - 1) / down;
  int outH = (fh + down - 1) / down;
  int pix = 1;
  while (pix < 2 && outW * (pix + 1) <= boxW && outH * (pix + 1) <= boxH) ++pix;

  int ox = boxX + (boxW - outW * pix) / 2;
  int oy = boxY + (boxH - outH * pix) / 2;

  for (int oyPix = 0; oyPix < outH; ++oyPix) {
    for (int oxPix = 0; oxPix < outW; ++oxPix) {
      int sx0 = oxPix * down;
      int sy0 = oyPix * down;
      uint8_t idx = 0xFF;

      // Pick the first visible pixel in the represented source block.
      for (int yy = sy0; yy < std::min<int>(fh, sy0 + down) && idx == 0xFF; ++yy) {
        for (int xx = sx0; xx < std::min<int>(fw, sx0 + down); ++xx) {
          uint8_t p = frame[(uint32_t)yy * fw + xx];
          if (p != 0xFF) {
            idx = p;
            break;
          }
        }
      }

      if (idx == 0xFF || idx >= palCount) continue;
      target.fillRect(ox + oxPix * pix, oy + oyPix * pix, pix, pix,
                      silhouette ? silhouetteColor : pal[idx]);
    }
  }

  free(frame);
  return true;
}
