#include "inference_engine.hpp"
#include <openvino/genai/llm_pipeline.hpp>
#include <openvino/runtime/properties.hpp>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

// ==========================================
// 1. CLI Interactive Runner (xeboost run)
// ==========================================
void run_inference(const std::string& model_dir, const std::string& device) {
    fs::create_directories("cache");
    ov::AnyMap properties = { ov::cache_dir("cache") };

    std::cout << "Loading model from " << model_dir << " onto " << device << " (Cache: ./cache)...\n";
    ov::genai::LLMPipeline pipe(model_dir, device, properties);
    std::cout << "Model loaded successfully. Type 'exit' to quit.\n";

    std::string prompt;
    while (true) {
        std::cout << "\n>>> ";
        if (!std::getline(std::cin, prompt) || prompt == "exit") break;
        if (prompt.empty()) continue;

        auto streamer = [](std::string word) {
            std::cout << word << std::flush;
            return ov::genai::StreamingStatus::RUNNING;
        };

        pipe.generate(prompt, ov::genai::streamer(streamer));
        std::cout << "\n";
    }
}

// ==========================================
// 2. Hardware Benchmarker (xeboost benchmark)
// ==========================================
void run_benchmark(const std::string& model_dir) {
    std::string prompt = "<|im_start|>user\nExplain the theory of relativity in one concise paragraph.<|im_end|>\n<|im_start|>assistant\n";
    std::vector<std::string> devices = {"CPU", "GPU", "NPU"};

    std::cout << "\n=========================================================\n";
    std::cout << " XeBoostLM Hardware Benchmark: " << model_dir << "\n";
    std::cout << "=========================================================\n";
    std::cout << std::left << std::setw(10) << "Device"
              << std::setw(15) << "Load Time"
              << std::setw(15) << "TTFT (ms)"
              << std::setw(15) << "Speed (tok/s)" << "\n";
    std::cout << "---------------------------------------------------------\n";

    for (const auto& device : devices) {
        try {
            auto load_start = std::chrono::high_resolution_clock::now();
            fs::create_directories("cache");
            ov::AnyMap properties = { ov::cache_dir("cache") };
            ov::genai::LLMPipeline pipe(model_dir, device, properties);
            auto load_end = std::chrono::high_resolution_clock::now();
            double load_time = std::chrono::duration<double>(load_end - load_start).count();

            ov::genai::GenerationConfig ov_config;
            ov_config.max_new_tokens = 128;
            ov_config.do_sample = false;

            bool first_token_received = false;
            double ttft_ms = 0.0;
            size_t tokens = 0;

            auto gen_start = std::chrono::high_resolution_clock::now();

            std::function<ov::genai::StreamingStatus(std::string)> streamer = 
                [&](std::string word) {
                    if (!first_token_received) {
                        auto first_token_time = std::chrono::high_resolution_clock::now();
                        ttft_ms = std::chrono::duration<double, std::milli>(first_token_time - gen_start).count();
                        first_token_received = true;
                    }
                    tokens++;
                    return ov::genai::StreamingStatus::RUNNING;
                };

            pipe.generate(prompt, ov_config, streamer);

            auto gen_end = std::chrono::high_resolution_clock::now();
            double gen_time = std::chrono::duration<double>(gen_end - gen_start).count();
            double tps = gen_time > 0 ? (tokens / gen_time) : 0.0;

            std::cout << std::left << std::setw(10) << device
                      << std::fixed << std::setprecision(2) << load_time << "s        "
                      << std::fixed << std::setprecision(2) << ttft_ms << "          "
                      << std::fixed << std::setprecision(2) << tps << "\n";

        } catch (const std::exception& e) {
            std::cout << std::left << std::setw(10) << device << "FAILED: " << e.what() << "\n";
        }
    }
    std::cout << "=========================================================\n\n";
}

// ==========================================
// 3. Server Pipeline (xeboost serve)
// ==========================================
static std::unique_ptr<ov::genai::LLMPipeline> global_pipe = nullptr;
static std::string current_model_dir = "";
static std::string current_device = "";
static std::mutex pipeline_mutex;

ServerResponse generate_server_response(
    const std::string& model_dir, 
    const std::string& device, 
    const std::string& prompt,
    const GenerationConfig& config,
    std::function<void(std::string)> stream_callback) 
{
    std::lock_guard<std::mutex> lock(pipeline_mutex);

    if (current_model_dir != model_dir || current_device != device || global_pipe == nullptr) {
        fs::create_directories("cache");
        ov::AnyMap properties = { ov::cache_dir("cache") };

        std::cout << "[Server] Loading " << model_dir << " onto target " << device << " (Cached)...\n";
        global_pipe = std::make_unique<ov::genai::LLMPipeline>(model_dir, device, properties);
        current_model_dir = model_dir;
        current_device = device;
        std::cout << "[Server] Pipeline ready on " << device << ".\n";
    }

    ov::genai::GenerationConfig ov_config;
    ov_config.max_new_tokens = config.max_new_tokens;
    ov_config.temperature = config.temperature;
    ov_config.top_p = config.top_p;
    ov_config.do_sample = config.do_sample;
    for (const auto& stop_str : config.stop_strings) {
        ov_config.stop_strings.insert(stop_str);
    }

    ServerResponse response;

    try {
        response.prompt_tokens = global_pipe->get_tokenizer().encode(prompt).input_ids.get_size();
    } catch (...) {
        response.prompt_tokens = prompt.length() / 4 + 1;
    }

    std::function<ov::genai::StreamingStatus(std::string)> ov_streamer = 
        [&response, stream_callback](std::string word) {
            response.completion_tokens++;
            if (stream_callback) {
                stream_callback(word);
            }
            return ov::genai::StreamingStatus::RUNNING;
        };

    ov::genai::DecodedResults results = global_pipe->generate(prompt, ov_config, ov_streamer);
    response.text = std::string(results);

    return response;
}