#include <cmath>
#include "gtest/gtest.h"

TEST(TestTest, test_test_1) {
	int res = pow(2,2);
	EXPECT_EQ(res, 4);
}