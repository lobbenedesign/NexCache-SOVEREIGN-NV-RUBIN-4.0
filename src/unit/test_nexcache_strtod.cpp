/*
 * Copyright (c) NexCache Contributors
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "generated_wrappers.hpp"

#include <cerrno>
#include <math.h>

extern "C" {
#include "nexcache_strtod.h"
}

class NexCacheStrtodTest : public ::testing::Test {};

TEST_F(NexCacheStrtodTest, TestNexCacheStrtod) {
    errno = 0;
    double value = nexcache_strtod("231.2341234", NULL);
    ASSERT_DOUBLE_EQ(value, 231.2341234);
    ASSERT_EQ(errno, 0);

    value = nexcache_strtod("+inf", NULL);
    ASSERT_TRUE(isinf(value));
    ASSERT_EQ(errno, 0);

    value = nexcache_strtod("-inf", NULL);
    ASSERT_TRUE(isinf(value));
    ASSERT_EQ(errno, 0);

    value = nexcache_strtod("inf", NULL);
    ASSERT_TRUE(isinf(value));
    ASSERT_EQ(errno, 0);
}
