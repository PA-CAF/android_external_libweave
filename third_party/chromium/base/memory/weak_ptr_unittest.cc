// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"

#include <string>

#include <gtest/gtest.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"

namespace base {
namespace {

struct Base {
  std::string member;
};
struct Derived : public Base {};

struct TargetBase {};
struct Target : public TargetBase, public SupportsWeakPtr<Target> {
  virtual ~Target() {}
};
struct DerivedTarget : public Target {};
struct Arrow {
  WeakPtr<Target> target;
};
struct TargetWithFactory : public Target {
  TargetWithFactory() : factory(this) {}
  WeakPtrFactory<Target> factory;
};

}  // namespace

TEST(WeakPtrFactoryTest, Basic) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
}

TEST(WeakPtrFactoryTest, Comparison) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = ptr;
  EXPECT_EQ(ptr.get(), ptr2.get());
}

TEST(WeakPtrFactoryTest, OutOfScope) {
  WeakPtr<int> ptr;
  EXPECT_EQ(NULL, ptr.get());
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();
  }
  EXPECT_EQ(NULL, ptr.get());
}

TEST(WeakPtrFactoryTest, Multiple) {
  WeakPtr<int> a, b;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    b = factory.GetWeakPtr();
    EXPECT_EQ(&data, a.get());
    EXPECT_EQ(&data, b.get());
  }
  EXPECT_EQ(NULL, a.get());
  EXPECT_EQ(NULL, b.get());
}

TEST(WeakPtrFactoryTest, MultipleStaged) {
  WeakPtr<int> a;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    {
      WeakPtr<int> b = factory.GetWeakPtr();
    }
    EXPECT_TRUE(NULL != a.get());
  }
  EXPECT_EQ(NULL, a.get());
}

TEST(WeakPtrFactoryTest, Dereference) {
  Base data;
  data.member = "123456";
  WeakPtrFactory<Base> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_EQ(data.member, (*ptr).member);
  EXPECT_EQ(data.member, ptr->member);
}

TEST(WeakPtrFactoryTest, UpCast) {
  Derived data;
  WeakPtrFactory<Derived> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  ptr = factory.GetWeakPtr();
  EXPECT_EQ(ptr.get(), &data);
}

TEST(WeakPtrTest, SupportsWeakPtr) {
  Target target;
  WeakPtr<Target> ptr = target.AsWeakPtr();
  EXPECT_EQ(&target, ptr.get());
}

TEST(WeakPtrTest, DerivedTarget) {
  DerivedTarget target;
  WeakPtr<DerivedTarget> ptr = AsWeakPtr(&target);
  EXPECT_EQ(&target, ptr.get());
}

TEST(WeakPtrFactoryTest, BooleanTesting) {
  int data;
  WeakPtrFactory<int> factory(&data);

  WeakPtr<int> ptr_to_an_instance = factory.GetWeakPtr();
  EXPECT_TRUE(ptr_to_an_instance);
  EXPECT_FALSE(!ptr_to_an_instance);

  if (ptr_to_an_instance) {
  } else {
    ADD_FAILURE() << "Pointer to an instance should result in true.";
  }

  if (!ptr_to_an_instance) {  // check for operator!().
    ADD_FAILURE() << "Pointer to an instance should result in !x being false.";
  }

  WeakPtr<int> null_ptr;
  EXPECT_FALSE(null_ptr);
  EXPECT_TRUE(!null_ptr);

  if (null_ptr) {
    ADD_FAILURE() << "Null pointer should result in false.";
  }

  if (!null_ptr) {  // check for operator!().
  } else {
    ADD_FAILURE() << "Null pointer should result in !x being true.";
  }
}

TEST(WeakPtrTest, InvalidateWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_EQ(NULL, ptr.get());
  EXPECT_FALSE(factory.HasWeakPtrs());

  // Test that the factory can create new weak pointers after a
  // InvalidateWeakPtrs call, and they remain valid until the next
  // InvalidateWeakPtrs call.
  WeakPtr<int> ptr2 = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr2.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_EQ(NULL, ptr2.get());
  EXPECT_FALSE(factory.HasWeakPtrs());
}

TEST(WeakPtrTest, HasWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  {
    WeakPtr<int> ptr = factory.GetWeakPtr();
    EXPECT_TRUE(factory.HasWeakPtrs());
  }
  EXPECT_FALSE(factory.HasWeakPtrs());
}

}  // namespace base
