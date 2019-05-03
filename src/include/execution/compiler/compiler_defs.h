#pragma once

// TODO(WAN): shouldn't planner give me all the nodes in one nice file
#include "planner/plannodes/abstract_plan_node.h"
#include "planner/plannodes/aggregate_plan_node.h"
#include "planner/plannodes/seq_scan_plan_node.h"

#include "util/common.h"

namespace tpl::compiler {
static SourcePosition DUMMY_POS{0, 0};
}