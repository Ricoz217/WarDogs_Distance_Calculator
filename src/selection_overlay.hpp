#pragma once

#include "wardogs/capture.hpp"

#include <Windows.h>
#include <QString>

#include <functional>
#include <optional>

class SelectionOverlay final {
public:
    using Callback =
        std::function<void(std::optional<wardogs::CaptureRegion>, QString)>;

    SelectionOverlay() = default;
    ~SelectionOverlay();
    SelectionOverlay(const SelectionOverlay&) = delete;
    SelectionOverlay& operator=(const SelectionOverlay&) = delete;

    bool begin(Callback callback);

private:
    static constexpr wchar_t class_name[] =
        L"WarDogsDistanceCalculatorSelectorQt";

    static void ensure_class();
    static LRESULT CALLBACK window_proc(HWND window, UINT message,
                                        WPARAM wparam, LPARAM lparam);
    void finish(std::optional<wardogs::CaptureRegion> region,
                QString error = {});
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);

    HWND window_{};
    bool selecting_{};
    POINT start_{};
    POINT end_{};
    HMONITOR monitor_{};
    Callback callback_;
};
