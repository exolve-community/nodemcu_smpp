#include <ESP8266WiFi.h>

// Ваши настройки WiFi
const char* ssid = "YOUR_WIFI_SSID"; // Замените на SSID вашей Wi-Fi сети
const char* password = "YOUR_WIFI_PASSWORD"; // Замените на пароль от вашей Wi-Fi сети

// Адрес и порт вашего SMPP-сервера
const char* smppServer = "YOUR_SMPP_SERVER_IP"; // Замените на IP-адрес вашего SMPP-сервера
const int smppPort = YOUR_SMPP_SERVER_PORT; // Замените на порт вашего SMPP-сервера (обычно 2775)

WiFiClient client;

void setup() {
  Serial.begin(9600);
  delay(10);

  // Подключение к Wi-Fi сети
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Подключение к SMPP-серверу
  Serial.println("Connecting to SMPP server...");
  if (client.connect(smppServer, smppPort)) {
    Serial.println("Connected to SMPP server");
    // Отправка команды привязки (bind_receiver)
    sendBindReceiver("YOUR_SYSTEM_ID", "YOUR_SYSTEM_PASSWORD"); // Замените на ваш system_id и пароль для SMPP-сервера
  } else {
    Serial.println("Connection failed");
  }

  // Пример отправки SMS
  sendSMS("SOURCE_NUMBER", "DESTINATION_NUMBER", "Hello, this is a test message!"); // Замените номера и текст на свои данные
}

void loop() {
  // Проверяем, есть ли доступные данные
  if (client.available()) {
    uint8_t data[1024];
    int len = client.read(data, 1024);
    if (len > 0) {
      parsePDU(data, len);
    }
  }

  // Проверка подключения клиента
  if (!client.connected()) {
    Serial.println("Disconnected from SMPP server");
    client.stop();
    // Попробуйте переподключиться
    if (client.connect(smppServer, smppPort)) {
      Serial.println("Reconnected to SMPP server");
      sendBindReceiver("YOUR_SYSTEM_ID", "YOUR_SYSTEM_PASSWORD"); // Замените на ваш system_id и пароль для SMPP-сервера
    } else {
      Serial.println("Reconnection failed");
    }
  }

  // Небольшая задержка для стабильности работы
  delay(1000);
}

void sendBindReceiver(const char* system_id, const char* password) {
  // Формирование и отправка команды bind_receiver
  byte pdu[23 + strlen(system_id) + strlen(password)];
  int pduLength = 0;

  // Заголовок PDU (длина, команда, статус, sequence_number)
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 23 + strlen(system_id) + strlen(password); // длина PDU
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x01; // bind_receiver
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00; // command_status
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x00;
  pdu[pduLength++] = 0x01; // sequence_number

  // system_id
  for (int i = 0; i < strlen(system_id); i++) {
    pdu[pduLength++] = system_id[i];
  }
  pdu[pduLength++] = 0x00;

  // password
  for (int i = 0; i < strlen(password); i++) {
    pdu[pduLength++] = password[i];
  }
  pdu[pduLength++] = 0x00;

  // system_type
  pdu[pduLength++] = 0x00;

  // interface_version
  pdu[pduLength++] = 0x34; // версия интерфейса

  // addr_ton
  pdu[pduLength++] = 0x00;

  // addr_npi
  pdu[pduLength++] = 0x00;

  // address_range
  pdu[pduLength++] = 0x00;

  // Отправка PDU
  client.write(pdu, pduLength);
  Serial.println("Sent bind_receiver");
}

void parsePDU(uint8_t* data, int len) {
  Serial.println("Received PDU:");
  for (int i = 0; i < len; i++) {
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  uint32_t command_length = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
  uint32_t command_id = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
  uint32_t command_status = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
  uint32_t sequence_number = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];

  Serial.print("command_length: "); Serial.println(command_length);
  Serial.print("command_id: "); Serial.println(command_id, HEX);
  Serial.print("command_status: "); Serial.println(command_status);
  Serial.print("sequence_number: "); Serial.println(sequence_number);

  if (command_id == 0x80000001) { // bind_receiver_resp
    Serial.println("Received bind_receiver_resp");
    // Обработка bind_receiver_resp (если требуется)
  } else if (command_id == 0x00000005) { // deliver_sm
    Serial.println("Received deliver_sm");
    parseSMS(data, len);
  } else {
    Serial.println("Not a deliver_sm command");
  }
}

void parseSMS(uint8_t* data, int len) {
  // Проверка esm_class
  if (data[16] == 0x00) { // esm_class должно быть 0x00 для входящего сообщения
    Serial.println("esm_class is 0x00");

    // Попробуем использовать другую начальную позицию
    int pos = 16; // Пропускаем заголовок PDU и мета данные до short_message
    
    // Пропустим дополнительные поля до sm_length
    pos += 13; // Пропускаем команду, статус и мета данные
    
    // Длина SMS сообщения
    int smLength = data[pos++];
    
    Serial.print("smLength: ");
    Serial.println(smLength);
    
    Serial.print("Remaining bytes: ");
    Serial.println(len - pos);
    
    Serial.print("Data: ");
    for (int i = pos; i < len; i++) {
      Serial.print((char)data[i]);
    }
    Serial.println();
    
    if (smLength > 0 && pos + smLength <= len) {
      String smsText = "";
      while (smLength-- > 0 && pos < len) {
        smsText += (char)data[pos++];
      }
      Serial.println("SMS Text: " + smsText);
    } else {
      Serial.println("No SMS text found or invalid length");
    }
  } else if (data[16] == 0x04) { // esm_class для отчета о доставке сообщения
    Serial.println("Received delivery receipt");
    int pos = 16; // Пропускаем заголовок PDU и мета данные до short_message
    pos += 13; // Пропускаем команду, статус и мета данные
    int smLength = data[pos++]; // Длина отчета
    String deliveryReceipt = "";
    while (smLength-- > 0 && pos < len) {
      deliveryReceipt += (char)data[pos++];
    }
    Serial.println("Delivery receipt: " + deliveryReceipt);
  } else {
    Serial.println("esm_class is not 0x00 or 0x04, ignoring message");
  }
}

void sendSMS(const char* sourceAddr, const char* destAddr, const char* message) {
  byte pdu[200]; // Размер PDU зависит от длины сообщения и других параметров
  int pduLength = 0;

  // Заголовок PDU (длина, команда, статус, sequence_number)
  pdu[pduLength++] = 