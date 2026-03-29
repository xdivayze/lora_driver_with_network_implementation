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
#include "sx127x_rx_utils.h"
#include "network_data_operations.h"
#include "rx_packet_handler.h"
#include "packet.h"

#define TAG           "BURST"
#define LORA_SPI_HOST SPI2_HOST
#define SENDER_ADDR   0x0001
#define RECEIVER_ADDR 0x0002
#define ACK_ID        0x01

static const char *burst_data = "hello burst test";
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

static int64_t get_time_us(void)     { return esp_timer_get_time(); }
static void    delay_ms(uint64_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

// -----------------------------------------------------------------------
// Hardware init
// -----------------------------------------------------------------------

static void spi_init(void)
{
    ESP_LOGI(TAG, "SPI pins — NSS:%d SCK:%d MISO:%d MOSI:%d",
             CONFIG_LORA_PIN_NSS, CONFIG_LORA_PIN_SCK,
             CONFIG_LORA_PIN_MISO, CONFIG_LORA_PIN_MOSI);

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
// Burst logic
// -----------------------------------------------------------------------

static void sender_task(void *arg)
{
    int data_len = strlen(burst_data);
    size_t npackets = (data_len + payload_length_max - 1) / payload_length_max;
    size_t total = npackets + 2; // BEGIN + data packets + END

    ESP_LOGI(TAG, "sending \"%s\" (%d bytes, %zu packets)", burst_data, data_len, npackets);

    packet **p_buf = malloc(total * sizeof(packet *));
    if (!p_buf)
    {
        ESP_LOGE(TAG, "out of memory");
        return;
    }

    if (data_to_packet_array(p_buf, (uint8_t *)burst_data, data_len,
                             RECEIVER_ADDR, SENDER_ADDR, ACK_ID, true) != 0)
    {
        ESP_LOGE(TAG, "failed to build packet array");
        free(p_buf);
        return;
    }

    for (size_t i = 1; i <= npackets; i++)
        ESP_LOGI(TAG, "sending packet %zu/%zu", i, npackets);

    if (send_burst(p_buf, total) != SX_OK)
    {
        ESP_LOGE(TAG, "burst send failed");
        free(p_buf);
        return;
    }

    free(p_buf);
    ESP_LOGI(TAG, "all %zu packets acknowledged", npackets);
    printf("burst sent\n");
    vTaskDelete(NULL);
}

static void run_sender(void)
{
    xTaskCreate(sender_task, "sender", 8192, NULL, 1, NULL);
}

static void on_burst_complete(packet **p_arr, int n)
{
    size_t total_len = 0;
    for (int i = 0; i < n; i++)
        total_len += p_arr[i]->payload_length;

    uint8_t *buf = malloc(total_len + 1);
    if (!buf)
    {
        ESP_LOGE(TAG, "out of memory in capture handler");
        return;
    }

    packet_array_to_data(p_arr, buf, n);
    buf[total_len] = '\0';

    ESP_LOGI(TAG, "received %d packets, %zu bytes", n, total_len);
    printf("burst complete: %s\n", (char *)buf);
    free(buf);
}

static rx_handler_ctx_t rx_handler;

static void rx_task(void *arg)
{
    rx_handler_ctx_t *handler = (rx_handler_ctx_t *)arg;
    packet *rx_p = malloc(sizeof(packet));
    rx_p->payload = NULL;

    sx1278_switch_mode(MODE_LORA | MODE_RX_CONTINUOUS);

    ESP_LOGI(TAG, "listening for burst...");

    while (1)
    {
        sx1278_clear_irq();
        sx1278_switch_mode(MODE_LORA | MODE_RX_CONTINUOUS);

        if (poll_for_irq_flag_no_timeout(1, (1 << 6), false) != SX_OK)
            continue;

        if (read_last_packet(rx_p) != SX_OK)
            continue;

        sx_1278_send_packet(ack_packet(rx_p->src_address, rx_p->dest_address,
                                       rx_p->ack_id, rx_p->sequence_number), false);

        rx_handler_return result = rx_packet_handler(handler, rx_p);

        switch (result)
        {
        case CAPTURE_BEGIN:
            ESP_LOGI(TAG, "burst begin received");
            break;
        case DATA_PACKET_CAPTURED:
            ESP_LOGI(TAG, "packet %zu/%d captured", handler->captured_n, (int)PACKET_CAPTURE_MAX_COUNT);
            break;
        case CAPTURE_END:
            ESP_LOGI(TAG, "burst end received");
            goto done;
        default:
            break;
        }
    }

done:
    free_packet(rx_p);
    vTaskDelete(NULL);
}

static void run_receiver(void)
{
    rx_handler_init(&rx_handler, NULL, on_burst_complete, RECEIVER_ADDR, SENDER_ADDR);
    xTaskCreate(rx_task, "rx", 8192, &rx_handler, 1, NULL);
}

// -----------------------------------------------------------------------
// app_main
// -----------------------------------------------------------------------

static void app_task(void *arg)
{
    spi_init();
    vTaskDelay(pdMS_TO_TICKS(100));

    spi_port_t         spi;
    logger_ctx_t       logger = {0};
    sx127x_timer_ctx_t timer;

    spi_port_init(&spi, lora_spi_write, lora_spi_read, (void *)spi_handle);
    sx127x_timer_init(&timer, get_time_us, delay_ms);

    sx127x_err_t init_ret = initialize_sx_1278(spi, logger, timer);
    if (init_ret != SX_OK)
    {
        printf("init failed: %d\n", (int)init_ret);
        vTaskDelete(NULL);
        return;
    }
    printf("sx1278 init ok\n");

#if defined(CONFIG_LORA_ROLE_SENDER)
    run_sender();
#else
    run_receiver();
#endif
    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(app_task, "app", 8192, NULL, 1, NULL);
}
