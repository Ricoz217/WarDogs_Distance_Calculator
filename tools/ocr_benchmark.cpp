#include "wardogs/ocr.hpp"

#include <Windows.h>
#include <Psapi.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    if (argc < 3 || argc > 4) {
        std::wcerr << L"usage: ocr_benchmark <model.onnx> <image.png> [iterations]\n";
        return 2;
    }
    const int iterations = argc == 4 ? std::max(1, _wtoi(argv[3])) : 20;
    try {
        const auto image = wardogs::load_image_file(argv[2]);
        const auto start_load = std::chrono::steady_clock::now();
        wardogs::RapidOcr ocr(argv[1]);
        const auto end_load = std::chrono::steady_clock::now();
        wardogs::OcrResult result;
        const auto start_runs = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) result = ocr.recognize(image);
        const auto end_runs = std::chrono::steady_clock::now();
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                             sizeof(memory));
        const auto load_ms = std::chrono::duration<double, std::milli>(end_load - start_load);
        const auto run_ms = std::chrono::duration<double, std::milli>(end_runs - start_runs);
        std::wcout << L"text=" << result.text << L'\n'
                   << L"confidence=" << std::fixed << std::setprecision(5)
                   << result.confidence << L'\n'
                   << L"model_load_ms=" << load_ms.count() << L'\n'
                   << L"average_ocr_ms=" << run_ms.count() / iterations << L'\n'
                   << L"working_set_mib="
                   << memory.WorkingSetSize / 1024.0 / 1024.0 << L'\n'
                   << L"private_mib="
                   << memory.PrivateUsage / 1024.0 / 1024.0 << L'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
