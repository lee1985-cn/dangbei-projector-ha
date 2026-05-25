// 【核心补丁 4】强制 BleKeyboard 统一使用 NimBLE 协议栈！消除精神分裂！
#define USE_NIMBLE 

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BleKeyboard.h>
#include <NimBLEDevice.h>
#include <nvs_flash.h> // 【新增】引入闪存管理库，用于持久化保存配对密钥

const char* ssid = "你的wifi名字";
const char* password = "你的wifi密码";

// 【核心补丁 1】统一蓝牙名称，防止配对信息因名称不一致而失效
const char* BLE_NAME = "Dangbei_ESP32"; 

// 跨软重启保持状态 (针对 ESP32-C3 的特殊防擦除宏)
RTC_NOINIT_ATTR uint32_t boot_mode_magic; 

WebServer server(80);
BleKeyboard* bleKeyboard = nullptr;

// 【核心补丁 2】使用 0x04 掩码触发你修改过的库文件电源键 (Bit 2)
const MediaKeyReport KEY_PROJ_POWER = {0x04, 0x00};

// 经过实测 100% 能够唤醒当贝主板的魔法数据包
const uint8_t pkt1[] = {0x02,0x01,0x05,0x05,0x02,0x0F,0x18,0x12,0x18,0x0E,0xFF,0x46,0x00,0x9B,0xAD,0x4A,0x6D,0x60,0x91,0x0C,0xFF,0xFF,0xFF,0xFF};
const uint8_t pkt2[] = {0x02,0x01,0x05,0x05,0x02,0x0F,0x18,0x12,0x18,0x0E,0xFF,0x46,0x00,0xC5,0xAD,0x4A,0x6D,0x60,0x91,0x0C,0xFF,0xFF,0xFF,0xFF};
const uint8_t pkt3[] = {0x02,0x01,0x05,0x05,0x02,0x0F,0x18,0x12,0x18,0x0E,0xFF,0x46,0x00,0xCB,0xAD,0x4A,0x6D,0x60,0x91,0x0C,0xFF,0xFF,0xFF,0xFF};

// 发送广播包辅助函数 (已修复 C++ 类型转换报错)
void sendRawAdv(NimBLEAdvertising* adv, const uint8_t* data, size_t len) {
    adv->stop();
    delay(10);
    NimBLEAdvertisementData advData;
    advData.addData(std::string((char*)data, len));
    adv->setAdvertisementData(advData);
    adv->start();
}

// ================= 模式 A：魔法开机重放 =================
void runWakeupBeaconMode() {
    Serial.println("\n[MODE A] 发射魔法唤醒包...");
    NimBLEDevice::init(BLE_NAME); // 使用统一名称
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    
    unsigned long t = millis();
    while(millis() - t < 1500) { sendRawAdv(adv, pkt1, sizeof(pkt1)); delay(200); }
    t = millis();
    while(millis() - t < 1500) { sendRawAdv(adv, pkt2, sizeof(pkt2)); delay(200); }
    t = millis();
    while(millis() - t < 1500) { sendRawAdv(adv, pkt3, sizeof(pkt3)); delay(200); }
    
    adv->stop();
    boot_mode_magic = 0; 
    delay(100);
    ESP.restart();
}

// ================= 模式 B：普通控制 =================
void setupHttpEndpoints() {
    server.on("/state", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        if (bleKeyboard && bleKeyboard->isConnected()) server.send(200, "application/json", "{\"state\":\"ON\"}");
        else server.send(200, "application/json", "{\"state\":\"OFF\"}");
    });

    server.on("/power_on", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        if (bleKeyboard && bleKeyboard->isConnected()) {
            server.send(200, "application/json", "{\"status\":\"already_on\"}");
        } else {
            server.send(200, "application/json", "{\"status\":\"waking_up\"}");
            boot_mode_magic = 0xAABBCCDD;
            delay(100);
            ESP.restart();
        }
    });

    server.on("/power_off", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        if (bleKeyboard && bleKeyboard->isConnected()) {
            // 【核心补丁 3】防吞键激活序列：先发一个短促的包唤醒 Sniff 模式链路
            bleKeyboard->releaseAll(); 
            delay(50);
            bleKeyboard->press(KEY_PROJ_POWER);
            delay(100);
            bleKeyboard->releaseAll();
            
            // 停顿后，执行真正的 1.5 秒长按关机
            delay(300); 
            bleKeyboard->press(KEY_PROJ_POWER);
            delay(1500); 
            bleKeyboard->releaseAll();
            
            server.send(200, "application/json", "{\"status\":\"turning_off\"}");
        } else {
            server.send(200, "application/json", "{\"status\":\"already_off\"}");
        }
    });

    // 预留多媒体接口
    server.on("/play_pause", HTTP_GET, []() {
        if (bleKeyboard && bleKeyboard->isConnected()) bleKeyboard->write(KEY_MEDIA_PLAY_PAUSE);
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    server.on("/mute", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        if (bleKeyboard && bleKeyboard->isConnected()) bleKeyboard->write(KEY_MEDIA_MUTE);
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/vol_up", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        if (bleKeyboard && bleKeyboard->isConnected()) bleKeyboard->write(KEY_MEDIA_VOLUME_UP);
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/vol_down", HTTP_GET, []() {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        if (bleKeyboard && bleKeyboard->isConnected()) bleKeyboard->write(KEY_MEDIA_VOLUME_DOWN);
        server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
}

void runNormalControlMode() {
    Serial.println("\n[MODE B] 进入常态控制模式...");
    bleKeyboard = new BleKeyboard(BLE_NAME, "Espressif", 100); // 使用统一名称
    bleKeyboard->begin();

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());

    setupHttpEndpoints();
    server.begin();
}

void setup() {
    Serial.begin(115200);

    // 【核心补丁 5】强制初始化 NVS 闪存，确保蓝牙配对密钥（Bonding）断电不丢
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_reset_reason_t reason = esp_reset_reason();
    if (reason != ESP_RST_SW) { boot_mode_magic = 0; } 

    if (boot_mode_magic == 0xAABBCCDD) {
        runWakeupBeaconMode();
    } else {
        runNormalControlMode();
    }
}

void loop() {
    if (boot_mode_magic != 0xAABBCCDD) server.handleClient();
}