#include "loggers/exec_logger.h"
#include <memory>
#include "loggers/main_logger.h"

namespace tpl::sql {

std::shared_ptr<spdlog::logger> exec_logger;

void init_exec_logger() {
  exec_logger = std::make_shared<spdlog::logger>("exec_logger", ::default_sink);
  spdlog::register_logger(exec_logger);
}

}  // namespace terrier::storage
