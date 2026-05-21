#include "audio_processor.h"
#include "esp_log.h"

static const char* TAG = "AudioProcessor";

AudioProcessor::AudioProcessor() {
}
AudioProcessor::~AudioProcessor() {}

void AudioProcessor::Initialize(int channels, bool use_agc) {
    (void)channels; (void)use_agc;
    ESP_LOGI(TAG, "AudioProcessor (stub) initialized");
}

void AudioProcessor::Start() {
    ESP_LOGI(TAG, "AudioProcessor started");
}

void AudioProcessor::Stop() {
    ESP_LOGI(TAG, "AudioProcessor stopped");
}

void AudioProcessor::Input(const std::vector<int16_t>& samples) {
    // For STT test: compute simple RMS and forward to output callback as-is
    if (output_cb) {
        std::vector<int16_t> copy = samples;
        output_cb(std::move(copy));
    }
}

void AudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&&)> cb) {
    output_cb = cb;
}
