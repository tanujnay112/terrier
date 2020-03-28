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
    {
      UDFASTContext ast_context;
      PLpgSQLParser udf_parser((common::ManagedPointer(&ast_context)));
      auto result = udf_parser.ParsePLpgSQL(" CREATE OR REPLACE FUNCTION dynamic() RETURNS double AS $$ "
                                            " DECLARE "
                                            " BEGIN "
                                            " RETURN 12.0;"
                                            " END; $$ "
                                            " LANGUAGE plpgsql;",
                                            common::ManagedPointer(&ast_context));
    }

//  {
//    UDFASTContext ast_context;
//    PLpgSQLParser udf_parser((common::ManagedPointer(&ast_context)));
//    auto result = udf_parser.ParsePLpgSQL("CREATE OR REPLACE FUNCTION while(a double)"
//                                          " RETURNS double AS $$ DECLARE n double := a;"
//                                          "BEGIN LOOP EXIT WHEN n > 1000; n := n + 1; END LOOP;"
//                                          " RETURN n; END; $$ LANGUAGE plpgsql;",
//                                          common::ManagedPointer(&ast_context));
//  }

  }
}