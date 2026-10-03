#pragma once

#include "audio.hpp"

#include <atomic>
#include <functional>
#include <string>

namespace el_sistema {

class TranscriptionService {
public:
    using ProgressCallback = std::function<void(float, const char*)>;

    bool transcribe(const std::string& url, const AudioBuffer& audio, std::atomic<bool>& cancel_requested,
                    std::string& text, std::string& error, const ProgressCallback& callback);
    static bool parse_response(const char* json, size_t length, std::string& text, std::string& error);
};

}  // namespace el_sistema
