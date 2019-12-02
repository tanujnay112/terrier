#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "catalog/catalog_defs.h"
#include "optimizer/optimizer_defs.h"
#include "planner/plannodes/abstract_plan_node.h"

namespace terrier::planner {

class SortKey {
 public:
  /**
   * Constructor
   * @param expr expression to sort by
   * @param sort_type order of the sort
   */
  SortKey(common::ManagedPointer<parser::AbstractExpression> expr, optimizer::OrderByOrderingType sort_type)
  : expr_{expr}
  , sort_type_{sort_type} {}

  /**
   * Default constructor for json.
   */
  SortKey() = default;

  /**
   * @return The expression to sort by
   */
  common::ManagedPointer<parser::AbstractExpression> Expr() const {
    return expr_;
  }

  bool operator==(const SortKey& other) const {
    return expr_ == other.expr_ && sort_type_ == other.sort_type_;
  }

  bool operator!=(const SortKey& other) const {
    return !(*this == other);
  }

  /**
     * @return column serialized to json
     */
  nlohmann::json ToJson() const {
    nlohmann::json j;
    j["expr"] = *expr_;
    j["sort_type"] = sort_type_;
    return j;
  }

  /**
   * @param j json to deserialize
   */
  std::unique_ptr<parser::AbstractExpression> FromJson(const nlohmann::json &j) {
    sort_type_ = j.at("sort_type").get<optimizer::OrderByOrderingType>();
    if (!j.at("expr").is_null()) {
      auto deserialized = parser::DeserializeExpression(j.at("expr"));
      expr_ = common::ManagedPointer(deserialized.result_);
      return std::move(deserialized.result_);
    }
    return nullptr;
  }

  /**
   * @return The order of the sort
   */
  optimizer::OrderByOrderingType SortType() const {
    return sort_type_;
  }
 private:
  common::ManagedPointer<parser::AbstractExpression> expr_{nullptr};
  optimizer::OrderByOrderingType sort_type_{};
};


/**
 * Plan node for order by operator
 */
class OrderByPlanNode : public AbstractPlanNode {
 public:
  /**
   * Builder for order by plan node
   */
  class Builder : public AbstractPlanNode::Builder<Builder> {
   public:
    Builder() = default;

    /**
     * Don't allow builder to be copied or moved
     */
    DISALLOW_COPY_AND_MOVE(Builder);

    /**
     * @param key column id for key to sort
     * @param ordering ordering (ASC or DESC) for key
     * @return builder object
     */
    Builder &AddSortKey(common::ManagedPointer<parser::AbstractExpression> key, optimizer::OrderByOrderingType ordering) {
      sort_keys_.emplace_back(key, ordering);
      return *this;
    }

    /**
     * @param limit number of tuples to limit to
     * @return builder object
     */
    Builder &SetLimit(size_t limit) {
      limit_ = limit;
      has_limit_ = true;
      return *this;
    }

    /**
     * @param offset offset for where to limit from
     * @return builder object
     */
    Builder &SetOffset(size_t offset) {
      offset_ = offset;
      return *this;
    }

    /**
     * Build the order by plan node
     * @return plan node
     */
    std::unique_ptr<OrderByPlanNode> Build() {
      return std::unique_ptr<OrderByPlanNode>(new OrderByPlanNode(std::move(children_), std::move(output_schema_),
                                                                  std::move(sort_keys_), has_limit_, limit_, offset_));
    }

   protected:
    /**
     * Column Ids and ordering type ([ASC] or [DESC]) used (in order) to sort input tuples
     */
    std::vector<SortKey> sort_keys_;
    /**
     * true if sort has a defined limit. False by default
     */
    bool has_limit_ = false;
    /**
     * limit for sort
     */
    size_t limit_ = 0;
    /**
     * offset for sort
     */
    size_t offset_ = 0;
  };

 private:
  /**
   * @param children child plan nodes
   * @param output_schema Schema representing the structure of the output of this plan node
   * @param sort_keys keys on which to sort on
   * @param sort_key_orderings orderings for each sort key (ASC or DESC). Same size as sort_keys
   * @param has_limit true if operator should perform a limit
   * @param limit number of tuples to limit output to
   * @param offset offset in sort from where to limit from
   */
  OrderByPlanNode(std::vector<std::unique_ptr<AbstractPlanNode>> &&children,
                  std::unique_ptr<OutputSchema> output_schema, std::vector<SortKey> sort_keys, bool has_limit,
                  size_t limit, size_t offset)
      : AbstractPlanNode(std::move(children), std::move(output_schema)),
        sort_keys_(std::move(sort_keys)),
        has_limit_(has_limit),
        limit_(limit),
        offset_(offset) {}

 public:
  /**
   * Default constructor used for deserialization
   */
  OrderByPlanNode() = default;

  DISALLOW_COPY_AND_MOVE(OrderByPlanNode)

  /**
   * @return keys to sort on
   */
  const std::vector<SortKey> &GetSortKeys() const { return sort_keys_; }

  /**
   * @return the type of this plan node
   */
  PlanNodeType GetPlanNodeType() const override { return PlanNodeType::ORDERBY; }

  /**
   * @return true if sort has a defined limit
   */
  bool HasLimit() const { return has_limit_; }

  /**
   * Should only be used if sort has limit
   * @return limit for sort
   */
  size_t GetLimit() const {
    TERRIER_ASSERT(HasLimit(), "OrderBy plan has no limit");
    return limit_;
  }

  /**
   * Should only be used if sort has limit
   * @return offset for sort
   */
  size_t GetOffset() const {
    TERRIER_ASSERT(HasLimit(), "OrderBy plan has no limit");
    return offset_;
  }

  /**
   * @return the hashed value of this plan node
   */
  common::hash_t Hash() const override;

  bool operator==(const AbstractPlanNode &rhs) const override;

  nlohmann::json ToJson() const override;
  std::vector<std::unique_ptr<parser::AbstractExpression>> FromJson(const nlohmann::json &j) override;

 private:
  /* Column Ids and ordering type ([ASC] or [DESC]) used (in order) to sort input tuples */
  std::vector<SortKey> sort_keys_;

  /* Whether there is limit clause */
  bool has_limit_;

  /* How many tuples to return */
  size_t limit_;

  /* How many tuples to skip first */
  size_t offset_;
};

DEFINE_JSON_DECLARATIONS(SortKey);
DEFINE_JSON_DECLARATIONS(OrderByPlanNode);

}  // namespace terrier::planner
