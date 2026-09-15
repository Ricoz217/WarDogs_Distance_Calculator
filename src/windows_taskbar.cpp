#include "windows_taskbar.hpp"

#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <string>

namespace wardogs_ui {
namespace {

constexpr wchar_t application_id[] =
    L"Rico217.WarDogsDistanceCalculator";
constexpr wchar_t unlock_event_name[] =
    L"Local\\Rico217.WarDogsDistanceCalculator.UnlockPinned";

std::wstring executable_path() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    return path;
}

}  // namespace

void configure_taskbar_identity() {
    SetCurrentProcessExplicitAppUserModelID(application_id);
}

bool install_unlock_jump_list_task() {
    using Microsoft::WRL::ComPtr;

    const auto path = executable_path();
    if (path.empty()) return false;

    ComPtr<ICustomDestinationList> destination_list;
    if (FAILED(CoCreateInstance(CLSID_DestinationList, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&destination_list))))
        return false;
    if (FAILED(destination_list->SetAppID(application_id))) return false;

    UINT maximum_slots = 0;
    ComPtr<IObjectArray> removed_items;
    if (FAILED(destination_list->BeginList(
            &maximum_slots, IID_PPV_ARGS(&removed_items))))
        return false;

    ComPtr<IObjectCollection> tasks;
    if (FAILED(CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&tasks)))) {
        destination_list->AbortList();
        return false;
    }

    ComPtr<IShellLinkW> link;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&link))) ||
        FAILED(link->SetPath(path.c_str())) ||
        FAILED(link->SetArguments(L"--unlock-pinned")) ||
        FAILED(link->SetDescription(L"取消固定浮窗")) ||
        FAILED(link->SetIconLocation(path.c_str(), 0))) {
        destination_list->AbortList();
        return false;
    }

    ComPtr<IPropertyStore> properties;
    if (FAILED(link.As(&properties))) {
        destination_list->AbortList();
        return false;
    }
    PROPVARIANT title{};
    if (FAILED(InitPropVariantFromString(L"取消固定浮窗", &title))) {
        destination_list->AbortList();
        return false;
    }
    const HRESULT title_result = properties->SetValue(PKEY_Title, title);
    PropVariantClear(&title);
    if (FAILED(title_result) || FAILED(properties->Commit()) ||
        FAILED(tasks->AddObject(link.Get()))) {
        destination_list->AbortList();
        return false;
    }

    ComPtr<IObjectArray> task_array;
    if (FAILED(tasks.As(&task_array)) ||
        FAILED(destination_list->AddUserTasks(task_array.Get())) ||
        FAILED(destination_list->CommitList())) {
        destination_list->AbortList();
        return false;
    }
    return true;
}

HANDLE create_pinned_unlock_event() {
    return CreateEventW(nullptr, FALSE, FALSE, unlock_event_name);
}

bool signal_pinned_unlock_event() {
    const HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE,
                                    unlock_event_name);
    if (!event) return false;
    const bool signaled = SetEvent(event) != FALSE;
    CloseHandle(event);
    return signaled;
}

bool consume_pinned_unlock_event(HANDLE event) {
    return event && WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
}

}  // namespace wardogs_ui
