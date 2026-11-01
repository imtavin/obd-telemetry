#include <cstdint>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
constexpr char kLogTag[] = "app";
constexpr uint32_t kHeartbeatPeriodMilliseconds = 5000;
constexpr uint32_t kApplicationTaskStackSize = 2048;
constexpr UBaseType_t kApplicationTaskPriority = 1;

void application_task(void *)
{
    ESP_LOGI(kLogTag, "Application task is running");

    while (true)
    {
        ESP_LOGI(kLogTag, "Firmware baseline is alive");
        vTaskDelay(pdMS_TO_TICKS(kHeartbeatPeriodMilliseconds));
    }
}
} // namespace

extern "C" void app_main()
{
    ESP_LOGI(kLogTag, "Vehicle telemetry firmware starting");

    const BaseType_t task_created = xTaskCreate(
        application_task,
        "application_task",
        kApplicationTaskStackSize,
        nullptr,
        kApplicationTaskPriority,
        nullptr);

    if (task_created != pdPASS)
    {
        ESP_LOGE(kLogTag, "Failed to create application task");
    }
}
