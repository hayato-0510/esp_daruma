#include <Arduino.h>
#include <MFRC522.h>
#include <esp_now.h>
#include <WiFi.h>

#define SS_PIN 5
#define RST_PIN 26
constexpr uint8_t BTN_PIN = 4;

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

unsigned long buttonPressStart = 0; // ボタンが押され始めた時間
bool isReading = false;             // 現在読み取り中かどうかのフラグ
bool dataSent = false;              // データ送信が完了したかどうか

// マスターのMACアドレス（送信先）
uint8_t masterAddress[] = {0xE8, 0x6B, 0xEA, 0x22, 0x59, 0x88};

// Wi-FiとESP-NOWの設定
void WiFi_setup();
void esp_now_setup();
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onReceive(const uint8_t *mac_addr, const uint8_t *data, int data_len);

void setup() {
    pinMode(BTN_PIN, INPUT);
    Serial.begin(9600);
    while (!Serial);

    SPI.begin();
    mfrc522.PCD_Init();
    mfrc522.PCD_DumpVersionToSerial();
    Serial.println(F("Hold the button for 3 seconds to start reading..."));

    WiFi_setup();
    esp_now_setup();
}

void loop() {
    if (isReading) {
        if (!dataSent) {
            // 「COUNTDOWN」を送信
            const char* message = "COUNTDOWN";
            esp_err_t result = esp_now_send(masterAddress, (const uint8_t*)message, strlen(message));
            if (result == ESP_OK) {
                Serial.println("Data sent: COUNTDOWN");
                dataSent = true; // 一度だけ送信する
            } else {
                Serial.println("Data send failed");
            }
        }

        // RFIDカードを一度だけ読み取り
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
            Serial.print(F("Card UID:"));
            for (byte i = 0; i < mfrc522.uid.size; i++) {
                Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
                Serial.print(mfrc522.uid.uidByte[i], HEX);
            }
            Serial.println();
            mfrc522.PICC_HaltA(); // Halt PICC
            isReading = false; // 読み取り終了
            Serial.println(F("Reading complete. Hold the button again to restart."));
        }
    } else {
        // ボタンが押されているかのチェック
        if (digitalRead(BTN_PIN) == HIGH) {
            if (buttonPressStart == 0) {
                buttonPressStart = millis(); // 押された時間を記録
            } else if (millis() - buttonPressStart >= 3000) {
                // 3秒以上押されていた場合
                Serial.println(F("Starting to read..."));
                isReading = true;
                dataSent = false; // データ送信を再び可能に
                buttonPressStart = 0; // リセット
            }
        } else {
            // ボタンが離された場合
            buttonPressStart = 0; // リセット
        }
    }
}

void WiFi_setup() {
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    while (WiFi.status() != WL_CONNECTED) delay(100);
    Serial.println("Connected Wifi");
}

void esp_now_setup() {
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOWの初期化に失敗しました");
        return;
    }
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, masterAddress, 6);
    peerInfo.channel = WiFi.channel();
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    esp_now_register_send_cb(onSent);
    esp_now_register_recv_cb(onReceive); // 受信コールバックを登録
}

void onSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("ESP-NOW送信ステータス: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "成功" : "失敗");
}

void onReceive(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    String receivedData = "";
    for (int i = 0; i < data_len; i++) {
        receivedData += (char)data[i];
    }
    Serial.print("Received data: ");
    Serial.println(receivedData);

    if (receivedData == "FINISH") {
        Serial.println("FINISH received. Starting RFID detection...");
        isReading = true; // RFID検知を開始
        dataSent = false; // 次回送信を許可
    }
}
