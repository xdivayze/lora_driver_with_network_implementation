#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "sx127x_config.h"
#include "sx127x_driver.h"
#include "sx127x_utils.h"

#define TAG        "PINGPONG"
#define LORA_SPI_HOST   SPI2_HOST
#define PING_STR   "ping"
#define PONG_STR   "pong"
#define ROUNDS     5

static spi_device_handle_t spi_handle;

// -----------------------------------------------------------------------
// SPI transactions
// -----------------------------------------------------------------------

static esp_err_t esp_spi_write(spi_device_handle_t spi, const uint8_t addr, const uint8_t *data, int len)
{
    esp_err_t ret = spi_device_acquire_bus(spi, portMAX_DELAY);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "couldnt acquire bus\n");
        return ret;
    }
    uint8_t *cmd = NULL;
    uint8_t *data_dma = heap_caps_malloc(len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!data_dma)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memcpy(data_dma, data, len);
    if (len <= 0)
        return ESP_OK;

    cmd = heap_caps_malloc(1, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!cmd)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memset(cmd, addr | 0b10000000, 1);
    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = data_dma;

    ret = spi_device_transmit(spi, &t0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t0:%s \n", esp_err_to_name(ret));
        goto cleanup;
    }
    ret = spi_device_transmit(spi, &t1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t1:%s \n", esp_err_to_name(ret));
        goto cleanup;
    }

cleanup:
    if (cmd)
        free(cmd);
    if (data_dma)
        free(data_dma);
    spi_device_release_bus(spi);
    return ret;
}

static esp_err_t esp_spi_read(spi_device_handle_t spi, const uint8_t addr, uint8_t *data, int len)
{
    if (len <= 0)
        return ESP_OK;

    esp_err_t ret;
    uint8_t *ret_data_dma = NULL;

    uint8_t *cmd = NULL;
    uint8_t *dummy = heap_caps_malloc(len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!dummy)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    cmd = heap_caps_malloc(1, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!cmd)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    memset(cmd, addr & 0b01111111, 1);

    spi_device_acquire_bus(spi, portMAX_DELAY);

    spi_transaction_t t0 = {0};
    t0.length = 8;
    t0.tx_buffer = cmd;
    t0.flags = SPI_TRANS_CS_KEEP_ACTIVE;

    ret = spi_device_transmit(spi, &t0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t0: ret: %s\n", esp_err_to_name(ret));
        goto cleanup;
    }

    memset(dummy, 0x00, len);
    ret_data_dma = heap_caps_malloc(len, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!ret_data_dma)
    {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    spi_transaction_t t1 = {0};
    t1.length = len * 8;
    t1.tx_buffer = dummy;
    t1.rx_buffer = ret_data_dma;
    ret = spi_device_transmit(spi, &t1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "t1: ret: %s\n", esp_err_to_name(ret));
        goto cleanup;
    }

    memcpy(data, ret_data_dma, len);

cleanup:
    if (ret_data_dma)
        free(ret_data_dma);
    if (dummy)
        free(dummy);
    if (cmd)
        free(cmd);
    spi_device_release_bus(spi);
    return ret;
}

// -----------------------------------------------------------------------
// Library callbacks
// -----------------------------------------------------------------------

static sx127x_err_t lora_spi_write(const void *handle, uint8_t reg, const uint8_t *data, int len)
{
    esp_err_t ret = esp_spi_write((spi_device_handle_t)handle, reg, data, len);
    return ret == ESP_OK ? SX_OK : SX_INVALID_STATE;
}

static sx127x_err_t lora_spi_read(const void *handle, uint8_t reg, uint8_t *data, int len)
{
    esp_err_t ret = esp_spi_read((spi_device_handle_t)handle, reg, data, len);
    return ret == ESP_OK ? SX_OK : SX_INVALID_STATE;
}

static int64_t get_time_us(void)       { return esp_timer_get_time(); }
static void    delay_ms(uint64_t ms)   { vTaskDelay(pdMS_TO_TICKS(ms)); }

// -----------------------------------------------------------------------
// Hardware init
// -----------------------------------------------------------------------

static void spi_init(void)
{
    spi_bus_config_t bus = {
        .mosi_io_num   = CONFIG_LORA_PIN_MOSI,
        .miso_io_num   = CONFIG_LORA_PIN_MISO,
        .sclk_io_num   = CONFIG_LORA_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1000000,
        .mode           = 0,
        .spics_io_num   = CONFIG_LORA_PIN_NSS,
        .queue_size     = 1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LORA_SPI_HOST, &bus, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(LORA_SPI_HOST, &dev, &spi_handle));
}

// -----------------------------------------------------------------------
// Pingpong logic
// -----------------------------------------------------------------------

static void run_sender(void)
{
    uint8_t buf[64];
    size_t  len;

    for (int i = 0; i < ROUNDS; i++)
    {
        if (sx1278_send_payload((uint8_t *)PING_STR, strlen(PING_STR), 1) != SX_OK)
        {
            ESP_LOGE(TAG, "failed to send ping");
            return;
        }
        ESP_LOGI(TAG, "ping sent");

        if (poll_for_irq_flag(PHY_TIMEOUT_MSEC, 3, (1 << 6), false) != SX_OK)
        {
            ESP_LOGE(TAG, "timed out waiting for pong");
            return;
        }
        if (sx1278_read_last_payload(buf, &len) != SX_OK)
        {
            ESP_LOGE(TAG, "failed to read pong");
            return;
        }
        if (len != strlen(PONG_STR) || memcmp(buf, PONG_STR, len) != 0)
        {
            ESP_LOGE(TAG, "unexpected payload");
            return;
        }
        ESP_LOGI(TAG, "pong received");
    }

    printf("test passed\n");
}

static void run_receiver(void)
{
    uint8_t buf[64];
    size_t  len;

    if (sx1278_switch_mode(MODE_LORA | MODE_RX_CONTINUOUS) != SX_OK)
    {
        ESP_LOGE(TAG, "failed to enter rx mode");
        return;
    }

    for (int i = 0; i < ROUNDS; i++)
    {
        if (poll_for_irq_flag(PHY_TIMEOUT_MSEC * 10, 3, (1 << 6), false) != SX_OK)
        {
            ESP_LOGE(TAG, "timed out waiting for ping");
            return;
        }
        if (sx1278_read_last_payload(buf, &len) != SX_OK)
        {
            ESP_LOGE(TAG, "failed to read ping");
            return;
        }
        if (len != strlen(PING_STR) || memcmp(buf, PING_STR, len) != 0)
        {
            ESP_LOGE(TAG, "unexpected payload");
            return;
        }
        ESP_LOGI(TAG, "ping received");

        if (sx1278_send_payload((uint8_t *)PONG_STR, strlen(PONG_STR), 1) != SX_OK)
        {
            ESP_LOGE(TAG, "failed to send pong");
            return;
        }
        ESP_LOGI(TAG, "pong sent");
    }
}

// -----------------------------------------------------------------------
// app_main
// -----------------------------------------------------------------------

void app_main(void)
{
    spi_init();

    spi_port_t         spi;
    logger_ctx_t       logger = {0};
    sx127x_timer_ctx_t timer;

    spi_port_init(&spi, lora_spi_write, lora_spi_read, (void *)spi_handle);
    sx127x_timer_init(&timer, get_time_us, delay_ms);

    if (initialize_sx_1278(spi, logger, timer) != SX_OK)
    {
        printf("init failed\n");
        return;
    }
    printf("sx1278 init ok\n");

#if defined(CONFIG_LORA_ROLE_SENDER)
    run_sender();
#else
    run_receiver();
#endif
}
