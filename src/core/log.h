#pragma once
// Logging minimo, independiente de plataforma. El backend real (a donde van
// los mensajes: consola serie, fichero en sdmc:/, etc.) se inyecta desde
// platform/switch/ en tiempo de arranque, para que este header no dependa
// de libnx. Ver docs/ARCHITECTURE.md #3 (separacion core/platform).

namespace pas::core {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

// Firma que debe implementar cualquier backend de log (p.ej. platform/switch
// escribira a sdmc:/arcade/logs/). Se registra via SetLogSink().
using LogSinkFn = void (*)(LogLevel level, const char* tag, const char* message);

void SetLogSink(LogSinkFn sink);

// No usar directamente: usar las macros PAS_LOG_* de abajo.
void LogImpl(LogLevel level, const char* tag, const char* fmt, ...);

#define PAS_LOG_TRACE(tag, fmt, ...) ::pas::core::LogImpl(::pas::core::LogLevel::Trace, tag, fmt, ##__VA_ARGS__)
#define PAS_LOG_DEBUG(tag, fmt, ...) ::pas::core::LogImpl(::pas::core::LogLevel::Debug, tag, fmt, ##__VA_ARGS__)
#define PAS_LOG_INFO(tag, fmt, ...)  ::pas::core::LogImpl(::pas::core::LogLevel::Info,  tag, fmt, ##__VA_ARGS__)
#define PAS_LOG_WARN(tag, fmt, ...)  ::pas::core::LogImpl(::pas::core::LogLevel::Warn,  tag, fmt, ##__VA_ARGS__)
#define PAS_LOG_ERROR(tag, fmt, ...) ::pas::core::LogImpl(::pas::core::LogLevel::Error, tag, fmt, ##__VA_ARGS__)

} // namespace pas::core
