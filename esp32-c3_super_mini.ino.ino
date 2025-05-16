#include <WiFi.h>
#include <HTTPClient.h>

// WiFi данные
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Пины для светодиодов. 
// Проверьте пины GPIO вашей платы ESP32-C3 Supermini! 
// Наиболее распространённые пины: 8, 9, 10, 11 (но ваша плата может отличаться)
const int ledPins[3] = {9, 10, 11}; // Замените на актуальные пины вашей платы

// Трек-номера для проверки (до 3)
const char* trackingNumbers[3] = {
  "RB123456789CN",
  "RB987654321CN",
  "RB555555555CN"
};

// Время задержки (мс)
const unsigned long FIRST_CHECK_DELAY = 30000;             // 30 секунд
const unsigned long REGULAR_CHECK_INTERVAL = 2UL*60*60*1000; // 2 часа

unsigned long lastCheckMillis = 0;
bool firstCheckDone = false;

const int MAX_RETRIES = 3;
const unsigned long RETRY_DELAY_MS = 5000;

// Ключевые слова для определения статуса "пришла посылка"
const char* statusKeywords[] = {
  "прибыла в пункт выдачи",
  "посылка готова к выдаче",
  "можно забрать",
  "пришла в пункт выдачи",
  "ожидает получения"
};

// Функция подключения к WiFi
void connectToWiFi() {
  Serial.printf("Подключение к WiFi SSID: %s\n", ssid);
  WiFi.begin(ssid, password);
  int waitSec = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    waitSec++;
    if(waitSec >= 40) {
      Serial.println("\nОшибка подключения к WiFi!");
      return;
    }
  }
  Serial.println("\nWiFi подключен!");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());
}

// Проверка содержит ли HTML страницы ключевое слово "посылка пришла"
bool isPackageArrived(const String& payload) {
  for (auto kw : statusKeywords) {
    if (payload.indexOf(kw) >= 0) {
      Serial.printf("Найден статус: \"%s\"\n", kw);
      return true;
    }
  }
  return false;
}

// Проверяем трек-номер с ретраями
bool checkTrackingNumber(const char* trackingNumber) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi не подключен, пропуск проверки");
    return false;
  }

  String url = String("https://gdeposylka.ru/tracking/") + trackingNumber;
  Serial.printf("Проверка трек-номера: %s\n", trackingNumber);

  HTTPClient http;
  int attempt = 0;
  bool arrived = false;
  bool success = false;

  while(attempt < MAX_RETRIES && !success) {
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      success = true;
      Serial.printf("Получен ответ, код: %d, длина страницы: %d символов\n", httpCode, payload.length());
      arrived = isPackageArrived(payload);

      if(payload.length() < 100) {
        Serial.println("Внимание: очень короткая страница, возможно ошибка сервера.");
      }
    } else {
      Serial.printf("Ошибка HTTP: %d, попытка %d из %d\n", httpCode, attempt+1, MAX_RETRIES);
      attempt++;
      http.end();
      if(attempt < MAX_RETRIES) {
        Serial.printf("Повтор запроса через %d мс\n", RETRY_DELAY_MS);
        delay(RETRY_DELAY_MS);
      }
    }
    http.end();
  }

  if (!success) {
    Serial.println("Не удалось получить ответ после всех попыток.");
  }

  return arrived;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Запуск ESP32-C3 SuperMini - проверка трек-номеров gdeposylka.ru ===");

  // Инициализация пинов светодиодов и тест подсветки
  for(int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  Serial.println("Тест включения всех светодиодов 1 секунду...");
  for(int i = 0; i < 3; i++) digitalWrite(ledPins[i], HIGH);
  delay(1000);
  for(int i = 0; i < 3; i++) digitalWrite(ledPins[i], LOW);
  Serial.println("Тест завершён.");

  connectToWiFi();

  lastCheckMillis = millis();
  firstCheckDone = false;
}

void loop() {
  unsigned long now = millis();

  if (!firstCheckDone) {
    if(now - lastCheckMillis >= FIRST_CHECK_DELAY) {
      Serial.println("=== Первая проверка трек-номеров ===");
      for(int i = 0; i < 3; i++) {
        if(strlen(trackingNumbers[i]) == 0) continue;
        bool arrived = checkTrackingNumber(trackingNumbers[i]);
        digitalWrite(ledPins[i], arrived ? HIGH : LOW);
        Serial.printf("Светодиод %d -> %s\n", ledPins[i], arrived ? "ВКЛ" : "ВЫКЛ");
      }
      lastCheckMillis = now;
      firstCheckDone = true;
      Serial.println("=== Первая проверка завершена ===");
    }
  } else {
    if(now - lastCheckMillis >= REGULAR_CHECK_INTERVAL) {
      Serial.println("=== Регулярная проверка трек-номеров ===");
      for(int i = 0; i < 3; i++) {
        if(strlen(trackingNumbers[i]) == 0) continue;
        bool arrived = checkTrackingNumber(trackingNumbers[i]);
        digitalWrite(ledPins[i], arrived ? HIGH : LOW);
        Serial.printf("Светодиод %d -> %s\n", ledPins[i], arrived ? "ВКЛ" : "ВЫКЛ");
      }
      lastCheckMillis = now;
      Serial.println("=== Регулярная проверка завершена ===");
    }
  }

  delay(1000);
}

