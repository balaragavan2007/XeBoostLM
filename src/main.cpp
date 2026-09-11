#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <cstdlib>

#include "inference_engine.hpp"
#include "httplib.h"
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

void init_terminal() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

void print_usage() {
    std::cout << "Usage:\n"
              << "  xeboost <command> [flags]\n\n"
              << "Commands:\n"
              << "  run <model> [--device CPU|GPU|NPU]  Run interactive chat\n"
              << "  pull <repo_or_alias>               Pull a model from HF\n"
              << "  list                               List local models\n"
              << "  serve                              Start REST API server (Open WebUI)\n"
              << "  benchmark <model>                  Benchmark model across CPU, GPU, NPU\n";
}

int cmd_list() {
    std::string base_dir = "local_models";
    if (!fs::exists(base_dir) || !fs::is_directory(base_dir)) {
        std::cout << "No models found in ./local_models\n";
        return 0;
    }

    std::cout << std::left 
              << std::setw(35) << "NAME" 
              << std::setw(15) << "SIZE" 
              << "TARGETS\n";

    for (const auto& entry : fs::directory_iterator(base_dir)) {
        if (!entry.is_directory()) continue;

        std::string model_name = entry.path().filename().string();
        
        uintmax_t total_bytes = 0;
        for (const auto& file : fs::recursive_directory_iterator(entry.path())) {
            if (fs::is_regular_file(file)) total_bytes += fs::file_size(file);
        }

        double size_mb = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
        std::ostringstream size_stream;
        if (size_mb >= 1024.0) {
            size_stream << std::fixed << std::setprecision(1) << (size_mb / 1024.0) << " GB";
        } else {
            size_stream << std::fixed << std::setprecision(0) << size_mb << " MB";
        }

        std::string hw_target = "CPU, iGPU";
        if (model_name.find("int8") != std::string::npos) {
            hw_target = "CPU, iGPU, NPU, HYBRID";
        }

        std::cout << std::left 
                  << std::setw(35) << model_name 
                  << std::setw(15) << size_stream.str() 
                  << hw_target << "\n";
    }
    return 0;
}

int cmd_run(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: xeboost run <model_name> [--device CPU|GPU|NPU]\n";
        return 1;
    }
    std::string model_name = argv[2];
    std::string device = "CPU";

    for (int i = 3; i < argc; ++i) {
        if (std::string(argv[i]) == "--device" && i + 1 < argc) {
            device = argv[++i];
        }
    }

    std::string model_path = "local_models/" + model_name;
    if (!fs::exists(model_path + "/openvino_model.xml")) {
        std::cerr << "Error: model '" << model_name << "' not found in " << model_path << "\n";
        return 1;
    }

    run_inference(model_path, device);
    return 0;
}

int cmd_serve(int argc, char* argv[]) {
    httplib::Server svr;

    // 1. CORS
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // 2. List Models
    svr.Get("/v1/models", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        json response;
        response["object"] = "list";
        response["data"] = json::array();

        if (fs::exists("local_models")) {
            for (const auto& entry : fs::directory_iterator("local_models")) {
                if (entry.is_directory()) {
                    std::string base_name = entry.path().filename().string();
                        
                    std::vector<std::string> devices = {"cpu", "gpu"};
                    if (base_name.find("int8") != std::string::npos) {
                        devices.push_back("npu");
                        devices.push_back("hybrid");
                    }

                    for (const auto& dev : devices) {
                        json model;
                        model["id"] = base_name + ":" + dev;
                        model["object"] = "model";
                        model["created"] = 1717200000;
                        model["owned_by"] = "xeboost";
                        response["data"].push_back(model);
                    }
                }
            }
        }
        res.set_content(response.dump(), "application/json");
    });

    // 3. Chat Completions
    svr.Post("/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        bool is_stream = false;
            
        try {
            auto req_json = json::parse(req.body);
            std::string full_model_id = req_json.value("model", "qwen2.5-0.5b-int8:npu");
            is_stream = req_json.value("stream", false);
                
            std::string model_name = full_model_id;
            std::string device = "CPU";
            size_t colon_pos = full_model_id.find(':');
            if (colon_pos != std::string::npos) {
                model_name = full_model_id.substr(0, colon_pos);
                std::string dev_tag = full_model_id.substr(colon_pos + 1);
                if (dev_tag == "npu") device = "NPU";
                else if (dev_tag == "gpu") device = "GPU";
                else if (dev_tag == "cpu") device = "CPU";
                else if (dev_tag == "hybrid") device = "AUTO:NPU,GPU,CPU";
            }
            std::string model_path = "local_models/" + model_name;

            std::string formatted_prompt = "";
            if (req_json.contains("messages") && req_json["messages"].is_array()) {
                for (const auto& msg : req_json["messages"]) {
                    std::string role = msg.value("role", "user");
                    std::string content = msg.value("content", "");
                    formatted_prompt += "<|im_start|>" + role + "\n" + content + "<|im_end|>\n";
                }
                formatted_prompt += "<|im_start|>assistant\n";
            } else {
                formatted_prompt = req_json.value("prompt", "");
            }

            GenerationConfig gen_config;
            gen_config.max_new_tokens = req_json.value("max_tokens", 512);

            if (req_json.contains("temperature") && !req_json["temperature"].is_null()) {
                float temp = req_json["temperature"].get<float>();
                if (temp <= 0.0f) {
                    gen_config.do_sample = false;
                } else {
                    gen_config.do_sample = true;
                    gen_config.temperature = temp;
                }
            }

            if (req_json.contains("top_p") && !req_json["top_p"].is_null()) {
                gen_config.top_p = req_json["top_p"].get<float>();
            }

            gen_config.stop_strings.push_back("<|im_end|>");
            gen_config.stop_strings.push_back("<|endoftext|>");

            if (req_json.contains("stop")) {
                if (req_json["stop"].is_string()) {
                    gen_config.stop_strings.push_back(req_json["stop"].get<std::string>());
                } else if (req_json["stop"].is_array()) {
                    for (const auto& s : req_json["stop"]) {
                        if (s.is_string()) gen_config.stop_strings.push_back(s.get<std::string>());
                    }
                }
            }

            std::cout << "[API] Executing: " << model_name << " on " << device 
                      << " (Stream: " << (is_stream ? "YES" : "NO") << ")\n";

            if (is_stream) {
                res.set_chunked_content_provider("text/event-stream", 
                    [model_path, device, formatted_prompt, full_model_id, gen_config](size_t offset, httplib::DataSink &sink) {
                        if (offset == 0) {
                            try {
                                auto start_time = std::chrono::high_resolution_clock::now();

                                auto chunk_callback = [&](std::string word) {
                                    json chunk;
                                    chunk["id"] = "chatcmpl-xeboost";
                                    chunk["object"] = "chat.completion.chunk";
                                    chunk["created"] = 1717200000;
                                    chunk["model"] = full_model_id;
                                    chunk["choices"] = json::array({{
                                        {"index", 0},
                                        {"delta", {{"content", word}}},
                                        {"finish_reason", nullptr}
                                    }});
                                    std::string sse = "data: " + chunk.dump() + "\n\n";
                                    sink.write(sse.c_str(), sse.size());
                                };

                                ServerResponse response = generate_server_response(model_path, device, formatted_prompt, gen_config, chunk_callback);

                                auto end_time = std::chrono::high_resolution_clock::now();
                                double duration = std::chrono::duration<double>(end_time - start_time).count();
                                double tps = duration > 0 ? (response.completion_tokens / duration) : 0.0;

                                std::cout << "[API] Completed: " << response.completion_tokens << " tokens in "
                                          << std::fixed << std::setprecision(2) << duration << "s ("
                                          << tps << " tok/s)\n";

                                json end_chunk = {
                                    {"id", "chatcmpl-xeboost"},
                                    {"object", "chat.completion.chunk"},
                                    {"created", 1717200000},
                                    {"model", full_model_id},
                                    {"choices", {{ {"index", 0}, {"delta", json::object()}, {"finish_reason", "stop"} }}},
                                    {"usage", {
                                        {"prompt_tokens", response.prompt_tokens},
                                        {"completion_tokens", response.completion_tokens},
                                        {"total_tokens", response.prompt_tokens + response.completion_tokens}
                                    }}
                                };
                                std::string end_sse = "data: " + end_chunk.dump() + "\n\n";
                                sink.write(end_sse.c_str(), end_sse.size());
                                
                                std::string done_sse = "data: [DONE]\n\n";
                                sink.write(done_sse.c_str(), done_sse.size());
                            } catch (const std::exception& e) {
                                std::cerr << "[API-STREAM-EXCEPTION] " << e.what() << "\n";
                            }
                            sink.done();
                        }
                        return true;
                    });
            } else {
                auto start_time = std::chrono::high_resolution_clock::now();
                ServerResponse response = generate_server_response(model_path, device, formatted_prompt, gen_config, nullptr);
                auto end_time = std::chrono::high_resolution_clock::now();
                double duration = std::chrono::duration<double>(end_time - start_time).count();
                double tps = duration > 0 ? (response.completion_tokens / duration) : 0.0;

                std::cout << "[API] Completed: " << response.completion_tokens << " tokens in "
                          << std::fixed << std::setprecision(2) << duration << "s ("
                          << tps << " tok/s)\n";

                json res_json;
                res_json["id"] = "chatcmpl-xeboost";
                res_json["object"] = "chat.completion";
                res_json["created"] = 1717200000;
                res_json["model"] = full_model_id;
                res_json["choices"] = json::array({{
                    {"index", 0},
                    {"message", { {"role", "assistant"}, {"content", response.text} }},
                    {"finish_reason", "stop"}
                }});
                res_json["usage"] = {
                    {"prompt_tokens", response.prompt_tokens},
                    {"completion_tokens", response.completion_tokens},
                    {"total_tokens", response.prompt_tokens + response.completion_tokens}
                };
                res.set_content(res_json.dump(), "application/json");
            }
        } catch (const std::exception& e) {
            std::cerr << "[API-EXCEPTION] " << e.what() << "\n";
            if (!is_stream) {
                res.status = 500;
                res.set_content("{\"error\": \"" + std::string(e.what()) + "\"}", "application/json");
            }
        }
    });

    std::cout << "Starting XeBoostLM server on http://0.0.0.0:11434\n";
    std::cout << "Ready for Open WebUI / LibreChat connections.\n";
    svr.listen("0.0.0.0", 11434);
    return 0;
}

// Structured model entry for categorized display
struct ModelInfo {
    std::string alias;
    std::string repo_id;
    std::string family;
    std::string version;
    std::string quantization;
};

// Comprehensive categorized catalog
static const std::vector<ModelInfo> MODEL_CATALOG = {
    // Qwen Family
    {"qwen2.5-0.5b-int8", "OpenVINO/Qwen2.5-0.5B-Instruct-int8-ov", "Qwen", "2.5 (0.5B)", "INT8"},
    {"qwen2.5-1.5b-int8", "OpenVINO/Qwen2.5-1.5B-Instruct-int8-ov", "Qwen", "2.5 (1.5B)", "INT8"},
    {"qwen2.5-3b-int8",   "OpenVINO/Qwen2.5-3B-Instruct-int8-ov",   "Qwen", "2.5 (3B)",   "INT8"},
    {"qwen2.5-7b-int8",   "OpenVINO/Qwen2.5-7B-Instruct-int8-ov",   "Qwen", "2.5 (7B)",   "INT8"},
    
    // Phi Family
    {"phi-3-mini-int4",   "OpenVINO/Phi-3-mini-4k-instruct-int4-ov", "Phi", "Phi-3 Mini", "INT4"},
    {"phi-3-mini-int8",   "OpenVINO/Phi-3-mini-4k-instruct-int8-ov", "Phi", "Phi-3 Mini", "INT8"},
    
    // Llama Family
    {"llama-3.2-1b-int8", "OpenVINO/Llama-3.2-1B-Instruct-int8-ov", "Llama", "3.2 (1B)", "INT8"},
    {"llama-3.2-3b-int8", "OpenVINO/Llama-3.2-3B-Instruct-int8-ov", "Llama", "3.2 (3B)", "INT8"},
    {"llama-3.1-8b-int8", "OpenVINO/Meta-Llama-3.1-8B-Instruct-int8-ov", "Llama", "3.1 (8B)", "INT8"},
    
    // Mistral Family
    {"mistral-7b-int8",   "OpenVINO/Mistral-7B-Instruct-v0.3-int8-ov", "Mistral", "v0.3 (7B)", "INT8"},
    
    // Gemma Family
    {"gemma-2-2b-int8",   "OpenVINO/gemma-2-2b-it-int8-ov", "Gemma", "Gemma 2 (2B)", "INT8"}
};

int cmd_pull(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: xeboost pull <alias_or_hf_repo>\n\n";
        std::cout << "Available Models by Family:\n";

        // Group and print by family
        std::vector<std::string> families = {"Qwen", "Phi", "Llama", "Mistral", "Gemma"};
        for (const auto& fam : families) {
            std::cout << "  [" << fam << "]\n";
            for (const auto& m : MODEL_CATALOG) {
                if (m.family == fam) {
                    std::cout << "    - " << std::left << std::setw(20) << m.alias 
                              << " | Version: " << std::setw(12) << m.version 
                              << " | Quant: " << m.quantization << "\n";
                }
            }
            std::cout << "\n";
        }
        return 1;
    }

    std::string input = argv[2];
    std::string repo_id = input;
    std::string folder_name = input;

    // Check if input matches an alias
    auto it = std::find_if(MODEL_CATALOG.begin(), MODEL_CATALOG.end(), [&](const ModelInfo& m) {
        return m.alias == input;
    });

    if (it != MODEL_CATALOG.end()) {
        repo_id = it->repo_id;
        folder_name = it->alias;
    } else {
        size_t slash = folder_name.find_last_of('/');
        if (slash != std::string::npos) {
            folder_name = folder_name.substr(slash + 1);
        }
    }

    fs::path target_dir = fs::path("local_models") / folder_name;
    fs::create_directories("local_models");

    std::cout << "[XeBoost] Pulling " << repo_id << " into " << target_dir.string() << "...\n";

    std::string py_cmd = "python -c \""
                         "from huggingface_hub import snapshot_download; "
                         "snapshot_download(repo_id='" + repo_id + "', "
                         "local_dir='" + target_dir.string() + "', "
                         "local_dir_use_symlinks=False)\"";

    int ret = std::system(py_cmd.c_str());
    if (ret != 0) {
        std::cerr << "[XeBoost-ERROR] Download failed. Ensure 'huggingface_hub' is installed (`pip install huggingface_hub`).\n";
        return 1;
    }

    if (fs::exists(target_dir / "openvino_model.xml")) {
        std::cout << "\n[XeBoost] Successfully downloaded and verified: " << folder_name << "\n";
        return 0;
    } else {
        std::cerr << "[XeBoost-ERROR] 'openvino_model.xml' not found in " << target_dir.string() << "\n";
        return 1;
    }
}

int cmd_benchmark(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: xeboost benchmark <model_folder_name>\n";
        return 1;
    }
    std::string model_name = argv[2];
    std::string model_path = "local_models/" + model_name;

    if (!fs::exists(model_path)) {
        std::cerr << "[Error] Model not found: " << model_path << "\n";
        return 1;
    }

    run_benchmark(model_path);
    return 0;
}

int main(int argc, char* argv[]) {
    init_terminal();

    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "serve") {
        return cmd_serve(argc, argv);
    } else if (cmd == "run") {
        return cmd_run(argc, argv);
    } else if (cmd == "pull") {
        return cmd_pull(argc, argv);
    } else if (cmd == "list") {
        return cmd_list();
    } else if (cmd == "benchmark" || cmd == "bench") {
        return cmd_benchmark(argc, argv);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        print_usage();
        return 1;
    }
}