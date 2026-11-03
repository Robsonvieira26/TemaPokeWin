// tests/framework.h
#pragma once
#include "../upstream/test/framework.h"

#define ASSERT_TRUE(cond) CHECK(cond)
#define ASSERT_EQ(a, b)   CHECK_EQ(a, b)
