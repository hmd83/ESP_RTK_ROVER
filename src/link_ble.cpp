// BLE transport - Nordic UART Service.
//
// Use this on chips with no Bluetooth Classic radio (C3 / C6 / S3), or with
// apps that have an explicit BLE mode. A BLE peripheral does not appear in the
// phone's normal Bluetooth pairing menu the way SPP does; apps discover it
// themselves over GATT.

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_mac.h>
#include <freertos/stream_buffer.h>

#include "config.h"
#include "link.h"

static char deviceName[32];  // LINK_DEVICE_NAME + "-XXXX", filled in linkBegin()

static NimBLEServer *bleServer = nullptr;
static NimBLECharacteristic *txChar = nullptr;  // notify -> phone
static NimBLECharacteristic *rxChar = nullptr;  // write  <- phone

// The BLE host task must not block on a full UART FIFO, so incoming writes land
// here and the main loop drains them.
static StreamBufferHandle_t downlink = nullptr;

static volatile bool connected = false;
static volatile bool notifyEnabled = false;
static volatile uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
static volatile uint16_t notifyMax = 20;  // pre-MTU-exchange default (23-3)

#if DEBUG_USB
#define DBG(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(...) ((void)0)
#endif

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override {
    connected = true;
    connHandle = info.getConnHandle();

    // Fast interval + full data length: an RTK rover pushes more than the
    // default 40 ms connection interval can carry.
    server->updateConnParams(connHandle, 6, 12, 0, 400);
    server->setDataLen(connHandle, 251);

    DBG("[BLE] connected: %s\n", info.getAddress().toString().c_str());
  }

  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &info, int reason) override {
    connected = false;
    notifyEnabled = false;
    connHandle = BLE_HS_CONN_HANDLE_NONE;
    notifyMax = 20;
    DBG("[BLE] disconnected (reason 0x%02x), advertising again\n", reason);
    NimBLEDevice::startAdvertising();
  }

  void onMTUChange(uint16_t mtu, NimBLEConnInfo &info) override {
    uint16_t payload = (mtu > 3) ? (mtu - 3) : 20;
    notifyMax = payload > BLE_NOTIFY_MAX ? BLE_NOTIFY_MAX : payload;
    DBG("[BLE] MTU %u -> notify payload %u\n", mtu, notifyMax);
  }
};

class TxCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic *chr, NimBLEConnInfo &info,
                   uint16_t subValue) override {
    notifyEnabled = (subValue & 0x0001) != 0;  // bit0 notify, bit1 indicate
    DBG("[BLE] notifications %s\n", notifyEnabled ? "on" : "off");
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *chr, NimBLEConnInfo &info) override {
    NimBLEAttValue val = chr->getValue();
    if (val.length() == 0) return;
    xStreamBufferSend(downlink, val.data(), val.length(), 0);
  }
};

// "ESP32-RTK-A1B2" - the last two octets of the Bluetooth MAC, which is also
// the tail of the address a BLE scanner shows, so the two can be matched up by
// eye. Read from efuse rather than from NimBLE because the name has to be known
// before init(): NimBLE has no way to rename the GAP characteristic afterwards.
static void buildDeviceName() {
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_BT);
  snprintf(deviceName, sizeof(deviceName), "%s-%02X%02X", LINK_DEVICE_NAME,
           mac[4], mac[5]);
}

void linkBegin() {
  downlink = xStreamBufferCreate(BLE_DOWNLINK_BUF, 1);

  buildDeviceName();

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setMTU(247);
  NimBLEDevice::setPower(BLE_TX_POWER_DBM);

#if BLE_ENABLE_BONDING
  // "Just Works" pairing for apps that insist on a bonded device. The
  // characteristics stay unencrypted, so apps that don't pair still work.
  NimBLEDevice::setSecurityAuth(true, false, true);  // bonding, no MITM, LE SC
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
#endif

  bleServer = NimBLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  bleServer->advertiseOnDisconnect(true);

  NimBLEService *svc = bleServer->createService(NUS_SERVICE_UUID);

  txChar = svc->createCharacteristic(NUS_TX_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);
  txChar->setCallbacks(new TxCallbacks());

  rxChar = svc->createCharacteristic(
      NUS_RX_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  // The 31-byte advertisement cannot hold flags + a 128-bit service UUID + the
  // name (3 + 18 + 16 = 37). If the name is what gets dropped, phone Bluetooth
  // menus hide the device entirely - they list BLE peers by name only. So:
  // name in the advertisement (3 + 16 + 3 = 22), service UUID in the scan
  // response. Keep the suffix in mind before lengthening LINK_DEVICE_NAME.
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setName(deviceName);
  advData.addTxPower();

  NimBLEAdvertisementData scanData;
  scanData.addServiceUUID(NUS_SERVICE_UUID);

  NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->enableScanResponse(true);
  adv->setMinInterval(160);  // ~100 ms, so phone scans pick it up promptly
  adv->setMaxInterval(240);  // ~150 ms
  adv->start();

  DBG("[BLE] advertising as \"%s\" (%s)\n", deviceName,
      NimBLEDevice::getAddress().toString().c_str());
}

bool linkReady() { return connected && notifyEnabled; }

size_t linkMaxChunk() { return notifyMax; }

bool linkWrite(const uint8_t *data, size_t len) {
  if (!linkReady()) return false;
  txChar->setValue(data, len);
  return txChar->notify();
}

size_t linkRead(uint8_t *buf, size_t max) {
  if (xStreamBufferBytesAvailable(downlink) == 0) return 0;
  return xStreamBufferReceive(downlink, buf, max, 0);
}

const char *linkName() { return "BLE"; }

const char *linkDeviceName() { return deviceName; }
