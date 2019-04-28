#pragma once

#include <memory>
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

namespace tpl::compiler {
extern std::shared_ptr<spdlog::logger> compiler_logger;

void init_compiler_logger();
}  // namespace terrier::storage

#define COMPILER_LOG_TRACE(...) ::tpl::compiler::compiler_logger->trace(__VA_ARGS__);

#define COMPILER_LOG_DEBUG(...) ::tpl::compiler::compiler_logger->debug(__VA_ARGS__);

#define COMPILER_LOG_INFO(...) ::tpl::compiler::compiler_logger->info(__VA_ARGS__);

#define COMPILER_LOG_WARN(...) ::tpl::compiler::compiler_logger->warn(__VA_ARGS__);

#define COMPILER_LOG_ERROR(...) ::tpl::compiler::compiler_logger->error(__VA_ARGS__);
