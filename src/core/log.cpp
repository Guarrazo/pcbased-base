#include "core/log.h"
#include <cstdio>
#include <cstdarg>

namespace pas::core {

namespace {
LogSinkFn g_sink = nullptr;

// Sink por defecto: stdout. platform/switch/main.cpp reemplaza esto por un
// sink que escribe a sdmc:/ (la consola homebrew no siempre tiene una
// terminal visible salvo con nxlink).
void DefaultSink(LogLevel, const char* tag, const char* message) {
    std::printf("[%s] %s\n", tag, message);
}
} // namespace

void SetLogSink(LogSinkFn sink) {
    g_sink = sink ? sink : DefaultSink;
}

void LogImpl(LogLevel level, const char* tag, const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    LogSinkFn sink = g_sink ? g_sink : DefaultSink;
    sink(level, tag, buffer);
}

} // namespace pas::core
