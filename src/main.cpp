#include <Arduino.h>
#include "esp_timer.h"

// ============================================================================
// Конфігурація пінів (ESP32-S3)
// ============================================================================
constexpr uint8_t PIN_FAN_RELAY  = 4;
constexpr uint8_t PIN_LED_ACTIVE = 5;
constexpr uint8_t PIN_LED_IDLE   = 6;

constexpr uint8_t RELAY_ON  = HIGH;
constexpr uint8_t RELAY_OFF = LOW;

// ============================================================================
// Налаштування інтервалів часу
// ============================================================================
#define DEBUG_MODE

#ifdef DEBUG_MODE
    constexpr uint64_t CYCLE_PERIOD_US = 16ULL * 1000000ULL; // 16 секунд
    constexpr uint64_t FAN_RUN_TIME_US = 4ULL  * 1000000ULL; // 4 секунди
#else
    constexpr uint64_t CYCLE_PERIOD_US = 60ULL * 60ULL * 1000000ULL; // 1 година
    constexpr uint64_t FAN_RUN_TIME_US = 15ULL * 60ULL * 1000000ULL; // 15 хвилин
#endif

volatile bool isFanRunning = false;

// Дескриптори системних таймерів ESP-IDF
esp_timer_handle_t cycleTimer   = nullptr;
esp_timer_handle_t shutoffTimer = nullptr;

void applyHardwareState(bool fanActive) {
    digitalWrite(PIN_FAN_RELAY,  fanActive ? RELAY_ON : RELAY_OFF);
    digitalWrite(PIN_LED_ACTIVE, fanActive ? HIGH : LOW);
    digitalWrite(PIN_LED_IDLE,   fanActive ? LOW  : HIGH);
}

/**
 * @brief Callback одноразового таймера: вимикає вентилятор після відпрацювання часу
 */
void IRAM_ATTR onShutoffTimerCallback(void* arg) {
    isFanRunning = false;
    applyHardwareState(false);
}

/**
 * @brief Callback періодичного таймера: запускає вентилятор і планує його вимкнення
 */
void IRAM_ATTR onCycleTimerCallback(void* arg) {
    isFanRunning = true;
    applyHardwareState(true);

    // Запускаємо одноразовий таймер на час роботи вентилятора
    esp_timer_start_once(shutoffTimer, FAN_RUN_TIME_US);
}

void initTimers() {
    const esp_timer_create_args_t shutoffTimerArgs = {
        .callback = &onShutoffTimerCallback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fan_shutoff_timer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&shutoffTimerArgs, &shutoffTimer));

    const esp_timer_create_args_t cycleTimerArgs = {
        .callback = &onCycleTimerCallback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fan_cycle_timer",
        .skip_unhandled_events = true
    };
    ESP_ERROR_CHECK(esp_timer_create(&cycleTimerArgs, &cycleTimer));

    // Перший запуск одразу при старті, далі — строго за періодом CYCLE_PERIOD_US
    onCycleTimerCallback(nullptr);
    ESP_ERROR_CHECK(esp_timer_start_periodic(cycleTimer, CYCLE_PERIOD_US));
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_FAN_RELAY,  OUTPUT);
    pinMode(PIN_LED_ACTIVE, OUTPUT);
    pinMode(PIN_LED_IDLE,   OUTPUT);

    applyHardwareState(false);
    initTimers();
}

void loop() {
    // Керування вентилятором повністю автономне і не використовує loop()
}