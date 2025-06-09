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

Demuxer demuxer;
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
    inputBuffer.reserve(chunkSize * 2);

    muxer.setOutputCallback([&](const uint8_t* data, size_t size) {
        if (outputFs->is_open()) {
            outputFs->write(reinterpret_cast<const char*>(data), size);
        }
    });
    demuxer.setHandler(&muxer);


    while (true) {
        if (inputStream->eof()) {
            break;
        }

        if (inputBuffer.size() < chunkSize) {
            size_t oldSize = inputBuffer.size();
            inputBuffer.resize(oldSize + chunkSize);
            inputStream->read(reinterpret_cast<char*>(inputBuffer.data() + oldSize), chunkSize);
            inputBuffer.resize(oldSize + inputStream->gcount());
        }

        demuxer.demux(inputBuffer);
        inputBuffer.clear();
    }

    outputFs->close();
}