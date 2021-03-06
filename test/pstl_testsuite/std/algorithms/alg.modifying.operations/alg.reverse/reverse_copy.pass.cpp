// -*- C++ -*-
//===-- reverse_copy.pass.cpp ---------------------------------------------===//
//
// Copyright (C) 2017-2020 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// This file incorporates work covered by the following copyright and permission
// notice:
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
//
//===----------------------------------------------------------------------===//

#include "support/pstl_test_config.h"
#include "support/utils.h"

#include _PSTL_TEST_HEADER(execution)
#include _PSTL_TEST_HEADER(algorithm)

#include <iterator>

using namespace TestUtils;

template <typename T>
struct wrapper
{
    T t;
    wrapper(): t() {}
    explicit wrapper(T t_) : t(t_) {}
    wrapper&
    operator=(const T& t_)
    {
        t = t_;
        return *this;
    }
    bool
    operator==(const wrapper& t_) const
    {
        return t == t_.t;
    }
};

template <typename T1, typename T2>
bool
eq(const wrapper<T1>& a, const wrapper<T2>& b)
{
    return a.t == b.t;
}

template <typename T1, typename T2>
bool
eq(const T1& a, const T2& b)
{
    return a == b;
}

// we need to save state here, because we need to test with different types of iterators
// due to the caller invoke_on_all_policies does forcing modification passed iterator type to cover additional usage cases.
template <typename Iterator, typename T1, typename T2>
struct test_one_policy
{
    Iterator data_b;
    Iterator data_e;
    test_one_policy(Iterator b, Iterator e) : data_b(b), data_e(e) {}

#if _PSTL_ICC_17_VC141_TEST_SIMD_LAMBDA_DEBUG_32_BROKEN ||                                                             \
    _PSTL_ICC_16_VC14_TEST_SIMD_LAMBDA_DEBUG_32_BROKEN // dummy specialization by policy type, in case of broken configuration
    template <typename Iterator1>
    typename ::std::enable_if<is_same_iterator_category<Iterator1, ::std::random_access_iterator_tag>::value, void>::type
    operator()(oneapi::dpl::execution::unsequenced_policy, Iterator1 actual_b, Iterator1 actual_e)
    {
    }

    template <typename Iterator1>
    typename ::std::enable_if<is_same_iterator_category<Iterator1, ::std::random_access_iterator_tag>::value, void>::type
    operator()(oneapi::dpl::execution::parallel_unsequenced_policy, Iterator1 actual_b, Iterator1 actual_e)
    {
    }
#endif

    template <typename ExecutionPolicy, typename Iterator1>
    void
    operator()(ExecutionPolicy&& exec, Iterator1 actual_b, Iterator1 actual_e)
    {
        using namespace std;
        using T = typename iterator_traits<Iterator1>::value_type;

        fill(actual_b, actual_e, T(-123));
        Iterator1 actual_return = reverse_copy(exec, data_b, data_e, actual_b);

        EXPECT_TRUE(actual_return == actual_e, "wrong result of reverse_copy");

        const auto n = ::std::distance(data_b, data_e);
        Sequence<T> res(n);
        ::std::copy(::std::reverse_iterator<Iterator>(data_e), ::std::reverse_iterator<Iterator>(data_b), res.begin());

        EXPECT_EQ_N(res.begin(), actual_b, n, "wrong effect of reverse_copy");
    }
};

template <typename T1, typename T2>
void
test()
{
    typedef typename Sequence<T1>::iterator iterator_type;

    const ::std::size_t max_len = 100000;

    Sequence<T2> actual(max_len);

    Sequence<T1> data(max_len, [](::std::size_t i) { return T1(i); });

    for (::std::size_t len = 0; len < max_len; len = len <= 16 ? len + 1 : ::std::size_t(3.1415 * len))
    {
        invoke_on_all_policies<0>()(test_one_policy<iterator_type, T1, T2>(data.begin(), data.begin() + len),
                                    actual.begin(), actual.begin() + len);
//For a heterogeneous version is only valid random access iterator
#if !_ONEDPL_BACKEND_SYCL
        typedef typename Sequence<T1>::const_bidirectional_iterator cbi_iterator_type;
        invoke_on_all_policies<1>()(test_one_policy<cbi_iterator_type, T1, T2>(data.cbibegin(),
                                    ::std::next(data.cbibegin(), len)), actual.begin(), actual.begin() + len);
#endif
    }
}

int
main()
{
    // clang-3.8 fails to correctly auto vectorize the loop in some cases of different types of container's elements,
    // for example: int32_t and int8_t. This issue isn't detected for clang-3.9 and newer versions.
    test<int16_t, int8_t>();
    test<uint16_t, float32_t>();
    test<float64_t, int64_t>();

#if !_ONEDPL_BACKEND_SYCL
    test<wrapper<float64_t>, wrapper<float64_t>>();
#endif

    ::std::cout << done() << ::std::endl;
    return 0;
}
