#include "downloader.hpp"
#include <iostream>

#ifdef _WIN32

// --- WINDOWS FAST PATH ---
// Windows bypasses libcurl entirely. 
// It will just use the files you already downloaded to your D: drive in WSL!
bool download_model(const std::string& repo_id, const std::string& filename, const std::string& save_path) {
    return true; 
}

#else

// --- LINUX / WSL PATH ---
#include <curl/curl.h>
#include <iomanip>

size_t write_callback(void* ptr, size_t size, size_t nmemb, void* stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE*)stream);
    return written;
}

int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal > 0) {
        double percent = (static_cast<double>(dlnow) / static_cast<double>(dltotal)) * 100.0;
        std::cout << "\r[DOWNLOAD] Progress: " << std::fixed << std::setprecision(1) << percent << "% (" 
                  << (dlnow / (1024 * 1024)) << " MB / " << (dltotal / (1024 * 1024)) << " MB)" << std::flush;
    }
    return 0;
}

bool download_model(const std::string& repo_id, const std::string& filename, const std::string& save_path) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    // Direct download endpoint with auth bypass
    std::string url = "https://huggingface.co/" + repo_id + "/resolve/main/" + filename + "?download=true";

    FILE* fp = fopen(save_path.c_str(), "wb");
    if (!fp) {
        std::cerr << "\n[ERROR] Cannot open file for writing.\n";
        return false;
    }

    std::cout << "[INFO] Connecting to Hugging Face Hub...\n";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); 
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "XeBoostLM/1.0");

    CURLcode res = curl_easy_perform(curl);
    std::cout << "\n"; 
    
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[ERROR] Download failed: " << curl_easy_strerror(res) << "\n";
        return false;
    }
    return true;
}

#endif