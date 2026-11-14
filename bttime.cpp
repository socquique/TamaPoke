#include "bttime.h"
#include "rtcbat.h"
#include "clockzone.h"
#include "pet.h"
#include <WiFi.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>
#include <BLEAdvertising.h>

#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <os/os_mbuf.h>
#include <nvs.h>

extern Pet pet;

// Advertised name. Keep <= 8 so it fits a scan-response complete name.
static const char *kAdvName = "TamaPoke";

// Custom writable Unix-time service (board is the GATT server). Do not
// publish official CTS 0x1805 here — that is the *phone's* service.
static const char *kTimeSvcUuid = "6b1d1e90-4c7a-4f6f-9c3a-2e8b7a11a001";
static const char *kTimeChrUuid = "6b1d1e90-4c7a-4f6f-9c3a-2e8b7a11a002";

// ANCS 128-bit UUID, little-endian in the AD payload
// 7905F431-B5CE-4E99-A40F-4B1E122D00D0
static const uint8_t kAncsSolicit[] = {
    0x11, 0x15, 0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
    0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79};

// 16-bit service solicitation for Current Time Service 0x1805
static const uint8_t kCtsSolicit[] = {0x03, 0x14, 0x05, 0x18};

static const uint16_t kCtsSvc = 0x1805;
static const uint16_t kCtsChr = 0x2A2B;
static const uint16_t kLtiChr = 0x2A0F;

static uint32_t sAppliedCount = 0;  // loop-task only
static TaskHandle_t sTask = nullptr;
static volatile uint32_t sReqMs = 0;
static volatile bool sReqPair = false;
static volatile bool sReqStop = false;
static volatile bool sReqClear = false;
static volatile bool sBusy = false;
static volatile bool sPairMode = false;
static volatile bool sConnected = false;
static volatile bool sEncrypted = false;
static volatile bool sNeedAdv = false;
static volatile uint16_t sConn = BLE_HS_CONN_HANDLE_NONE;
static volatile uint32_t sPendingEpoch = 0;
static volatile bool sPendingApply = false;
static volatile bool sGotTime = false;
static volatile int sBondCount = -1;
static char sLast[48] = "none";
static char sPeer[24] = "-";
static BLECharacteristic *sTimeChr = nullptr;
static BLESecurity *sSec = nullptr;

struct GattWait {
  SemaphoreHandle_t sem;
  bool found;
  uint16_t start;
  uint16_t end;
  uint16_t handle;
  uint8_t data[16];
  uint16_t len;
  int status;
};

static GattWait sGatt;

static void setLast(const char *s) {
  strncpy(sLast, s, sizeof(sLast) - 1);
  sLast[sizeof(sLast) - 1] = 0;
}

static void wifiOff() {
  WiFi.mode(WIFI_OFF);
  delay(50);
}

static int bondCountLive() {
  int n = 0;
  if (ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &n) != 0) return 0;
  return n;
}

static void refreshBondCount() {
  if (BLEDevice::getInitialized()) sBondCount = bondCountLive();
}

static bool queueEpoch(uint32_t e, const char *src) {
  if (!rtcEpochIsPlausible(e)) {
    Serial.printf("BLE time: rejected epoch=%lu from %s (below compile floor %lu)\n",
                  (unsigned long)e, src, (unsigned long)rtcCompileFloorEpoch());
    setLast("rejected");
    return false;
  }
  sPendingEpoch = e;
  sPendingApply = true;
  sGotTime = true;
  setLast("ok");
  Serial.printf("BLE time ok epoch=%lu (%s)\n", (unsigned long)e, src);
  return true;
}

static bool parseWrittenTime(const uint8_t *data, size_t len, uint32_t *out) {
  if (!data || len == 0) return false;
  if (len == 4) {
    *out = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
    return *out != 0;
  }
  // ASCII decimal Unix epoch
  size_t i = 0;
  while (i < len && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r')) i++;
  if (i >= len || !isdigit((int)data[i])) return false;
  uint64_t v = 0;
  for (; i < len; i++) {
    if (data[i] == 0 || data[i] == ' ' || data[i] == '\n' || data[i] == '\r') break;
    if (!isdigit((int)data[i])) return false;
    v = v * 10u + (uint32_t)(data[i] - '0');
    if (v > 0xFFFFFFFFULL) return false;
  }
  *out = (uint32_t)v;
  return *out != 0;
}

static bool ctsToUtc(const uint8_t *ct, size_t ctLen, const uint8_t *lti, size_t ltiLen,
                     uint32_t *out) {
  if (!ct || ctLen < 7) return false;
  uint16_t year = (uint16_t)ct[0] | ((uint16_t)ct[1] << 8);
  uint8_t mon = ct[2], day = ct[3], h = ct[4], mi = ct[5], sec = ct[6];
  if (year < 2020 || year > 2100 || mon < 1 || mon > 12 || day < 1 || day > 31 || h > 23 ||
      mi > 59 || sec > 60) {
    return false;
  }
  int64_t local = (int64_t)rtcCivilToEpoch((int)year, (int)mon, (int)day, (int)h, (int)mi, (int)sec);
  int tz15 = 0, dst15 = 0;
  bool haveTz = false;
  if (lti && ltiLen >= 2) {
    int8_t tz = (int8_t)lti[0];
    uint8_t dst = lti[1];
    if (tz != (int8_t)-128) {
      tz15 = tz;
      haveTz = true;
    }
    if (dst != 255) dst15 = (int)dst;  // already in 15-minute units (0, 2, 4, 8)
  }
  if (!haveTz) {
    // CTS is civil time, not UTC. Guessing here would silently shift the clock
    // by nine hours. A bonded iPhone supplies LTI; otherwise use epoch write.
    Serial.println("BLE time: CTS has no valid timezone; refusing to guess UTC");
    return false;
  }
  uint32_t utc = clockCtsUtc((uint32_t)local, tz15, dst15);
  if (!utc) return false;
  *out = utc;
  Serial.printf("BLE CTS local=%04u-%02u-%02u %02u:%02u:%02u tz15=%d dst15=%d utc=%lu%s\n",
                year, mon, day, h, mi, sec, haveTz ? tz15 : 0, dst15, (unsigned long)*out,
                haveTz ? "" : " (no LTI, treating as UTC)");
  return true;
}

// --- GATT client on the already-open peripheral connection -----------------

static int discSvcCb(uint16_t, const struct ble_gatt_error *error, const struct ble_gatt_svc *svc,
                     void *arg) {
  GattWait *w = (GattWait *)arg;
  if (!error) return 0;
  if (error->status == 0 && svc) {
    w->found = true;
    w->start = svc->start_handle;
    w->end = svc->end_handle;
  }
  if (error->status == BLE_HS_EDONE || (error->status != 0 && error->status != BLE_HS_EDONE && !w->found)) {
    w->status = (error->status == BLE_HS_EDONE) ? 0 : error->status;
    xSemaphoreGive(w->sem);
  } else if (error->status != 0 && error->status != BLE_HS_EDONE) {
    w->status = error->status;
    xSemaphoreGive(w->sem);
  }
  return 0;
}

static int discChrCb(uint16_t, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
                     void *arg) {
  GattWait *w = (GattWait *)arg;
  if (!error) return 0;
  if (error->status == 0 && chr) {
    w->found = true;
    w->handle = chr->val_handle;
  }
  if (error->status == BLE_HS_EDONE) {
    w->status = w->found ? 0 : BLE_HS_ENOENT;
    xSemaphoreGive(w->sem);
  } else if (error->status != 0) {
    w->status = error->status;
    xSemaphoreGive(w->sem);
  }
  return 0;
}

static int readCb(uint16_t, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  GattWait *w = (GattWait *)arg;
  w->len = 0;
  w->status = error ? error->status : BLE_HS_EUNKNOWN;
  if (error && error->status == 0 && attr && attr->om) {
    uint16_t n = OS_MBUF_PKTLEN(attr->om);
    if (n > sizeof(w->data)) n = sizeof(w->data);
    if (os_mbuf_copydata(attr->om, 0, n, w->data) == 0) w->len = n;
  }
  xSemaphoreGive(w->sem);
  return 0;
}

static bool waitGatt(uint32_t ms) {
  if (xSemaphoreTake(sGatt.sem, pdMS_TO_TICKS(ms)) != pdTRUE) {
    sGatt.status = BLE_HS_ETIMEOUT;
    return false;
  }
  return sGatt.status == 0;
}

static bool discoverSvc(uint16_t conn, uint16_t uuid16, uint16_t *start, uint16_t *end) {
  sGatt.found = false;
  sGatt.status = 0;
  ble_uuid16_t u = BLE_UUID16_INIT(uuid16);
  int rc = ble_gattc_disc_svc_by_uuid(conn, &u.u, discSvcCb, &sGatt);
  if (rc != 0) {
    Serial.printf("BLE time: disc svc 0x%04x rc=%d\n", uuid16, rc);
    return false;
  }
  if (!waitGatt(2500) || !sGatt.found) return false;
  *start = sGatt.start;
  *end = sGatt.end;
  return true;
}

static bool discoverChr(uint16_t conn, uint16_t start, uint16_t end, uint16_t uuid16, uint16_t *handle) {
  sGatt.found = false;
  sGatt.status = 0;
  sGatt.handle = 0;
  ble_uuid16_t u = BLE_UUID16_INIT(uuid16);
  int rc = ble_gattc_disc_chrs_by_uuid(conn, start, end, &u.u, discChrCb, &sGatt);
  if (rc != 0) return false;
  if (!waitGatt(2500) || !sGatt.found) return false;
  *handle = sGatt.handle;
  return true;
}

static bool readChr(uint16_t conn, uint16_t handle, uint8_t *buf, uint16_t *len, uint16_t maxLen) {
  sGatt.len = 0;
  sGatt.status = 0;
  int rc = ble_gattc_read(conn, handle, readCb, &sGatt);
  if (rc != 0) return false;
  if (!waitGatt(2500)) return false;
  uint16_t n = sGatt.len;
  if (n > maxLen) n = maxLen;
  memcpy(buf, sGatt.data, n);
  *len = n;
  return n > 0;
}

static bool tryReadCts(uint16_t conn) {
  uint16_t start = 0, end = 0, hTime = 0, hLti = 0;
  if (!discoverSvc(conn, kCtsSvc, &start, &end)) {
    Serial.println("BLE time: CTS 0x1805 not found");
    return false;
  }
  if (!discoverChr(conn, start, end, kCtsChr, &hTime)) {
    Serial.println("BLE time: Current Time 0x2A2B not found");
    return false;
  }
  uint8_t ct[16];
  uint16_t ctLen = 0;
  if (!readChr(conn, hTime, ct, &ctLen, sizeof(ct))) {
    Serial.println("BLE time: 0x2A2B read failed");
    return false;
  }
  uint8_t lti[8];
  uint16_t ltiLen = 0;
  if (discoverChr(conn, start, end, kLtiChr, &hLti)) {
    if (!readChr(conn, hLti, lti, &ltiLen, sizeof(lti))) ltiLen = 0;
  } else {
    Serial.println("BLE time: Local Time Info 0x2A0F not found");
  }
  uint32_t utc = 0;
  if (!ctsToUtc(ct, ctLen, ltiLen ? lti : nullptr, ltiLen, &utc)) {
    Serial.println("BLE time: CTS payload invalid");
    return false;
  }
  return queueEpoch(utc, "CTS");
}

// --- Server / security callbacks -------------------------------------------

class TimeWriteCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override { onWrite(c, (ble_gap_conn_desc *)nullptr); }
  void onWrite(BLECharacteristic *c, ble_gap_conn_desc *) override {
    if (!c) return;
    uint8_t *data = c->getData();
    size_t len = c->getLength();
    uint32_t e = 0;
    if (!parseWrittenTime(data, len, &e)) {
      Serial.println("BLE time: write ignored (need 4-byte LE uint32 or ASCII epoch)");
      return;
    }
    queueEpoch(e, "write");
  }
};

class SrvCb : public BLEServerCallbacks {
  void onConnect(BLEServer *, ble_gap_conn_desc *desc) override {
    if (!desc) return;
    sConn = desc->conn_handle;
    sConnected = true;
    sEncrypted = desc->sec_state.encrypted;
    snprintf(sPeer, sizeof(sPeer), "%02x:%02x:%02x:%02x:%02x:%02x", desc->peer_id_addr.val[5],
             desc->peer_id_addr.val[4], desc->peer_id_addr.val[3], desc->peer_id_addr.val[2],
             desc->peer_id_addr.val[1], desc->peer_id_addr.val[0]);
    Serial.printf("BLE time: connected from %s handle=%u encrypted=%d\n", sPeer, desc->conn_handle,
                  (int)sEncrypted);
    if (!sEncrypted) {
      int rc = 0;
      if (!BLESecurity::startSecurity(desc->conn_handle, &rc)) {
        Serial.printf("BLE time: startSecurity rc=%d (will retry)\n", rc);
      } else {
        Serial.println("BLE time: Just Works pairing — confirm Pair on the iPhone if asked. No passkey.");
      }
    }
  }
  void onDisconnect(BLEServer *) override {
    Serial.println("BLE time: disconnected");
    sConnected = false;
    sEncrypted = false;
    sConn = BLE_HS_CONN_HANDLE_NONE;
    sNeedAdv = true;
  }
};

class SecCb : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override {
    Serial.println("BLE time: passkey request (unexpected with Just Works); using 000000");
    return 0;
  }
  void onPassKeyNotify(uint32_t passkey) override {
    Serial.printf("BLE time: passkey=%06lu (enter this on the phone if asked)\n",
                  (unsigned long)passkey);
  }
  bool onSecurityRequest() override { return true; }
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    if (!desc) return;
    bool ok = desc->sec_state.encrypted;
    sEncrypted = ok;
    if (ok) {
      sConn = desc->conn_handle;
      refreshBondCount();
      Serial.printf("BLE time: bonded/encrypted bonded=%d\n", (int)desc->sec_state.bonded);
    } else {
      Serial.println("BLE time: authentication failed");
    }
  }
  bool onConfirmPIN(uint32_t) override { return true; }
};

static TimeWriteCb sWriteCb;
static SrvCb sSrvCb;
static SecCb sSecCb;

static void startAdvertising() {
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  if (!adv) return;
  adv->stop();

  BLEAdvertisementData ad;
  ad.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  // addData takes a non-const pointer
  uint8_t ancs[sizeof(kAncsSolicit)];
  memcpy(ancs, kAncsSolicit, sizeof(ancs));
  ad.addData((char *)ancs, sizeof(ancs));
  uint8_t cts[sizeof(kCtsSolicit)];
  memcpy(cts, kCtsSolicit, sizeof(cts));
  ad.addData((char *)cts, sizeof(cts));

  BLEAdvertisementData sr;
  sr.setName(kAdvName);
  // Put the custom time service UUID in the scan response so nRF Connect
  // can filter on it. 128-bit UUID AD is 18 bytes; name "TamaPoke" is 10;
  // together they exceed 31, so leave the UUID to on-connect discovery.

  adv->setAdvertisementData(ad);
  adv->setScanResponseData(sr);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  adv->start();
  sNeedAdv = false;
  Serial.printf("BLE time: advertising as %s (%s)\n", kAdvName, sPairMode ? "pair, 120s" : "sync");
}

static bool setupServer() {
  BLEServer *server = BLEDevice::createServer();
  if (!server) return false;
  server->setCallbacks(&sSrvCb);
  server->advertiseOnDisconnect(false);

  BLEService *svc = server->createService(BLEUUID(kTimeSvcUuid));
  sTimeChr = svc->createCharacteristic(
      BLEUUID(kTimeChrUuid),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_WRITE_NR);
  sTimeChr->setCallbacks(&sWriteCb);
  uint32_t now = rtcEpoch();
  sTimeChr->setValue(now);
  svc->start();
  return true;
}

static void setupSecurity() {
  if (!sSec) sSec = new BLESecurity();
  BLESecurity::setCapability(ESP_IO_CAP_NONE);
  BLESecurity::setAuthenticationMode(true, false, true);  // bond, no MITM, SC
  BLESecurity::setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLESecurity::setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(&sSecCb);
}

static bool initStack() {
  if (BLEDevice::getInitialized()) return true;
  wifiOff();
  Serial.println("BLE time: init");
  if (!BLEDevice::init(kAdvName)) {
    Serial.println("BLE time: init failed");
    setLast("init-fail");
    return false;
  }
  setupSecurity();
  if (!setupServer()) {
    Serial.println("BLE time: GATT server failed");
    setLast("init-fail");
    return false;
  }
  refreshBondCount();
  return true;
}

static void teardown() {
  if (!BLEDevice::getInitialized()) return;
  BLEDevice::stopAdvertising();
  if (sConn != BLE_HS_CONN_HANDLE_NONE) {
    ble_gap_terminate(sConn, BLE_ERR_REM_USER_CONN_TERM);
    sConn = BLE_HS_CONN_HANDLE_NONE;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  sTimeChr = nullptr;
  sConnected = false;
  sEncrypted = false;
  // deinit(false) so BTPAIR/BTSYNC can start the stack again later.
  BLEDevice::deinit(false);
  Serial.println("BLE time: stopped");
}

static void runSession(uint32_t advertiseMs, bool pairMode) {
  sGotTime = false;
  sPairMode = pairMode;
  sReqStop = false;
  setLast("adv");

  if (!initStack()) return;
  startAdvertising();
  if (pairMode) {
    Serial.println("BLE time: first pairing window. On iPhone: Settings > Bluetooth, tap TamaPoke, then Pair.");
    Serial.println("BLE time: if it does not appear there, use nRF Connect or LightBlue, connect, and write the Unix UTC epoch (ASCII or 4-byte LE) to the Time characteristic.");
  }

  const uint32_t deadline = millis() + advertiseMs;
  uint32_t lastCtsTry = 0;
  bool ctsOk = false;

  while ((int32_t)(millis() - deadline) < 0 && !sGotTime && !sReqStop && !sReqClear) {
    if (sEncrypted && !ctsOk) {
      uint32_t now = millis();
      if (lastCtsTry == 0 || now - lastCtsTry > 1500) {
        lastCtsTry = now;
        uint16_t conn = sConn;
        if (conn != BLE_HS_CONN_HANDLE_NONE) {
          Serial.println("BLE time: reading phone CTS...");
          if (tryReadCts(conn)) ctsOk = true;
        }
      }
    }
    if (sNeedAdv && !sConnected && BLEDevice::getInitialized() && !sGotTime) {
      startAdvertising();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    // honour a mid-session request to extend / restart
    if (sReqMs) break;
  }

  if (sGotTime) {
    setLast("ok");
  } else if (sReqStop || sReqClear) {
    setLast("stopped");
    Serial.println("BLE time: cancelled");
  } else if (!sReqMs) {
    setLast("timeout");
    Serial.println("BLE time: timeout (no CTS / no write)");
  }
  teardown();
}

static void btTask(void *) {
  for (;;) {
    if (!sReqMs && !sReqClear && !sReqStop) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    if (sReqClear) {
      sReqClear = false;
      sBusy = true;
      if (initStack()) {
        ble_store_clear();
        sBondCount = 0;
        Serial.println("BLE bonds cleared. Also forget TamaPoke in iPhone Settings > Bluetooth.");
        setLast("cleared");
        teardown();
      }
      sBusy = false;
    }
    uint32_t ms = sReqMs;
    bool pair = sReqPair;
    if (ms) {
      sReqMs = 0;
      sBusy = true;
      runSession(ms, pair);
      sBusy = false;
    } else if (sReqStop) {
      sReqStop = false;
      teardown();
    }
  }
}

static void ensureTask() {
  if (sGatt.sem == nullptr) sGatt.sem = xSemaphoreCreateBinary();
  if (sTask) return;
  xTaskCreatePinnedToCore(btTask, "bttime", 8192, nullptr, 1, &sTask, 0);
}

void btTimeStart(uint32_t advertiseMs, bool pairMode) {
  if (advertiseMs < 1000) advertiseMs = 1000;
  wifiOff();
  sReqMs = advertiseMs;
  sReqPair = pairMode;
  sReqStop = false;
  ensureTask();
  if (sTask) xTaskNotifyGive(sTask);
}

void btTimeStop() {
  sReqStop = true;
  sReqMs = 0;
  if (sTask) xTaskNotifyGive(sTask);
}

void btTimePoll() {
  if (!sPendingApply) return;
  uint32_t e = sPendingEpoch;
  sPendingApply = false;
  if (!rtcEpochIsPlausible(e)) return;
  rtcSetEpoch(e);
  pet.syncClock(e);
  ++sAppliedCount;
  if (sTimeChr) sTimeChr->setValue(e);
  Serial.printf("BLE time applied epoch=%lu rtc=%lu\n", (unsigned long)e, (unsigned long)rtcEpoch());
}

// NimBLE keeps bonds in NVS namespace "nimble_bond" (peer_sec_N / our_sec_N).
// Looked up without starting the BLE stack, so a board that was never paired
// never turns the radio on at boot.
bool btTimeHasBond() {
  if (sBondCount >= 0) return sBondCount > 0;
  nvs_iterator_t it = nullptr;
  bool found = false;
  esp_err_t err = nvs_entry_find(NVS_DEFAULT_PART_NAME, "nimble_bond", NVS_TYPE_ANY, &it);
  while (err == ESP_OK && it) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    if (strncmp(info.key, "peer_sec", 8) == 0) { found = true; break; }
    err = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  return found;
}

uint32_t btTimeAppliedCount() { return sAppliedCount; }

bool btTimeBusy() { return sBusy || sReqMs || sReqClear; }

bool btSerialCommand(const String &line) {
  if (line == "BTPAIR") {
    Serial.println("BLE time: pairing window 120s, Just Works (no passkey).");
    btTimeStart(120000, true);
    Serial.println("OK");
    return true;
  }
  if (line == "BTSYNC") {
    Serial.println("BLE time: sync window 45s");
    btTimeStart(45000, false);
    Serial.println("OK");
    return true;
  }
  if (line == "BTCLR") {
    sReqClear = true;
    sReqStop = false;
    ensureTask();
    if (sTask) xTaskNotifyGive(sTask);
    Serial.println("OK");
    return true;
  }
  if (line == "BT?") {
    const char *st = "off";
    if (sBusy && sConnected) st = "conn";
    else if (sBusy) st = "adv";
    Serial.printf("bt=%s bonds=%d last=%s peer=%s floor=%lu rtc=%lu\n", st, sBondCount, sLast, sPeer,
                  (unsigned long)rtcCompileFloorEpoch(), (unsigned long)rtcEpoch());
    Serial.println("OK");
    return true;
  }
  return false;
}
