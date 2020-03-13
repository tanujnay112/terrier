#include "gtest/gtest.h"

#include "parser/udf/udf_parser.h"
#include "test_util/test_harness.h"

namespace terrier::parser::udf {

  class UDFParserTestBase : public TerrierTest {
   protected:
    /**
     * Initialization
     */
    void SetUp() override {
      parser_logger->set_level(spdlog::level::debug);
      spdlog::flush_every(std::chrono::seconds(1));
    }
  };

    // NOLINTNEXTLINE
    TEST_F(UDFParserTestBase, ConstantValueExpressionTest) {
      PLpgSQLParser udf_parser;
      UDFASTContext ast_context;
      auto result = udf_parser.ParsePLpgSQL("BEGIN; IF a < 1000 THEN RETURN a; ELSE RETURN a * 100; END IF; END;",
                                            common::ManagedPointer(&ast_context));
  }
}