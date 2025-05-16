#include <WiFi.h>
#include <HTTPClient.h>

// Настройки WiFi
const char* ssid = "ИМЯ";       // Замените на ваш SSID
const char* password = "ПАРОЛЬ"; // Замените на ваш пароль

// Пины светодиодов (G1, G2, G3) - выберите подходящие GPIO для вашей платы
const int ledPins[3] = {4, 5, 6};  // Пример: GPIO 4, 5, 6

// Трек-номера (до 3)
const char* trackingNumbers[3] = {
  "ТРЕКНОМЕР",        // Замените на ваш первый трек-номер
  "ТРЕКНОМЕР",     // Замените на ваш второй трек-номер
  "ТРЕКНОМЕР"         // Замените на ваш третий трек-номер
};

// Интервалы
const unsigned long FIRST_CHECK_DELAY = 30000UL; // 30 секунд
const unsigned long REGULAR_CHECK_INTERVAL = 2UL * 60UL * 60UL * 1000UL; // 2 часа

// Таймеры
unsigned long lastCheckMillis = 0;
bool firstCheckDone = false;

// Максимум попыток запроса при ошибках для одного трек-номера
const int MAX_RETRIES = 5;
const unsigned long RETRY_DELAY = 5000; // 5 секунд между попытками

// Ключевые фразы, указывающие на то, что посылку можно забрать
const char* statusKeywords[] = {
  "прибыла в пункт выдачи",
  "посылка готова к выдаче",
  "можно забрать",
  "пришла в пункт выдачи",
  "ожидает получения",
  "пункт выдачи",
  "посылка доставлена",
  "Посылка доставлена"
};

// Подключаемся к WiFi с логом
void connectToWiFi() {
  Serial.printf("Подключение к WiFi сети '%s'...\n", ssid);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 60) { // ~30 секунд таймаут
      Serial.println("\nОшибка подключения к WiFi. Проверьте настройки и перезапустите устройство.");
      return;
    }
  }
  Serial.println("\nWiFi подключен.");
  Serial.print("IP адрес устройства: ");
  Serial.println(WiFi.localIP());
}

// Проверка содержимого страницы на ключевые слова
bool isPackageArrived(const String& html) {
  for (int i = 0; i < sizeof(statusKeywords) / sizeof(statusKeywords[0]); i++) {
    if (html.indexOf(statusKeywords[i]) >= 0) {
      Serial.printf("Найден признак того, что посылка доступна: \"%s\"\n", statusKeywords[i]);
      return true;
    }
  }
  return false;
}

// Функция запроса статуса трек-номера с ретраями
bool checkTrackingNumber(const char* trackingNumber) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi не подключен. Пропускаем проверку.");
    return false;
  }

  String url = String("https://gdeposylka.ru/courier/cainiao/tracking/") + trackingNumber;
  Serial.printf("Проверка трек-номера %s\n", trackingNumber);

  HTTPClient http;
  String responsePayload;
  int attempt = 0;
  bool success = false;
  bool arrived = false;

  while (attempt < MAX_RETRIES && !success) {
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      responsePayload = http.getString();
      success = true;
      Serial.printf("Успешно получен ответ для %s (код %d).\n", trackingNumber, httpCode);

      if (responsePayload.length() < 100) {
        Serial.println("Внимание: получен короткий ответ, возможно проблема с сайтом.");
      }

      arrived = isPackageArrived(responsePayload);
      
    } else {
      Serial.printf("Ошибка HTTP для %s: код %d. Попытка %d из %d.\n", trackingNumber, httpCode, attempt + 1, MAX_RETRIES);
      attempt++;
      http.end();
      if (attempt < MAX_RETRIES) {
        Serial.printf("Повторная попытка через %d мс...\n", RETRY_DELAY);
        delay(RETRY_DELAY);
      }
    }
  }

  if (!success) {
    Serial.printf("Не удалось получить корректный ответ для %s после %d попыток.\n", trackingNumber, MAX_RETRIES);
  }

  http.end();
  return arrived;
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Немного подождать Serial

  Serial.println("=== Запуск ESP32-C3 SuperMini. Проверка статусов посылок ===");

  // Инициируем пины светодиодов
  for (int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  connectToWiFi();

  lastCheckMillis = millis();
  firstCheckDone = false;
}

void loop() {
  unsigned long currentMillis = millis();

  if (!firstCheckDone) {
    if (currentMillis - lastCheckMillis >= FIRST_CHECK_DELAY) {
      Serial.println("=== Первая проверка посылок ===");

      for (int i = 0; i < 3; i++) {
        if (strlen(trackingNumbers[i]) == 0) continue;

        Serial.printf("Проверяем посылку %d: %s\n", i + 1, trackingNumbers[i]);
        bool arrived = checkTrackingNumber(trackingNumbers[i]);
        digitalWrite(ledPins[i], arrived ? HIGH : LOW);
        Serial.printf("LED %d -> %s\n", ledPins[i], arrived ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН");
      }

      Serial.println("=== Первая проверка завершена ===");
      lastCheckMillis = currentMillis;
      firstCheckDone = true;
    }
  } else {
    if (currentMillis - lastCheckMillis >= REGULAR_CHECK_INTERVAL) {
      Serial.println("=== Плановая проверка посылок ===");

      for (int i = 0; i < 3; i++) {
        if (strlen(trackingNumbers[i]) == 0) continue;

        Serial.printf("Проверяем посылку %d: %s\n", i + 1, trackingNumbers[i]);
        bool arrived = checkTrackingNumber(trackingNumbers[i]);
        digitalWrite(ledPins[i], arrived ? HIGH : LOW);
        Serial.printf("LED %d -> %s\n", ledPins[i], arrived ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН");
      }

      Serial.println("=== Плановая проверка завершена ===");
      lastCheckMillis = currentMillis;
    }
  }

  delay(1000);
}
