#pragma once

#include <Windows.h>

namespace wardogs_ui {

void configure_taskbar_identity();
bool install_unlock_jump_list_task();

HANDLE create_pinned_unlock_event();
bool signal_pinned_unlock_event();
bool consume_pinned_unlock_event(HANDLE event);

}  // namespace wardogs_ui
