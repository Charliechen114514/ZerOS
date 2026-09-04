#pragma once

// Life channels, so do Zeros

namespace ZerOS::log {
using ActLog = bool (*)(const char*);

void RegisterGlobalChannel(ActLog action);
ActLog FetchGlobalChannel();

} // namespace ZerOS::log
