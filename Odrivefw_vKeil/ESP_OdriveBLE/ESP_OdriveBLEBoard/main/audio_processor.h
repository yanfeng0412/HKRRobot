#pragma once

#include <cstdint>
#include <vector>
#include <functional>

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    void Initialize(int channels = 1, bool use_agc = false);
    void Start();
    void Stop();
    void Input(const std::vector<int16_t>& samples);
    void OnOutput(std::function<void(std::vector<int16_t>&&)> cb);

private:
    std::function<void(std::vector<int16_t>&&)> output_cb;
};
