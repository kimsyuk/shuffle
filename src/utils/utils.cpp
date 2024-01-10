#define NOMINMAX

#include "utils/utils.h"

#include <iostream>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <random>
#include <curl/curl.h>
#include <restclient-cpp/connection.h>
#include <restclient-cpp/restclient.h>
#include <zip/zip.h>

#include "utils/console.h"
#include "version.h"

using std::cout, std::endl;

using std::ifstream, std::ostringstream, std::ofstream, std::sregex_iterator, std::smatch, std::to_string,
        std::filesystem::temp_directory_path;

string pythonPlatform;

vector<string> splitBySpace(const string&input) {
    std::regex regex_pattern(R"((\S|^)\"[^"]*"|\([^)]*(\)*)|"[^"]*"|\S+)");
    std::vector<std::string> tokens;

    auto words_begin = std::sregex_iterator(input.begin(), input.end(), regex_pattern);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        if (const std::smatch&match = *i;
            !tokens.empty() &&
            tokens.back()[tokens.back().size() - 1] == ')' && match.str()[0] == '!')
            tokens.back() += match.str();
        else tokens.push_back(match.str());
    }

    return tokens;
}

vector<string> split(const string&s, const regex&regex) {
    std::sregex_token_iterator iter(s.begin(), s.end(), regex, -1);
    std::sregex_token_iterator end;
    return {iter, end};
}

const string WHITESPACE = " \n\r\t\f\v";

string ltrim(const string&s) {
    const size_t start = s.find_first_not_of(WHITESPACE);
    return start == string::npos ? "" : s.substr(start);
}

string rtrim(const string&s) {
    const size_t end = s.find_last_not_of(WHITESPACE);
    return end == string::npos ? "" : s.substr(0, end + 1);
}

string trim(const string&s) { return rtrim(ltrim(s)); }

int levenshteinDist(const string&str1, const string&str2) {
    const int len1 = static_cast<int>(str1.length());
    const int len2 = static_cast<int>(str2.length());

    vector dp(len1 + 1, vector(len2 + 1, 0));

    for (int i = 0; i <= len1; ++i) {
        for (int j = 0; j <= len2; ++j) {
            if (i == 0) {
                dp[i][j] = j;
            }
            else if (j == 0) {
                dp[i][j] = i;
            }
            else if (str1[i - 1] == str2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            }
            else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }

    return dp[len1][len2];
}

string replace(string str, const string&from, const string&to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

string readFile(const path&path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

void writeFile(const path&path, const string&value) {
    ofstream file(path);
    file << value;
    file.close();
}

size_t writeText(void* ptr, const size_t size, const size_t nmemb, std::string* data) {
    data->append(static_cast<char *>(ptr), size * nmemb);
    return size * nmemb;
}


std::string readTextFromWeb(const std::string&url) {
    auto [code, body, headers] = RestClient::get(url);

    if (code == 200) {
        return body;
    }

    std::cerr << "Error: " << code << " - " << body << std::endl;
    return "";
}

struct ProgressData {
    double lastPercentage;
};

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    FILE* fp = static_cast<FILE *>(userp);
    return fwrite(contents, size, nmemb, fp);
}

int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    auto* progressData = static_cast<ProgressData *>(clientp);

    if (dltotal > 0) {
        int progress = static_cast<int>(static_cast<float>(dlnow) / static_cast<float>(dltotal) * 100);

        if (progress - progressData->lastPercentage >= 1.0) {
            std::cout << "[";
            const int pos = progress * 20;
            for (int i = 0; i < 20; ++i) {
                if (i < pos) std::cout << "=";
                else if (i == pos) std::cout << ">";
                else std::cout << " ";
            }
            std::cout << "] " << static_cast<int>(progress * 100.0) << " %\r";
            progressData->lastPercentage = progress;
        }
    }

    return 0;
}

bool downloadFile(const string&url, const path&file) {
    ProgressData progress = {0.0};

    curl_global_init(CURL_GLOBAL_DEFAULT);

    if (CURL* curl = curl_easy_init()) {
        CURLcode res;

        if (FILE* fp = fopen(file.string().c_str(), "wb")) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

            res = curl_easy_perform(curl);

            fclose(fp);
        }
        else {
            std::cerr << "Failed to open local file for writing." << std::endl;
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return false;
        }

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return false;
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();
        cout << "[" << string(20, '=') << "] DONE!" << endl;
        return true;
    }

    return false;
}

int onExtractEntry(const char* filename, void* arg) {
    if (const string name = path(filename).filename().string(); !name.empty()) {
        cout << erase_cursor_to_end << "Extracting... (" << name << ")" << teleport(0, wherey() - 1) << endl;
    }
    return 0;
}

path extractZip(const path&zipFile, path extractPath) {
    int arg = 0;
    zip_extract(zipFile.string().c_str(), extractPath.string().c_str(), onExtractEntry, &arg);
    cout << erase_cursor_to_end << "Extracting... (Done!)" << endl;
    return extractPath;
}

void updateShuffle() {
    const path temp = temp_directory_path().append("shflupdater.zip");
#ifdef _WIN32
    string url = "https://github.com/shflterm/shuffle/releases/download/updater/bin-windows.zip";
#elif defined(__linux__)
    string url = "https://github.com/shflterm/shuffle/releases/download/updater/bin-linux.zip";
#elif defined(__APPLE__)
    string url = "https://github.com/shflterm/shuffle/releases/download/updater/bin-macos.zip";
#endif
    downloadFile(url, temp.string());
    path extractPath = temp_directory_path().append("shflupdater");
    extractZip(temp, extractPath);

#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    const string command = extractPath.append("updater.exe").string();

    CreateProcess(nullptr,
                  const_cast<LPSTR>(command.c_str()),
                  nullptr,
                  nullptr,
                  FALSE,
                  CREATE_NEW_CONSOLE,
                  nullptr,
                  nullptr,
                  &si,
                  &pi);
#elif defined(__linux__) || defined(__APPLE__)
    const string command = extractPath.append("updater").string();
    system(("chmod +x " + command).c_str());
    system(command.c_str());
#endif
    exit(0);
}

bool checkUpdate(const bool checkBackground) {
    if (const string latest = trim(readTextFromWeb(
        "https://raw.githubusercontent.com/shflterm/shuffle/main/LATEST")); latest != SHUFFLE_VERSION.str()) {
        cout << "New version available: " << SHUFFLE_VERSION.str() << " -> "
                << latest << endl;
        if (checkBackground) cout << "Type 'shfl upgrade' to get new version!" << endl;
        return true;
    }
    if (!checkBackground) cout << "You are using the latest version of Shuffle." << endl;
    return false;
}

std::string generateRandomString(const int length) {
    std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, characters.length() - 1);

    std::string randomString;
    for (int i = 0; i < length; ++i) {
        randomString += characters[distribution(generator)];
    }

    return randomString;
}

bool isExecutableInPath(const string& executableName) {
#ifdef _WIN32
    if (const char* pathVariable = std::getenv("Path"); pathVariable != nullptr) {
        // PATH를 ';' 기준으로 분리
        std::string pathEnv = pathVariable;
        size_t start = 0;
        size_t end = pathEnv.find(';');

        while (end != std::string::npos) {
            // 각 디렉터리에 실행 파일이 존재하는지 확인
            path directory = pathEnv.substr(start, end - start);
            path fullPath = directory / (executableName + ".exe");

            const DWORD attributes = GetFileAttributes(fullPath.string().c_str());

            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                return true;
            }

            start = end + 1;
            end = pathEnv.find(';', start);
        }
    }

    // PATH의 모든 디렉터리에서 찾지 못하면 false 반환
    return false;
#elif defined(__linux__) || defined(__APPLE__)
    std::string command = "which " + executableName;
    FILE* pipe = popen(command.c_str(), "r");

    if (pipe != nullptr) {
        char buffer[128];
        std::string result = "";

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        int status = pclose(pipe);

        if (status == 0 && !result.empty()) {
            return true;
        }
    }

    return false;
#endif
}

bool endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.substr(str.length() - suffix.length()) == suffix;
}