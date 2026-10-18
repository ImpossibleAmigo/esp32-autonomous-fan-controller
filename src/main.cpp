#include <Arduino.h>

// ============================================================================
// Конфігурація пінів (ESP32-S3)
// ============================================================================
constexpr uint8_t PIN_FAN_RELAY  = 4; // Керування реле або транзистором вентилятора
constexpr uint8_t PIN_LED_ACTIVE = 5; // Зелений LED: вентилятор працює
constexpr uint8_t PIN_LED_IDLE   = 6; // Синій/Червоний LED: режим очікування

// Якщо модуль реле керується низьким рівнем (Low-Level Trigger), зміни на LOW / HIGH
constexpr uint8_t RELAY_ON  = HIGH;
constexpr uint8_t RELAY_OFF = LOW;

// ============================================================================
// Налаштування інтервалів часу
// ============================================================================
// Закоментуй рядок нижче для переходу в реальний режим (1 год / 15 хв)
#define DEBUG_MODE

#ifdef DEBUG_MODE
    // Масштабований час для налагодження: цикл 16 с, робота 4 с (пропорція 4:1)
    constexpr uint64_t CYCLE_PERIOD_US = 16ULL * 1000000ULL;
    constexpr uint64_t FAN_RUN_TIME_US = 4ULL  * 1000000ULL;
#else
    // Промисловий режим: цикл 1 година (3600 с), робота 15 хвилин (900 с)
    constexpr uint64_t CYCLE_PERIOD_US = 60ULL * 60ULL * 1000000ULL;
    constexpr uint64_t FAN_RUN_TIME_US = 15ULL * 60ULL * 1000000ULL;
#endif

// Поточний стан вентилятора (volatile, оскільки змінюватиметься в перериваннях таймера)
volatile bool isFanRunning = false;

/**
 * @brief Встановлює фізичний стан реле та світлодіодів індикації
 */
void applyHardwareState(bool fanActive) {
    digitalWrite(PIN_FAN_RELAY,  fanActive ? RELAY_ON : RELAY_OFF);
    digitalWrite(PIN_LED_ACTIVE, fanActive ? HIGH : LOW);
    digitalWrite(PIN_LED_IDLE,   fanActive ? LOW  : HIGH);
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_FAN_RELAY,  OUTPUT);
    pinMode(PIN_LED_ACTIVE, OUTPUT);
    pinMode(PIN_LED_IDLE,   OUTPUT);

    // Початковий стан — вимкнено (очікування)
    applyHardwareState(false);
}

void loop() {
    // Цикл залишається вільним
}