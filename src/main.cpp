#include <Arduino.h>
#include "esp_timer.h"
#include "esp_task_wdt.h"

// ============================================================================
// Конфігурація пінів (ESP32-S3)
// ============================================================================
constexpr uint8_t PIN_FAN_RELAY  = 4; // Керування реле або транзистором
constexpr uint8_t PIN_LED_ACTIVE = 5; // Зелений LED: вентилятор увімкнено
constexpr uint8_t PIN_LED_IDLE   = 6; // Синій/Червоний LED: режим очікування

constexpr uint8_t RELAY_ON  = HIGH;
constexpr uint8_t RELAY_OFF = LOW;

// ============================================================================
// Налаштування інтервалів часу та Watchdog
// ============================================================================
#define DEBUG_MODE

#ifdef DEBUG_MODE
    constexpr uint64_t CYCLE_PERIOD_US = 16ULL * 1000000ULL; // 16 секунд
    constexpr uint64_t FAN_RUN_TIME_US = 4ULL  * 1000000ULL; // 4 секунди
#else
    constexpr uint64_t CYCLE_PERIOD_US = 60ULL * 60ULL * 1000000ULL; // 1 година
    constexpr uint64_t FAN_RUN_TIME_US = 15ULL * 60ULL * 1000000ULL; // 15 хвилин
#endif

constexpr uint32_t WDT_TIMEOUT_SEC = 5; // Таймаут сторожового таймера (5 с)

// Перевірка коректності конфігурації на етапі компіляції
static_assert(FAN_RUN_TIME_US < CYCLE_PERIOD_US, "Fan run time must be strictly less than cycle period!");

volatile bool isFanRunning = false;

esp_timer_handle_t cycleTimer   = nullptr;
esp_timer_handle_t shutoffTimer = nullptr;

/**
 * @brief Допоміжна функція логування з міткою часу (в секундах від старту)
 */
void logState(const char* tag, const char* message) {
    double uptimeSec = esp_timer_get_time() / 1000000.0;
    Serial.printf("[%8.2f s] [%s] %s\n", uptimeSec, tag, message);
}

void applyHardwareState(bool fanActive) {
    digitalWrite(PIN_FAN_RELAY,  fanActive ? RELAY_ON : RELAY_OFF);
    digitalWrite(PIN_LED_ACTIVE, fanActive ? HIGH : LOW);
    digitalWrite(PIN_LED_IDLE,   fanActive ? LOW  : HIGH);
}

/**
 * @brief Callback одноразового таймера: вимикає вентилятор
 */
void onShutoffTimerCallback(void* arg) {
    if (!isFanRunning) {
        logState("WARN", "Спроба вимкнути вентилятор, який вже вимкнений.");
        return;
    }

    isFanRunning = false;
    applyHardwareState(false);
    logState("FAN_OFF", "Вентилятор ВИМКНЕНО. Перехід у режим очікування (LED: IDLE).");
}

/**
 * @brief Callback періодичного таймера: вмикає вентилятор із захистом від повторного пуску
 */
void onCycleTimerCallback(void* arg) {
    if (isFanRunning || esp_timer_is_active(shutoffTimer)) {
        logState("GUARD", "Захист спрацював: вентилятор вже працює! Повторний запуск заблоковано.");
        return;
    }

    isFanRunning = true;
    applyHardwareState(true);
    logState("FAN_ON", "Вентилятор УВІМКНЕНО (LED: ACTIVE). Запущено відлік до вимкнення.");

    ESP_ERROR_CHECK(esp_timer_start_once(shutoffTimer, FAN_RUN_TIME_US));
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

    // Перший цикл запускаємо одразу при старті пристрою
    onCycleTimerCallback(nullptr);
    ESP_ERROR_CHECK(esp_timer_start_periodic(cycleTimer, CYCLE_PERIOD_US));
}

void initWatchdog() {
    // Налаштування Task Watchdog Timer (сумісно з ESP-IDF v4.x та v5.x у Arduino)
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true); // true = паніка і перезавантаження МК при зависанні
    esp_task_wdt_add(NULL);                   // Додаємо поточний loopTask під нагляд WDT
    logState("WDT", "Сторожовий таймер (Watchdog) активовано (таймаут: 5 с).");
}

void setup() {
    Serial.begin(115200);
    uint32_t startWait = millis();
    while (!Serial && (millis() - startWait < 2000)) {}

    Serial.println("\n=======================================================");
    Serial.println("  ESP32-S3 Autonomous Exhaust Fan Controller Initialized");
    Serial.printf("  Період циклу: %llu с | Тривалість роботи: %llu с\n",
                  CYCLE_PERIOD_US / 1000000ULL, FAN_RUN_TIME_US / 1000000ULL);
    Serial.println("=======================================================");

    pinMode(PIN_FAN_RELAY,  OUTPUT);
    pinMode(PIN_LED_ACTIVE, OUTPUT);
    pinMode(PIN_LED_IDLE,   OUTPUT);

    applyHardwareState(false);
    initWatchdog();
    initTimers();
}

void loop() {
    // Керування вентилятором працює на апаратних/системних таймерах повністю незалежно від loop().
    // У loop() лише скидаємо сторожовий таймер (Watchdog) для підтвердження життєздатності системи.
    esp_task_wdt_reset();
}