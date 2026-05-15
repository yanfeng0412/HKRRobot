#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

// Include your project headers (adjust paths if different)
#include "es8388_audio_codec.h"
#include "audio_processor.h"
#include "private_config.h" // For API Key

static const char *TAG = "TTS_SST_DEMO";

// --- Hardware mapping (CORRECTED) ---
#define I2C_MASTER_PORT I2C_NUM_0
#define I2C_SDA_GPIO GPIO_NUM_1   // I2C SDA on IO1
#define I2C_SCL_GPIO GPIO_NUM_2   // I2C SCL on IO2

#define ES8388_I2C_ADDR 0x10 // ES8388 I2C address
#define XL9555_I2C_ADDR 0x20 // XL9555 IO expander I2C address

// I2S pin mapping (CORRECTED)
#define I2S_MCLK  GPIO_NUM_38  // IO38 - Master Clock
#define I2S_SCK   GPIO_NUM_4   // IO4  - Bit Clock (BCLK)
#define I2S_WS    GPIO_NUM_5   // IO5  - Word Select (LRCK)
#define I2S_SDIN  GPIO_NUM_6   // IO6  - Data In (microphone input to ESP32)
#define I2S_SDOUT GPIO_NUM_7   // IO7  - Data Out (speaker output from ESP32)

// Speaker Enable on IO41 (direct GPIO, not via XL9555)
#define SPK_EN_GPIO GPIO_NUM_41

// MD8002A SPK EN was via XL9555 IO3, but now using direct GPIO
#define XL9555_SPK_EN_PIN 3  // Keep for compatibility, but not used

// Speaker amplifier GPIO control (using direct IO41)
#define PA_GPIO SPK_EN_GPIO

// Simple I2C helper init
static esp_err_t i2c_master_init_simple(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint32_t clk_hz = 100000) {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = clk_hz;
    esp_err_t err = i2c_param_config(port, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(port, conf.mode, 0, 0, 0);
}

// Very small XL9555 helper: configure a single output bit (NOTE: adjust to actual XL9555 register map)
static esp_err_t xl9555_set_output_bit(i2c_port_t port, uint8_t dev_addr, uint8_t bit, bool value) {
    // This is a placeholder implementation. XL9555 datasheet defines which register controls outputs.
    // Many expanders use an output register (e.g. 0x01). Adjust register addresses and data format per datasheet.
    uint8_t reg = 0x01; // OUTPUT port register (placeholder)
    uint8_t out_mask = (1 << bit);
    // Read current output register
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    uint8_t current = 0;
    i2c_master_read_byte(cmd, &current, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "xl9555 read failed: 0x%x", ret);
        // try to just write desired value anyway
        current = 0;
    }

    if (value) current |= out_mask; else current &= ~out_mask;

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, current, true);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "xl9555 write failed: 0x%x", ret);
    }
    return ret;
}

// I2C Scanner to detect devices on the bus
static void i2c_scan_bus(i2c_port_t port) {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Found device at address: 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "  No I2C devices found!");
    } else {
        ESP_LOGI(TAG, "  Total devices found: %d", found);
    }
}

// Global codec object pointer (using your C++ wrapper)
static Es8388AudioCodec *g_codec = nullptr;
static AudioProcessor *g_processor = nullptr;

// Forward declaration
static void tts_play_sine(float freq_hz, int duration_ms);

// Simple TTS: generate sine wave at given frequency and play via codec
static void tts_perform_synthesis(const char* text) {
    if (!g_codec) {
        ESP_LOGE(TAG, "Codec not initialized for TTS");
        return;
    }
    ESP_LOGI(TAG, "TTS request for text: \"%s\"", text);

    // --- API WINDOW: GOOGLE TTS ---
    // 1. Make an HTTP POST request to Google Cloud TTS API with the `text`.
    //    - You will need to handle authentication (API Key or OAuth2).
    //    - The request URL will look like: "https://texttospeech.googleapis.com/v1/text:synthesize?key=YOUR_API_KEY"
    //    - Example: https://cloud.google.com/text-to-speech/docs/reference/rest/v1/text/synthesize

    // 2. Receive the audio data.
    //    - The API will respond with audio content, likely base64-encoded in the JSON response.
    //    - You need to parse the JSON and decode the base64 string into a binary audio buffer (e.g., uint8_t*).

    // 3. Decode and play the audio.
    //    - The audio from Google might be in formats like MP3 or raw PCM. If it's not LINEAR16 (int16_t), you'll need a decoder.
    //    - For this example, we assume you have decoded it into a `std::vector<int16_t> pcm_data`.
    //    - You would then write this data to the codec:
    //      g_codec->EnableOutput(true);
    //      g_codec->Write(pcm_data.data(), pcm_data.size());
    //      g_codec->EnableOutput(false);
    // --- END API WINDOW ---

    // You can now access your key using the GOOGLE_API_KEY macro
    ESP_LOGI(TAG, "Using API Key starting with: %.5s...", GOOGLE_API_KEY);

    // For demonstration, we just play a tone to simulate the TTS output.
    ESP_LOGI(TAG, "Simulating TTS playback with a tone.");
    tts_play_sine(880.0f, 500); // Play a short tone
}

static void tts_play_sine(float freq_hz, int duration_ms) {
    if (!g_codec) {
        ESP_LOGE(TAG, "Codec not initialized");
        return;
    }
    const int sample_rate = 16000;
    const int total_samples = (sample_rate * duration_ms) / 1000;
    const int chunk = 256;

    std::vector<int16_t> buffer(chunk);
    float phase = 0.0f;
    float phase_inc = 2.0f * M_PI * freq_hz / sample_rate;

    g_codec->EnableOutput(true);

    int sent = 0;
    while (sent < total_samples) {
        for (int i = 0; i < chunk; ++i) {
            float v = sinf(phase);
            buffer[i] = (int16_t)(v * 16000.0f); // amplitude
            phase += phase_inc;
            if (phase > (2.0f * M_PI)) phase -= 2.0f * M_PI;
        }
        g_codec->Write(buffer.data(), chunk);
        sent += chunk;
    }

    // small drain and disable
    vTaskDelay(pdMS_TO_TICKS(50));
    g_codec->EnableOutput(false);
}

// Task handle and flag to control the SST task
static TaskHandle_t sst_task_handle = NULL;
static volatile bool sst_task_running = false;
// SST: read from codec and feed into AFE processor
static void sst_capture_task(void *arg) {
    (void)arg;
    if (!g_codec || !g_processor) {
        ESP_LOGE(TAG, "codec or processor not initialized");
        vTaskDelete(NULL);
        return;
    }

    sst_task_running = true;
    g_codec->EnableInput(true);
    g_processor->Start();

    ESP_LOGI(TAG, "=== Starting audio capture - speak into microphone ===");

    const int chunk = 256;
    std::vector<int16_t> inbuf(chunk);
    int frame_count = 0;

    while (true) {
        if (!sst_task_running) {
            break; // Exit loop if flag is cleared
        }
        
        int samples_read = g_codec->Read(inbuf.data(), chunk);
        
        if (samples_read > 0) {
            // Calculate RMS (volume level)
            float sum_squares = 0;
            int16_t max_val = 0;
            for (int i = 0; i < samples_read; i++) {
                sum_squares += (float)(inbuf[i] * inbuf[i]);
                if (abs(inbuf[i]) > max_val) max_val = abs(inbuf[i]);
            }
            float rms = sqrtf(sum_squares / samples_read);
            
            frame_count++;
            // Print audio info every 50 frames (~0.8 seconds at 16kHz)
            if (frame_count % 50 == 0) {
                ESP_LOGI(TAG, "[AUDIO] Frame %d: RMS=%.1f, Peak=%d, Samples=%d %s", 
                         frame_count, rms, max_val, samples_read,
                         rms > 500 ? "<<< VOICE DETECTED" : "");
                // Show first 8 samples as raw data for debugging
                ESP_LOGI(TAG, "  Raw samples: [%d, %d, %d, %d, %d, %d, %d, %d]",
                         inbuf[0], inbuf[1], inbuf[2], inbuf[3],
                         inbuf[4], inbuf[5], inbuf[6], inbuf[7]);
            }
            
            // feed to AFE / audio processor
            g_processor->Input(inbuf);
        } else {
            ESP_LOGW(TAG, "No audio data read from codec");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Cleanup
    g_codec->EnableInput(false);
    ESP_LOGI(TAG, "SST capture task stopped.");
    sst_task_handle = NULL;
    vTaskDelete(NULL);
}

// Simple console task to control demo
static void demo_console_task(void *arg) {
    (void)arg;
    char line[128];
    while (true) {
        printf("\nCommands:\n");
        printf("  tts <text>  - Synthesize and play text (e.g., tts hello world)\n");
        printf("  sst start   - Start voice capture for STT\n");
        printf("  sst stop    - Stop voice capture\n");
        printf("  tone        - Play a test tone\n");
        printf("Enter command: ");

        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        // Trim newline
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "tts ", 4) == 0) {
            const char* text_to_speak = line + 4;
            tts_perform_synthesis(text_to_speak);
        } else if (strcmp(line, "sst start") == 0) {
            if (sst_task_handle) {
                ESP_LOGW(TAG, "SST task is already running.");
            } else {
                ESP_LOGI(TAG, "Starting SST capture task...");
                xTaskCreate(sst_capture_task, "sst_capture", 4096, NULL, 6, &sst_task_handle);
            }
        } else if (strcmp(line, "sst stop") == 0) {
            ESP_LOGI(TAG, "Stopping SST task...");
            sst_task_running = false; // Signal task to stop
        } else if (strcmp(line, "tone") == 0) {
            ESP_LOGI(TAG, "Playing test tone 440Hz for 2s");
            tts_play_sine(440.0f, 2000);
        } else {
            if (strlen(line) > 0) ESP_LOGW(TAG, "Unknown command: '%s'", line);
        }
    }
    vTaskDelete(NULL);
}

// Exported start function so main can call to start demo without duplicate app_main
extern "C" void start_tts_sst_demo(void) {
    ESP_LOGI(TAG, "TTS/SST demo starting");

    // 1) Init I2C
    ESP_ERROR_CHECK(i2c_master_init_simple(I2C_MASTER_PORT, I2C_SDA_GPIO, I2C_SCL_GPIO));
    ESP_LOGI(TAG, "I2C initialized on SDA=GPIO%d, SCL=GPIO%d", I2C_SDA_GPIO, I2C_SCL_GPIO);
    
    // 1.5) Scan I2C bus to find devices
    i2c_scan_bus(I2C_MASTER_PORT);

    // 2) Initialize speaker enable GPIO (IO41)
    if (SPK_EN_GPIO != GPIO_NUM_NC) {
        gpio_set_direction(SPK_EN_GPIO, GPIO_MODE_OUTPUT);
        gpio_set_level(SPK_EN_GPIO, 0);  // Speaker off initially
        ESP_LOGI(TAG, "Speaker Enable (GPIO%d) set to OFF", SPK_EN_GPIO);
    }
    
    // Optional: Try XL9555 control (may not be needed if using direct GPIO)
    xl9555_set_output_bit(I2C_MASTER_PORT, XL9555_I2C_ADDR, XL9555_SPK_EN_PIN, false);

    // 3) Initialize Es8388 codec object
    // Try common ES8388 addresses: 0x10 or 0x11
    uint8_t es8388_addr = ES8388_I2C_ADDR;
    ESP_LOGI(TAG, "Trying ES8388 at I2C address 0x%02X", es8388_addr);
    
    g_codec = new Es8388AudioCodec(nullptr, I2C_MASTER_PORT, 16000, 16000,
                                   I2S_MCLK, I2S_SCK, I2S_WS, I2S_SDOUT, I2S_SDIN,
                                   PA_GPIO, es8388_addr);
    if (g_codec) {
        g_codec->Init();
        // Note: Init() now always succeeds if I2S installs, even if ES8388 I2C fails
        // This allows testing with just I2S hardware
    }

    // 4) Initialize AudioProcessor used for SST (uses AFE)
    g_processor = new AudioProcessor();
    g_processor->Initialize(1, false);
    g_processor->OnOutput([](std::vector<int16_t>&& out) {
        // --- API WINDOW: GOOGLE STT ---
        // This lambda is called with chunks of audio data from the AudioProcessor.
        // This is where you send data to Google's Streaming Speech-to-Text API.
        //
        // 1. Establish a connection to the Google STT streaming endpoint (likely over gRPC or WebSocket).
        //    - You'll need to handle authentication.
        //    - The first message you send must be a configuration message (e.g., encoding: LINEAR16, sampleRateHertz: 16000).
        //
        // 2. Stream the audio data.
        //    - For each `out` vector received here, send its content (`out.data()`, `out.size() * sizeof(int16_t)`) over the established stream.
        //
        // 3. Receive and process transcription results.
        //    - The API will send back transcription results asynchronously. You'll need a mechanism to read them.
        //    - Results can be intermediate or final. You can print them to the console as they arrive.
        //    - Example: ESP_LOGI(TAG, "STT Interim Result: '%s'", result.transcript);
        // --- END API WINDOW ---

        // Calculate audio statistics for the chunk
        static int processor_frame_count = 0;
        processor_frame_count++;
        
        float sum_squares = 0;
        int16_t max_val = 0;
        for (size_t i = 0; i < out.size(); i++) {
            sum_squares += (float)(out[i] * out[i]);
            if (abs(out[i]) > max_val) max_val = abs(out[i]);
        }
        float rms = sqrtf(sum_squares / out.size());
        
        // Only print every 100 frames to reduce log spam
        if (processor_frame_count % 100 == 0) {
            ESP_LOGI(TAG, "[PROCESSOR] Frame %d: RMS=%.1f, Peak=%d %s", 
                     processor_frame_count, rms, max_val,
                     rms > 500 ? "<<< SPEECH DETECTED" : "");
        }
    });

    // 5) Start console control task (disabled - using touch screen instead)
    // xTaskCreate(demo_console_task, "demo_console", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "TTS/SST demo initialized. Use touch screen for interaction.");
    
    // 6) Auto-start SST capture for microphone testing
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Auto-starting microphone capture...");
    ESP_LOGI(TAG, "Speak into the microphone to test!");
    ESP_LOGI(TAG, "Watch for 'VOICE DETECTED' messages");
    ESP_LOGI(TAG, "========================================");
    xTaskCreate(sst_capture_task, "sst_capture", 4096, NULL, 6, &sst_task_handle);
}
