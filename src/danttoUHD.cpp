#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <list>
#include <iomanip>
#include <sstream>
#include "stream.h"
#include "demuxer.h"
#include "muxer.h"
#include "streamPacket.h"
#include "httplib.h"
#include "config.h"

atsc3::Demuxer demuxer;
Muxer muxer;

int main(int argc, char* argv[]) {
    std::string inputPath, outputPath;
    std::mutex mutex;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg.find("--casServerUrl=") == 0) {
            config.casServerUrl = arg.substr(std::string("--casServerUrl=").length());
        }

        if (inputPath == "") {
            inputPath = arg;
        }
        else if (outputPath == "") {
            outputPath = arg;
        }
    }

    if (inputPath == "" || outputPath == "") {
        std::cerr << "danttoUHD.exe <input> <output.ts>" << std::endl;
        std::cerr << "options:" << std::endl;
        std::cerr << "\t--casServerUrl=<url>" << std::endl;
        return 1;
    }

    if (inputPath == outputPath) {
        std::cerr << "Input and output paths cannot be the same." << std::endl;
        return 1;
    }

    std::unique_ptr<std::istream> inputStream;
    std::unique_ptr<std::ifstream> inputFs;
    inputFs = std::make_unique<std::ifstream>(inputPath, std::ios::binary);
    if (!inputFs->is_open()) {
        std::cerr << "Unable to open input file: " << inputPath << std::endl;
        return 1;
    }
    inputStream = std::move(inputFs);

    std::unique_ptr<std::ofstream> outputFs;
    outputFs = std::make_unique<std::ofstream>(outputPath, std::ios::binary);
    if (!outputFs->is_open()) {
        std::cerr << "Unable to open output file: " << inputPath << std::endl;
        return 1;
    }

    std::vector<uint8_t> inputBuffer;
    constexpr size_t chunkSize = 1024 * 1024;

    uint64_t lastTime = 0;

    std::map<uint64_t, std::vector<uint8_t>> tsBuffer;
    muxer.setOutputCallback([&](const uint8_t* data, size_t size, uint64_t time) {
        if (time == 0) {
            time = lastTime + 1;
            lastTime = time;
        }

        auto it = tsBuffer.find(time);
        if (it == tsBuffer.end()) {
            tsBuffer[time] = std::vector<uint8_t>(data, data + size);
        }
        else {
            it->second.insert(it->second.end(), data, data + size);
        }

        if (tsBuffer.size() > 100) {
            for (auto it = tsBuffer.begin(); it != tsBuffer.end(); ) {
                if (tsBuffer.size() < 100) {
                    break;
                }
                if (outputFs->is_open()) {
                    outputFs->write(reinterpret_cast<const char*>(it->second.data()), it->second.size());
                }
                it = tsBuffer.erase(it);
            }
        }

        if (lastTime < time) {
            lastTime = time;
        }
    });
    demuxer.setHandler(&muxer);


    while (true) {
        if (inputStream->eof()) {
            break;
        }

        size_t oldSize = inputBuffer.size();
        inputBuffer.resize(oldSize + chunkSize);
        inputStream->read(reinterpret_cast<char*>(inputBuffer.data() + oldSize), chunkSize);
        inputBuffer.resize(oldSize + inputStream->gcount());

        demuxer.demux(inputBuffer);
        inputBuffer.clear();
    }

    // flush remaining data
    for (auto it = tsBuffer.begin(); it != tsBuffer.end(); ) {
        if (tsBuffer.size() < 100) {
            break;
        }
        if (outputFs->is_open()) {
            outputFs->write(reinterpret_cast<const char*>(it->second.data()), it->second.size());
        }
        it = tsBuffer.erase(it);
    }

    outputFs->close();
}