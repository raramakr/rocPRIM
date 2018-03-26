// MIT License
//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <iostream>
#include <type_traits>

// Google Test
#include <gtest/gtest.h>
// rocPRIM API
#include <rocprim/rocprim.hpp>

#include "test_utils.hpp"

TEST(RocprimZipIteratorTests, Traits)
{
    ASSERT_TRUE((
        std::is_same<
            rocprim::zip_iterator<
                rocprim::tuple<int*, double*, char*>
            >::reference,
            rocprim::tuple<int&, double&, char&>
        >::value
    ));
    ASSERT_TRUE((
        std::is_same<
            rocprim::zip_iterator<
                rocprim::tuple<const int*, double*, const char*>
            >::reference,
            rocprim::tuple<const int&, double&, const char&>
        >::value
    ));
    auto to_double = [](const int& x) -> double { return double(x); };
    ASSERT_TRUE((
        std::is_same<
            rocprim::zip_iterator<
                rocprim::tuple<
                    rocprim::counting_iterator<int>,
                    rocprim::transform_iterator<int*, decltype(to_double)>
                >
            >::reference,
            rocprim::tuple<
                rocprim::counting_iterator<int>::reference,
                rocprim::transform_iterator<int*, decltype(to_double)>::reference
            >
        >::value
    ));
}

TEST(RocprimZipIteratorTests, Basics)
{
    int a[] = { 1, 2, 3, 4, 5};
    int b[] = { 6, 7, 8, 9, 10};
    double c[] = { 1., 2., 3., 4., 5.};
    auto iterator_tuple = rocprim::make_tuple(a, b, c);

    // Constructor
    rocprim::zip_iterator<decltype(iterator_tuple)> zit(iterator_tuple);

    // dereferencing
    ASSERT_EQ(*zit, rocprim::make_tuple(1, 6, 1.0));
    *zit = rocprim::make_tuple(1, 8, 15);
    ASSERT_EQ(*zit, rocprim::make_tuple(1, 8, 15.0));
    ASSERT_EQ(a[0], 1);
    ASSERT_EQ(b[0], 8);
    ASSERT_EQ(c[0], 15.0);
    auto ref = *zit;
    ref = rocprim::make_tuple(1, 6, 1.0);
    ASSERT_EQ(*zit, rocprim::make_tuple(1, 6, 1.0));
    ASSERT_EQ(a[0], 1);
    ASSERT_EQ(b[0], 6);
    ASSERT_EQ(c[0], 1.0);

    // inc, dec, advance
    ++zit;
    ASSERT_EQ(*zit, rocprim::make_tuple(2, 7, 2.0));
    zit++;
    ASSERT_EQ(*zit, rocprim::make_tuple(3, 8, 3.0));
    --zit;
    ASSERT_EQ(*zit, rocprim::make_tuple(2, 7, 2.0));
    zit--;
    ASSERT_EQ(*zit, rocprim::make_tuple(1, 6, 1.0));
    zit += 3;
    ASSERT_EQ(*zit, rocprim::make_tuple(4, 9, 4.0));
    zit -= 2;
    ASSERT_EQ(*zit, rocprim::make_tuple(2, 7, 2.0));

    // <, >, <=, >=, ==, !=
    auto zit2 = rocprim::zip_iterator<decltype(iterator_tuple)>(iterator_tuple);
    ASSERT_LT(zit2, zit);
    ASSERT_GT(zit, zit2);

    ASSERT_LE(zit2, zit);
    ASSERT_LE(zit, zit);
    ASSERT_LE(zit2, zit2);
    ASSERT_GE(zit, zit2);
    ASSERT_GE(zit, zit);
    ASSERT_GE(zit2, zit2);

    ASSERT_NE(zit2, zit);
    ASSERT_NE(zit, zit2);
    ASSERT_NE(zit2, ++rocprim::zip_iterator<decltype(iterator_tuple)>(iterator_tuple));

    ASSERT_EQ(zit2, zit2);
    ASSERT_EQ(zit, zit);
    ASSERT_EQ(zit2, rocprim::zip_iterator<decltype(iterator_tuple)>(iterator_tuple));

    // distance
    ASSERT_EQ(zit - zit2, 1);
    ASSERT_EQ(zit2 - zit, -1);
    ASSERT_EQ(zit - zit, 0);

    // []
    ASSERT_EQ((zit2[0]), rocprim::make_tuple(1, 6, 1.0));
    ASSERT_EQ((zit2[2]), rocprim::make_tuple(3, 8, 3.0));
    // +
    ASSERT_EQ(*(zit2+3), rocprim::make_tuple(4, 9, 4.0));
}

TEST(RocprimZipIteratorTests, Transform)
{
    using T1 = int;
    using T2 = double;
    using T3 = unsigned char;
    using U = T1;
    const bool debug_synchronous = false;
    const size_t size = 1024 * 16;

    hc::accelerator acc;
    hc::accelerator_view acc_view = acc.create_view();

    // Generate data
    std::vector<T1> input1 = test_utils::get_random_data<T1>(size, 1, 100);
    std::vector<T2> input2 = test_utils::get_random_data<T2>(size, 1, 100);
    std::vector<T3> input3 = test_utils::get_random_data<T3>(size, 1, 100);

    hc::array<T1> d_input1(hc::extent<1>(size), input1.begin(), acc_view);
    hc::array<T2> d_input2(hc::extent<1>(size), input2.begin(), acc_view);
    hc::array<T3> d_input3(hc::extent<1>(size), input3.begin(), acc_view);
    hc::array<U> d_output(size, acc_view);
    acc_view.wait();

    auto transform_op =
        [](const rocprim::tuple<int, double, unsigned char>& t) [[hc]] [[cpu]] -> int
        {
            return rocprim::get<0>(t) + rocprim::get<1>(t) + rocprim::get<2>(t);
        };

    // Calculate expected results on host
    std::vector<U> expected(input1.size());
    std::transform(
        rocprim::make_zip_iterator(
            rocprim::make_tuple(input1.begin(), input2.begin(), input3.begin())
        ),
        rocprim::make_zip_iterator(
            rocprim::make_tuple(input1.end(), input2.end(), input3.end())
        ),
        expected.begin(),
        transform_op
    );

    // Run
    rocprim::transform(
        rocprim::make_zip_iterator(
            rocprim::make_tuple(
                d_input1.accelerator_pointer(),
                d_input2.accelerator_pointer(),
                d_input3.accelerator_pointer()
            )
        ),
        d_output.accelerator_pointer(),
        input1.size(),
        transform_op,
        acc_view,
        debug_synchronous
    );
    acc_view.wait();

    // Check if output values are as expected
    std::vector<U> output = d_output;
    for(size_t i = 0; i < output.size(); i++)
    {
        SCOPED_TRACE(testing::Message() << "where index = " << i);
        auto diff = std::max<U>(std::abs(0.01f * expected[i]), U(0.01f));
        if(std::is_integral<U>::value) diff = 0;
        ASSERT_NEAR(output[i], expected[i], diff);
    }
}