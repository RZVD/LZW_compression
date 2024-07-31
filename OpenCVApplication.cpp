#include "stdafx.h"
#include <filesystem>
#include "common.h"
#include <opencv2/core/utils/logger.hpp>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <opencv2/core.hpp>
#include <Windows.h>
#include <bitset>
wchar_t* projectPath;


// A starting bit width of 9 seems to be the standard 
// Also used in GIF and Unix compress
#define LZW_STARTING_BIT_WIDTH 9

std::vector<uint8_t> LZWencode(const Mat_<uchar>& img) {
    std::string_view original((char*)img.data, img.total());

    std::unordered_map<std::string, int> string_table;
    int dictSize = 256;
    int bitWidth = LZW_STARTING_BIT_WIDTH;

    for (int i = 0; i < dictSize; ++i) {
        string_table[std::string(1, i)] = i;
    }

    std::vector<uint8_t> result;
    std::string omega;
    uint32_t bitBuffer = 0;
    int bitCount = 0;

    auto flushBuffer = [&]() {
        while (bitCount >= 8) {
            result.push_back(bitBuffer & 0xFF);
            bitBuffer >>= 8;
            bitCount -= 8;
        }
    };

    for (char k : original) {
        std::string temp = omega + k;
        if (string_table.find(temp) != string_table.end()) {
            omega = temp;
        } else {
            bitBuffer |= (string_table[omega] << bitCount);
            bitCount += bitWidth;
            flushBuffer();
            string_table[temp] = dictSize++;

            if (dictSize > (1 << bitWidth)) {
                bitWidth++;
            }
            omega = k;
        }
    }

    if (!omega.empty()) {
        bitBuffer |= string_table[omega] << bitCount;
        bitCount += bitWidth;
        flushBuffer();
    }

    if (bitCount > 0) {
        result.push_back(bitBuffer & 0xFF);
        if (bitCount > 8) {
            result.push_back((bitBuffer >> 8) & 0xFF);
        }
    }

}

Mat_<uchar> LZWdecode(const std::vector<uint8_t>& v, int rows, int cols) {
    std::unordered_map<int, std::string> string_table;
    int dict_size = 256;
    int bit_width = LZW_STARTING_BIT_WIDTH;
    int bit_count = 0;
    uint32_t bit_buffer = 0;

    std::string s;
    std::string entry;
    std::string result;

    for (int i = 0; i < dict_size; i++) {
        string_table[i] = std::string(1, i);
    }

    auto fetchNextCode = [&](int& index) {
        while (bit_count < bit_width) {
            if (index < v.size()) {
                bit_buffer |= (v[index++] << bit_count);
                bit_count += 8;
            } else {
                return -1;
            }
        }

        int code = bit_buffer & ((1 << bit_width) - 1);
        bit_buffer >>= bit_width;
        bit_count -= bit_width;
        return code;
    };

    int index = 0;
    int code = fetchNextCode(index);

    s = string_table[code];
    result = s;

    while ((code = fetchNextCode(index)) != -1) {
        if (string_table.find(code) != string_table.end()) {
            entry = string_table[code];
        } else if (code == dict_size) {
            entry = s + s[0];
        }

        result += entry;

        uint32_t max_dict_size = 1 << bit_width;
        if (dict_size < max_dict_size ){
            string_table[dict_size++] = s + entry[0];
        }
        if (dict_size >= (1 << bit_width)) {
            bit_width++;
        }

        s = entry;
    }

    Mat_<uchar> dst(rows, cols);
    std::memcpy(dst.data, result.data(), rows * cols);

    return dst;
}

void TestLZW() {
    namespace fs = std::filesystem;
    char fname[512];

    while (openFileDlg(fname)) {
        cv::Mat_<uchar> img = cv::imread(fname, 0);
        cv::imshow("Original Image", img);

        std::vector<uint8_t> code = LZWencode(img);
        auto filesize = fs::file_size(fname);

        std::cout << "Original file size: " << filesize << "\n";
        std::string compressed_file_path(fname);
        compressed_file_path += ".compressed";

        std::ofstream output_file(compressed_file_path, std::ios::binary);
        output_file.write((char*)code.data(), code.size() * sizeof(char));
        output_file.close();


        auto compressed_filesize = fs::file_size(compressed_file_path);

        std::cout << "Compressed file size: " << compressed_filesize << "\n";
        std::cout << "Compression ratio: " << (float)filesize / (float)compressed_filesize << "\n";

        std::vector<uint8_t> reconstructed;
        std::ifstream compressed_file(compressed_file_path, std::ios::binary);

        reconstructed.assign(
            std::istreambuf_iterator<char>(compressed_file),
			std::istreambuf_iterator<char>()
        );
        compressed_file.close();

        auto dst = LZWdecode(reconstructed, img.rows, img.cols);

        cv::imshow("Reconstructed image", dst);

        std::cout << (std::memcmp(img.data, dst.data, img.total()) == 0 ? "Images are identical" : "Images differ") << "\n";
        cv::waitKey();
    }
}


int main() {
	cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_FATAL);
    projectPath = _wgetcwd(0, 0);
	destroyAllWindows();
	TestLZW();
	return 0;
}