#include "rec_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "esp_log.h"
#include "esp_check.h"

#include "driver/sdspi_host.h"
#include "driver/sdmmc_host.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"

#include "sensecap-watcher.h"
#include "rec_audio.h"

#define REC_STORAGE_USB_ATTACHED_LEVEL     0
#define REC_STORAGE_PCM_STREAM_SIZE        (64 * 1024)
#define REC_STORAGE_PCM_TRIGGER_LEVEL      (320 * sizeof(int16_t))
#define REC_STORAGE_WRITER_STACK_SIZE      6144
#define REC_STORAGE_WRITER_PRIO            9
#define REC_STORAGE_MAX_PATH               160
#define REC_STORAGE_SEGMENT_MAX_BYTES      (REC_AUDIO_SAMPLE_RATE * (REC_AUDIO_BITS_PER_SAMPLE / 8) * REC_STORAGE_SEGMENT_SECONDS)

static const char *TAG = "rec_storage";

typedef struct __attribute__((packed)) {
    char riff[4];
    uint32_t chunk_size;
    char wave[4];
    char fmt[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t subchunk2_size;
} rec_wav_header_t;

static bool s_initialized;
static bool s_error;
static bool s_recording;
static bool s_usb_exposed;
static bool s_tinyusb_ready;
static uint32_t s_next_file_index = 1;
static uint32_t s_dropped_pcm_frames;

static sdmmc_card_t *s_card;
static sdspi_dev_handle_t s_sdspi_handle = -1;
static StreamBufferHandle_t s_pcm_stream;
static TaskHandle_t s_writer_task;
static FILE *s_wav_file;
static uint32_t s_wav_pcm_bytes;

static esp_err_t rec_storage_prepare_dir(void)
{
    int ret = mkdir(REC_STORAGE_RECORDINGS_DIR, 0775);
    if (ret == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "mkdir %s failed, errno=%d", REC_STORAGE_RECORDINGS_DIR, errno);
    return ESP_FAIL;
}

static void rec_storage_scan_next_index(void)
{
    s_next_file_index = 1;
    DIR *dir = opendir(REC_STORAGE_RECORDINGS_DIR);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        unsigned int idx = 0;
        char ext[8] = {0};
        if (sscanf(entry->d_name, "REC_%u.%7s", &idx, ext) == 2) {
            if ((strcmp(ext, "WAV") == 0 || strcmp(ext, "wav") == 0) && idx >= s_next_file_index) {
                s_next_file_index = idx + 1;
            }
        }
    }
    closedir(dir);
}

static esp_err_t rec_storage_write_wav_header(FILE *file, uint32_t pcm_bytes)
{
    rec_wav_header_t hdr = {
        .riff = {'R', 'I', 'F', 'F'},
        .chunk_size = pcm_bytes + sizeof(rec_wav_header_t) - 8,
        .wave = {'W', 'A', 'V', 'E'},
        .fmt = {'f', 'm', 't', ' '},
        .subchunk1_size = 16,
        .audio_format = 1,
        .num_channels = REC_AUDIO_CHANNELS,
        .sample_rate = REC_AUDIO_SAMPLE_RATE,
        .byte_rate = REC_AUDIO_SAMPLE_RATE * REC_AUDIO_CHANNELS * (REC_AUDIO_BITS_PER_SAMPLE / 8),
        .block_align = REC_AUDIO_CHANNELS * (REC_AUDIO_BITS_PER_SAMPLE / 8),
        .bits_per_sample = REC_AUDIO_BITS_PER_SAMPLE,
        .data = {'d', 'a', 't', 'a'},
        .subchunk2_size = pcm_bytes,
    };

    if (fseek(file, 0, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    if (fwrite(&hdr, 1, sizeof(hdr), file) != sizeof(hdr)) {
        return ESP_FAIL;
    }
    fflush(file);
    (void)fsync(fileno(file));
    return ESP_OK;
}

static esp_err_t rec_storage_close_wav_file(void)
{
    if (s_wav_file == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = rec_storage_write_wav_header(s_wav_file, s_wav_pcm_bytes);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "update wav header failed");
    }
    fclose(s_wav_file);
    s_wav_file = NULL;
    s_wav_pcm_bytes = 0;
    return ret;
}

static esp_err_t rec_storage_open_next_wav_file(void)
{
    char path[REC_STORAGE_MAX_PATH];
    (void)snprintf(path, sizeof(path), "%s/REC_%04lu.WAV", REC_STORAGE_RECORDINGS_DIR,
                   (unsigned long)s_next_file_index++);

    s_wav_file = fopen(path, "wb");
    if (s_wav_file == NULL) {
        ESP_LOGE(TAG, "open %s failed", path);
        return ESP_FAIL;
    }

    s_wav_pcm_bytes = 0;
    esp_err_t ret = rec_storage_write_wav_header(s_wav_file, 0);
    if (ret != ESP_OK) {
        fclose(s_wav_file);
        s_wav_file = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "recording file: %s", path);
    return ESP_OK;
}

static esp_err_t rec_storage_rotate_segment_if_needed(void)
{
    if (s_wav_pcm_bytes < REC_STORAGE_SEGMENT_MAX_BYTES) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(rec_storage_close_wav_file(), TAG, "close segment failed");
    ESP_RETURN_ON_ERROR(rec_storage_open_next_wav_file(), TAG, "open segment failed");
    return ESP_OK;
}

static esp_err_t rec_storage_write_pcm(const uint8_t *data, size_t size)
{
    if (s_wav_file == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fwrite(data, 1, size, s_wav_file) != size) {
        return ESP_FAIL;
    }
    s_wav_pcm_bytes += (uint32_t)size;
    return rec_storage_rotate_segment_if_needed();
}

static void rec_storage_writer_task(void *arg)
{
    (void)arg;
    uint8_t write_buf[2048];

    while (s_recording || xStreamBufferBytesAvailable(s_pcm_stream) > 0) {
        size_t n = xStreamBufferReceive(s_pcm_stream, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
        if (n == 0) {
            continue;
        }
        if (rec_storage_write_pcm(write_buf, n) != ESP_OK) {
            s_error = true;
            s_recording = false;
            break;
        }
    }

    if (rec_storage_close_wav_file() != ESP_OK) {
        s_error = true;
    }

    s_writer_task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t rec_storage_card_init(void)
{
    if (s_card != NULL) {
        return ESP_OK;
    }
    if (bsp_io_expander_init() == NULL) {
        return ESP_FAIL;
    }

    (void)bsp_exp_io_set_level(BSP_PWR_SDCARD, 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_RETURN_ON_ERROR(bsp_spi_bus_init(), TAG, "SPI2 init failed");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = BSP_SD_SPI_NUM;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = host.slot;
    slot_config.gpio_cs = BSP_SD_SPI_CS;
    slot_config.gpio_cd = SDSPI_SLOT_NO_CD;
    slot_config.gpio_wp = SDSPI_SLOT_NO_WP;

    esp_err_t ret = host.init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "sdspi host init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = sdspi_host_init_device(&slot_config, &s_sdspi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sdspi device init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_host_t host_for_card = host;
    host_for_card.slot = s_sdspi_handle;

    s_card = (sdmmc_card_t *)calloc(1, sizeof(sdmmc_card_t));
    if (s_card == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = sdmmc_card_init(&host_for_card, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "sd card init failed: %s", esp_err_to_name(ret));
        free(s_card);
        s_card = NULL;
        (void)sdspi_host_remove_device(s_sdspi_handle);
        s_sdspi_handle = -1;
        return ret;
    }

    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

static esp_err_t rec_storage_tinyusb_init(void)
{
    if (s_tinyusb_ready) {
        return ESP_OK;
    }

    tinyusb_msc_sdmmc_config_t sdmmc_cfg = {
        .card = s_card,
        .mount_config = {
            .max_files = 10,
        },
    };
    ESP_RETURN_ON_ERROR(tinyusb_msc_storage_init_sdmmc(&sdmmc_cfg), TAG, "tinyusb msc sd init failed");
    ESP_RETURN_ON_ERROR(tinyusb_msc_storage_mount(REC_STORAGE_MOUNT_PATH), TAG, "initial msc mount failed");

    tinyusb_config_t tinyusb_cfg = {0};
    tinyusb_cfg.external_phy = false;
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tinyusb_cfg), TAG, "tinyusb driver install failed");

    s_tinyusb_ready = true;
    s_usb_exposed = false;
    return ESP_OK;
}

esp_err_t rec_storage_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(rec_storage_card_init(), TAG, "sd card bringup failed");
    ESP_RETURN_ON_ERROR(rec_storage_tinyusb_init(), TAG, "tinyusb bringup failed");
    ESP_RETURN_ON_ERROR(rec_storage_prepare_dir(), TAG, "recordings dir failed");

    rec_storage_scan_next_index();
    s_initialized = true;
    s_error = false;
    s_recording = false;
    return ESP_OK;
}

bool rec_storage_is_usb_attached(void)
{
    if (bsp_io_expander_init() == NULL) {
        return false;
    }
    uint8_t level = bsp_exp_io_get_level(BSP_PWR_VBUS_IN_DET);
    return level == REC_STORAGE_USB_ATTACHED_LEVEL;
}

bool rec_storage_is_usb_exposed(void)
{
    return s_usb_exposed;
}

bool rec_storage_is_recording(void)
{
    return s_recording;
}

bool rec_storage_has_error(void)
{
    return s_error;
}

esp_err_t rec_storage_start_recording(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_usb_exposed) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_recording) {
        return ESP_OK;
    }

    s_error = false;
    s_dropped_pcm_frames = 0;
    if (s_pcm_stream == NULL) {
        s_pcm_stream = xStreamBufferCreate(REC_STORAGE_PCM_STREAM_SIZE, REC_STORAGE_PCM_TRIGGER_LEVEL);
        if (s_pcm_stream == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_RETURN_ON_ERROR(rec_storage_prepare_dir(), TAG, "prepare dir failed");
    ESP_RETURN_ON_ERROR(rec_storage_open_next_wav_file(), TAG, "open wav failed");

    s_recording = true;
    BaseType_t ok = xTaskCreate(rec_storage_writer_task, "rec_writer", REC_STORAGE_WRITER_STACK_SIZE, NULL,
                                REC_STORAGE_WRITER_PRIO, &s_writer_task);
    if (ok != pdPASS) {
        s_recording = false;
        (void)rec_storage_close_wav_file();
        vStreamBufferDelete(s_pcm_stream);
        s_pcm_stream = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t rec_storage_stop_recording(void)
{
    if (!s_recording && s_writer_task == NULL) {
        return s_error ? ESP_FAIL : ESP_OK;
    }

    s_recording = false;
    while (s_writer_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    if (s_pcm_stream != NULL) {
        vStreamBufferDelete(s_pcm_stream);
        s_pcm_stream = NULL;
    }

    return s_error ? ESP_FAIL : ESP_OK;
}

void rec_storage_pcm_sink(const int16_t *samples, size_t sample_count, void *user_ctx)
{
    (void)user_ctx;
    if (!s_recording || s_pcm_stream == NULL || samples == NULL || sample_count == 0) {
        return;
    }

    const uint8_t *bytes = (const uint8_t *)samples;
    size_t total_bytes = sample_count * sizeof(int16_t);
    size_t sent = xStreamBufferSend(s_pcm_stream, bytes, total_bytes, 0);
    if (sent < total_bytes) {
        // Non-blocking drop on overflow keeps capture task real-time.
        s_dropped_pcm_frames++;
        if ((s_dropped_pcm_frames % 50U) == 0U) {
            ESP_LOGW(TAG, "pcm overflow drops=%lu", (unsigned long)s_dropped_pcm_frames);
        }
    }
}

esp_err_t rec_storage_enter_usb_exposed(void)
{
    if (!s_tinyusb_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_recording) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_usb_exposed) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(tinyusb_msc_storage_unmount(), TAG, "unmount for usb exposure failed");
    s_usb_exposed = true;
    return ESP_OK;
}

esp_err_t rec_storage_exit_usb_exposed(void)
{
    if (!s_tinyusb_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_usb_exposed) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(tinyusb_msc_storage_mount(REC_STORAGE_MOUNT_PATH), TAG, "mount after usb detach failed");
    ESP_RETURN_ON_ERROR(rec_storage_prepare_dir(), TAG, "prepare dir after usb detach failed");
    rec_storage_scan_next_index();
    s_usb_exposed = false;
    return ESP_OK;
}
