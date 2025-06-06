/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <folly/executors/IOThreadPoolExecutor.h>
#include <gtest/gtest.h>

#include "velox/common/caching/SsdCache.h"
#include "velox/core/Expressions.h"
#include "velox/core/PlanNode.h"
#include "velox/exec/HashProbe.h"
#include "velox/exec/tests/utils/QueryAssertions.h"
#include "velox/parse/ExpressionsParser.h"
#include "velox/vector/tests/utils/VectorMaker.h"
#include "velox/vector/tests/utils/VectorTestBase.h"

namespace facebook::velox::exec::test {
class OperatorTestBase : public virtual testing::Test,
                         public velox::test::VectorTestBase {
 public:
  /// The following methods are used by google unit test framework to do
  /// one-time setup/teardown for all the unit tests from OperatorTestBase. We
  /// make them public as some benchmark like ReduceAgg also call these methods
  /// to setup/teardown benchmark test environment.
  static void SetUpTestCase();
  static void TearDownTestCase();

  /// Sets up the velox memory system.
  ///
  /// NOTE: a second call to this will clear the previous memory system
  /// instances and create a new set.
  static void setupMemory(
      int64_t allocatorCapacity,
      int64_t arbitratorCapacity,
      int64_t arbitratorReservedCapacity,
      int64_t memoryPoolInitCapacity,
      int64_t memoryPoolReservedCapacity,
      int64_t memoryPoolMinReclaimBytes,
      int64_t memoryPoolAbortCapacityLimit);

  static void resetMemory();

 protected:
  OperatorTestBase();
  ~OperatorTestBase() override;

  void SetUp() override;

  void TearDown() override;

  /// Allow base classes to register custom vector serde.
  /// By default, registers Presto-compatible serde.
  virtual void registerVectorSerde();

  void createDuckDbTable(const std::vector<RowVectorPtr>& data) {
    duckDbQueryRunner_.createTable("tmp", data);
  }

  void createDuckDbTable(
      const std::string& tableName,
      const std::vector<RowVectorPtr>& data) {
    duckDbQueryRunner_.createTable(tableName, data);
  }

  std::shared_ptr<Task> assertQueryOrdered(
      const core::PlanNodePtr& plan,
      const std::string& duckDbSql,
      const std::vector<uint32_t>& sortingKeys) {
    return test::assertQuery(plan, duckDbSql, duckDbQueryRunner_, sortingKeys);
  }

  std::shared_ptr<Task> assertQueryOrdered(
      const CursorParameters& params,
      const std::string& duckDbSql,
      const std::vector<uint32_t>& sortingKeys) {
    return test::assertQuery(
        params, [&](auto*) {}, duckDbSql, duckDbQueryRunner_, sortingKeys);
  }

  /// Assumes plan has a single leaf node. All splits are added to that node.
  std::shared_ptr<Task> assertQueryOrdered(
      const core::PlanNodePtr& plan,
      const std::vector<std::shared_ptr<connector::ConnectorSplit>>& splits,
      const std::string& duckDbSql,
      const std::vector<uint32_t>& sortingKeys) {
    return assertQuery(plan, splits, duckDbSql, sortingKeys);
  }

  std::shared_ptr<Task> assertQuery(
      const CursorParameters& params,
      const std::string& duckDbSql) {
    return test::assertQuery(
        params, [&](exec::Task* /*task*/) {}, duckDbSql, duckDbQueryRunner_);
  }

  std::shared_ptr<Task> assertQuery(
      const core::PlanNodePtr& plan,
      const std::string& duckDbSql) {
    return test::assertQuery(plan, duckDbSql, duckDbQueryRunner_);
  }

  std::shared_ptr<Task> assertQuery(
      const core::PlanNodePtr& plan,
      const RowVectorPtr& expectedResults) {
    return test::assertQuery(plan, {expectedResults});
  }

  std::shared_ptr<Task> assertQuery(
      const CursorParameters& params,
      const RowVectorPtr& expectedResults) {
    return test::assertQuery(params, {expectedResults});
  }

  /// Assumes plan has a single leaf node. All splits are added to that node.
  std::shared_ptr<Task> assertQuery(
      const core::PlanNodePtr& plan,
      const std::vector<std::shared_ptr<connector::ConnectorSplit>>&
          connectorSplits,
      const std::string& duckDbSql,
      std::optional<std::vector<uint32_t>> sortingKeys = std::nullopt);

  /// Assumes plan has a single leaf node. All splits are added to that node.
  std::shared_ptr<Task> assertQuery(
      const core::PlanNodePtr& plan,
      std::vector<exec::Split>&& splits,
      const std::string& duckDbSql,
      std::optional<std::vector<uint32_t>> sortingKeys = std::nullopt);

  std::shared_ptr<Task> assertQuery(
      const core::PlanNodePtr& plan,
      std::unordered_map<core::PlanNodeId, std::vector<exec::Split>>&& splits,
      const std::string& duckDbSql,
      std::optional<std::vector<uint32_t>> sortingKeys = std::nullopt);

  static RowTypePtr makeRowType(std::vector<TypePtr>&& types) {
    return velox::test::VectorMaker::rowType(
        std::forward<std::vector<TypePtr>&&>(types));
  }

  static std::shared_ptr<core::FieldAccessTypedExpr> toFieldExpr(
      const std::string& name,
      const RowTypePtr& rowType);

  core::TypedExprPtr parseExpr(
      const std::string& text,
      RowTypePtr rowType,
      const parse::ParseOptions& options = {});

 public:
  static void deleteTaskAndCheckSpillDirectory(std::shared_ptr<Task>& task);

 protected:
  // Used as default AsyncDataCache.
  static inline std::shared_ptr<cache::AsyncDataCache> asyncDataCache_;

  DuckDbQueryRunner duckDbQueryRunner_;

  // Used for driver thread execution.
  std::unique_ptr<folly::CPUThreadPoolExecutor> driverExecutor_;

  // Used for IO prefetch and spilling.
  std::unique_ptr<folly::IOThreadPoolExecutor> ioExecutor_;
};
} // namespace facebook::velox::exec::test
