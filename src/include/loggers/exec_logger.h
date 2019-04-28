#pragma once

#include <memory>
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

namespace tpl::sql {
extern std::shared_ptr<spdlog::logger> exec_logger;

void init_exec_logger();
}  // namespace terrier::storage

#define EXEC_LOG_TRACE(...) ::tpl::sql::exec_logger->trace(__VA_ARGS__);

#define EXEC_LOG_DEBUG(...) ::tpl::sql::exec_logger->debug(__VA_ARGS__);

#define EXEC_LOG_INFO(...) ::tpl::sql::exec_logger->info(__VA_ARGS__);

#define EXEC_LOG_WARN(...) ::tpl::sql::exec_logger->warn(__VA_ARGS__);

#define EXEC_LOG_ERROR(...) ::tpl::sql::exec_logger->error(__VA_ARGS__);
