// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/states/state_manager.h"

#include <cstdlib>  // for abs().
#include <vector>

#include <base/bind.h>
#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "libweave/src/commands/schema_constants.h"
#include "libweave/src/commands/unittest_utils.h"
#include "libweave/src/states/error_codes.h"
#include "libweave/src/states/mock_state_change_queue_interface.h"

namespace weave {

using testing::_;
using testing::Return;
using unittests::CreateDictionaryValue;

namespace {
std::unique_ptr<base::DictionaryValue> GetTestSchema() {
  return CreateDictionaryValue(R"({
    'base': {
      'manufacturer':'string',
      'serialNumber':'string'
    },
    'device': {
      'state_property':'string'
    }
  })");
}

std::unique_ptr<base::DictionaryValue> GetTestValues() {
  return CreateDictionaryValue(R"({
    'base': {
      'manufacturer':'Test Factory',
      'serialNumber':'Test Model'
    }
  })");
}

}  // anonymous namespace

class StateManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Initial expectations.
    EXPECT_CALL(mock_state_change_queue_, IsEmpty()).Times(0);
    EXPECT_CALL(mock_state_change_queue_, NotifyPropertiesUpdated(_, _))
        .Times(0);
    EXPECT_CALL(mock_state_change_queue_, GetAndClearRecordedStateChanges())
        .Times(0);
    mgr_.reset(new StateManager(&mock_state_change_queue_));

    EXPECT_CALL(*this, OnStateChanged()).Times(1);
    mgr_->AddOnChangedCallback(
        base::Bind(&StateManagerTest::OnStateChanged, base::Unretained(this)));

    LoadStateDefinition(GetTestSchema().get(), "default", nullptr);
    ASSERT_TRUE(mgr_->LoadStateDefaults(*GetTestValues().get(), nullptr));
  }
  void TearDown() override { mgr_.reset(); }

  void LoadStateDefinition(const base::DictionaryValue* json,
                           const std::string& category,
                           chromeos::ErrorPtr* error) {
    ASSERT_TRUE(mgr_->LoadStateDefinition(*json, category, error));
  }

  bool SetPropertyValue(const std::string& name,
                        const chromeos::Any& value,
                        chromeos::ErrorPtr* error) {
    return mgr_->SetPropertyValue(name, value, timestamp_, error);
  }

  MOCK_CONST_METHOD0(OnStateChanged, void());

  base::Time timestamp_{base::Time::Now()};
  std::unique_ptr<StateManager> mgr_;
  MockStateChangeQueueInterface mock_state_change_queue_;
};

TEST(StateManager, Empty) {
  MockStateChangeQueueInterface mock_state_change_queue;
  StateManager manager(&mock_state_change_queue);
  EXPECT_TRUE(manager.GetCategories().empty());
}

TEST_F(StateManagerTest, Initialized) {
  EXPECT_EQ(std::set<std::string>{"default"}, mgr_->GetCategories());
  auto expected = R"({
    'base': {
      'manufacturer': 'Test Factory',
      'serialNumber': 'Test Model'
    },
    'device': {
      'state_property': ''
    }
  })";
  EXPECT_JSON_EQ(expected, *mgr_->GetStateValuesAsJson());
}

TEST_F(StateManagerTest, LoadStateDefinition) {
  auto dict = CreateDictionaryValue(R"({
    'power': {
      'battery_level':'integer'
    }
  })");
  LoadStateDefinition(dict.get(), "powerd", nullptr);
  EXPECT_EQ((std::set<std::string>{"default", "powerd"}),
            mgr_->GetCategories());

  auto expected = R"({
    'base': {
      'manufacturer': 'Test Factory',
      'serialNumber': 'Test Model'
    },
    'power': {
      'battery_level': 0
    },
    'device': {
      'state_property': ''
    }
  })";
  EXPECT_JSON_EQ(expected, *mgr_->GetStateValuesAsJson());
}

TEST_F(StateManagerTest, SetPropertyValue) {
  ValueMap expected_prop_set{
      {"device.state_property",
       unittests::make_string_prop_value("Test Value")},
  };
  EXPECT_CALL(mock_state_change_queue_,
              NotifyPropertiesUpdated(timestamp_, expected_prop_set))
      .WillOnce(Return(true));
  ASSERT_TRUE(SetPropertyValue("device.state_property",
                               std::string{"Test Value"}, nullptr));
  auto expected = R"({
    'base': {
      'manufacturer': 'Test Factory',
      'serialNumber': 'Test Model'
    },
    'device': {
      'state_property': 'Test Value'
    }
  })";
  EXPECT_JSON_EQ(expected, *mgr_->GetStateValuesAsJson());
}

TEST_F(StateManagerTest, SetPropertyValue_Error_NoName) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(SetPropertyValue("", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNameMissing, error->GetCode());
}

TEST_F(StateManagerTest, SetPropertyValue_Error_NoPackage) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(SetPropertyValue("state_property", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPackageNameMissing, error->GetCode());
}

TEST_F(StateManagerTest, SetPropertyValue_Error_UnknownPackage) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(SetPropertyValue("power.level", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNotDefined, error->GetCode());
}

TEST_F(StateManagerTest, SetPropertyValue_Error_UnknownProperty) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(SetPropertyValue("base.level", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNotDefined, error->GetCode());
}

TEST_F(StateManagerTest, GetAndClearRecordedStateChanges) {
  EXPECT_CALL(mock_state_change_queue_, NotifyPropertiesUpdated(timestamp_, _))
      .WillOnce(Return(true));
  ASSERT_TRUE(SetPropertyValue("device.state_property",
                               std::string{"Test Value"}, nullptr));
  std::vector<StateChange> expected_val;
  expected_val.emplace_back(
      timestamp_, ValueMap{{"device.state_property",
                            unittests::make_string_prop_value("Test Value")}});
  EXPECT_CALL(mock_state_change_queue_, GetAndClearRecordedStateChanges())
      .WillOnce(Return(expected_val));
  auto changes = mgr_->GetAndClearRecordedStateChanges();
  ASSERT_EQ(1, changes.second.size());
  EXPECT_EQ(expected_val.back().timestamp, changes.second.back().timestamp);
  EXPECT_EQ(expected_val.back().changed_properties,
            changes.second.back().changed_properties);
}

TEST_F(StateManagerTest, SetProperties) {
  ValueMap expected_prop_set{
      {"base.manufacturer", unittests::make_string_prop_value("No Name")},
  };
  EXPECT_CALL(mock_state_change_queue_,
              NotifyPropertiesUpdated(_, expected_prop_set))
      .WillOnce(Return(true));

  EXPECT_CALL(*this, OnStateChanged()).Times(1);
  ASSERT_TRUE(mgr_->SetProperties(
      {{"base.manufacturer", std::string("No Name")}}, nullptr));

  auto expected = R"({
    'base': {
      'manufacturer': 'No Name',
      'serialNumber': 'Test Model'
    },
    'device': {
      'state_property': ''
    }
  })";
  EXPECT_JSON_EQ(expected, *mgr_->GetStateValuesAsJson());
}

}  // namespace weave