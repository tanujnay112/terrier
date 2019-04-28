#include "loggers/index_logger.h"
#include <memory>
#include "loggers/main_logger.h"

namespace tpl::compiler {

std::shared_ptr<spdlog::logger> compiler_logger;

void init_compiler_logger() {
  compiler_logger = std::make_shared<spdlog::logger>("compiler_logger", ::default_sink);
  spdlog::register_logger(compiler_logger);
}

}  // namespace terrier::storage
