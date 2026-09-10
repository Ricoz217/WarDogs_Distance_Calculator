#include "wardogs/ocr.hpp"
#include "wardogs/windows_ocr.hpp"

#include <winrt/base.h>
#include <Windows.h>
#include <Psapi.h>

#include <chrono>
#include <iomanip>
#include <iostream>

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::wcerr << L"usage: windows_ocr_probe <image.png>\n";
        return 2;
    }
    try {
        const auto image = wardogs::load_image_file(argv[1]);
        wardogs::WindowsOcr ocr;
        const auto begin = std::chrono::steady_clock::now();
        const auto original = ocr.recognize(image);
        const auto middle = std::chrono::steady_clock::now();
        std::wcout << L"original=" << original.text << L'\n';
        const auto contrasted = ocr.recognize_high_contrast(image);
        const auto end = std::chrono::steady_clock::now();
        std::wcout << L"high_contrast=" << contrasted.text << L'\n';
        PROCESS_MEMORY_COUNTERS_EX memory{sizeof(memory)};
        GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                             sizeof(memory));
        std::wcout << std::fixed << std::setprecision(2)
                   << L"original_ms="
                   << std::chrono::duration<double, std::milli>(middle - begin).count()
                   << L'\n' << L"high_contrast_ms="
                   << std::chrono::duration<double, std::milli>(end - middle).count()
                   << L'\n' << L"private_mib="
                   << memory.PrivateUsage / 1024.0 / 1024.0 << L'\n';
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::wcerr << L"HRESULT=0x" << std::hex
                   << static_cast<unsigned>(error.code()) << L" message="
                   << error.message().c_str() << L'\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
