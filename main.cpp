#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

struct DownloadContext {
    FILE* file = nullptr;
    curl_off_t downloaded = 0;
    curl_off_t total = 0;
};

static size_t write_callback(
    void* contents,
    size_t size,
    size_t nmemb,
    void* userp
) {
    FILE* file = static_cast<FILE*>(userp);
    return std::fwrite(contents, size, nmemb, file);
}

static int progress_callback(
    void* clientp,
    curl_off_t total_download,
    curl_off_t now_download,
    curl_off_t,
    curl_off_t
) {
    DownloadContext* ctx =
        static_cast<DownloadContext*>(clientp);

    ctx->downloaded = now_download;
    ctx->total = total_download;

    if (total_download > 0) {
        double percent =
            static_cast<double>(now_download) /
            static_cast<double>(total_download);

        int width = 40;
        int filled = static_cast<int>(percent * width);

        std::cout << "\r[";

        for (int i = 0; i < width; ++i) {
            std::cout << (i < filled ? '=' : ' ');
        }

        std::cout << "] "
                  << static_cast<int>(percent * 100.0)
                  << "%";

        std::cout.flush();
    } else {
        std::cout << "\rDownloaded: "
                  << now_download
                  << " bytes";

        std::cout.flush();
    }

    return 0;
}

static std::string trim(const std::string& input) {
    size_t start = 0;
    size_t end = input.size();

    while (start < end &&
           std::isspace(
               static_cast<unsigned char>(input[start]))) {
        ++start;
    }

    while (end > start &&
           std::isspace(
               static_cast<unsigned char>(input[end - 1]))) {
        --end;
    }

    return input.substr(start, end - start);
}

static std::string filename_from_url(const std::string& url) {
    std::string clean = url;

    size_t query = clean.find('?');

    if (query != std::string::npos) {
        clean = clean.substr(0, query);
    }

    size_t fragment = clean.find('#');

    if (fragment != std::string::npos) {
        clean = clean.substr(0, fragment);
    }

    size_t slash = clean.find_last_of('/');

    if (slash == std::string::npos ||
        slash + 1 >= clean.size()) {
        return "video.bin";
    }

    std::string name = clean.substr(slash + 1);

    if (name.empty()) {
        return "video.bin";
    }

    return name;
}

static std::string sanitize_filename(std::string name) {
    const std::string invalid = "\\/:*?\"<>|";

    for (char& c : name) {
        if (invalid.find(c) != std::string::npos) {
            c = '_';
        }
    }

    if (name.empty()) {
        name = "video.bin";
    }

    return name;
}

static bool has_url_scheme(const std::string& url) {
    return url.rfind("http://", 0) == 0 ||
           url.rfind("https://", 0) == 0;
}

int main() {

    std::cout << "\n";
    std::cout << "====================================\n";
    std::cout << "       SARI VIDEO DOWNLOADER\n";
    std::cout << "====================================\n\n";

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        std::cerr << "Failed to initialize libcurl.\n";
        return 1;
    }

    std::string url;

    std::cout << "Enter direct video URL:\n> ";
    std::getline(std::cin, url);

    url = trim(url);

    if (url.empty()) {
        std::cerr << "URL cannot be empty.\n";
        curl_global_cleanup();
        return 1;
    }

    if (!has_url_scheme(url)) {
        std::cerr << "Invalid URL. Use http:// or https://\n";
        curl_global_cleanup();
        return 1;
    }

    std::string default_name =
        sanitize_filename(filename_from_url(url));

    std::cout << "\nFilename [" << default_name << "]:\n> ";

    std::string filename;
    std::getline(std::cin, filename);

    filename = trim(filename);

    if (filename.empty()) {
        filename = default_name;
    }

    filename = sanitize_filename(filename);

    fs::path download_dir = "downloads";

    try {
        fs::create_directories(download_dir);
    }
    catch (const std::exception& e) {
        std::cerr << "Cannot create downloads directory: "
                  << e.what() << "\n";

        curl_global_cleanup();
        return 1;
    }

    fs::path output_path =
        download_dir / filename;

    /*
       If the file already exists, create a new name.
    */

    if (fs::exists(output_path)) {

        std::string stem =
            output_path.stem().string();

        std::string extension =
            output_path.extension().string();

        int counter = 1;

        do {
            output_path =
                download_dir /
                (stem + "_" +
                 std::to_string(counter) +
                 extension);

            ++counter;

        } while (fs::exists(output_path));
    }

    FILE* file =
        std::fopen(
            output_path.string().c_str(),
            "wb"
        );

    if (!file) {
        std::cerr << "Cannot create output file.\n";
        curl_global_cleanup();
        return 1;
    }

    CURL* curl = curl_easy_init();

    if (!curl) {
        std::fclose(file);
        curl_global_cleanup();

        std::cerr << "Failed to create CURL handle.\n";
        return 1;
    }

    DownloadContext context;
    context.file = file;

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        write_callback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        file
    );

    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_MAXREDIRS,
        10L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "SARI-Video-Downloader/1.0"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_NOPROGRESS,
        0L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_XFERINFOFUNCTION,
        progress_callback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_XFERINFODATA,
        &context
    );

    curl_easy_setopt(
        curl,
        CURLOPT_FAILONERROR,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        20L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_LOW_SPEED_TIME,
        30L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_LOW_SPEED_LIMIT,
        1024L
    );

    std::cout << "\n\nDownloading...\n";

    CURLcode result =
        curl_easy_perform(curl);

    std::cout << "\n";

    if (result != CURLE_OK) {

        std::cerr
            << "\nDownload failed:\n"
            << curl_easy_strerror(result)
            << "\n";

        std::fclose(file);
        curl_easy_cleanup(curl);
        curl_global_cleanup();

        std::error_code ec;
        fs::remove(output_path, ec);

        return 1;
    }

    long response_code = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &response_code
    );

    double downloaded_size = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_SIZE_DOWNLOAD,
        &downloaded_size
    );

    std::fclose(file);

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    std::cout << "\n====================================\n";
    std::cout << "Download completed!\n";
    std::cout << "HTTP status: "
              << response_code << "\n";
    std::cout << "File: "
              << output_path.string() << "\n";
    std::cout << "Size: "
              << static_cast<long long>(
                     downloaded_size
                 )
              << " bytes\n";
    std::cout << "====================================\n\n";

    return 0;
}
