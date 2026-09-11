#pragma once
#include <string>
#include <vector>
#include <functional>

struct GenerationConfig {
    size_t max_new_tokens = 512;
    float temperature = 0.7f;
    float top_p = 0.95f;
    bool do_sample = true;
    std::vector<std::string> stop_strings;
};

struct ServerResponse {
    std::string text = "";
    size_t prompt_tokens = 0;
    size_t completion_tokens = 0;
};

// CLI & Benchmark runners
void run_inference(const std::string& model_dir, const std::string& device);
void run_benchmark(const std::string& model_dir);

// Server completion function
ServerResponse generate_server_response(
    const std::string& model_dir, 
    const std::string& device, 
    const std::string& prompt,
    const GenerationConfig& config,
    std::function<void(std::string)> stream_callback = nullptr
);