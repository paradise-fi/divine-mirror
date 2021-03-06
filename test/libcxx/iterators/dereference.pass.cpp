/* TAGS: c++ fin */
/* CC_OPTS: -std=c++2a */
/* VERIFY_OPTS: -o nofail:malloc */
//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <iterator>

// class ostream_iterator

// ostream_iterator& operator*() const;

#include <iterator>
#include <sstream>
#include <cassert>

#include "test_macros.h"

int main(int, char**)
{
    std::ostringstream os;
    std::ostream_iterator<int> i(os);
    std::ostream_iterator<int>& iref = *i;
    assert(&iref == &i);

  return 0;
}
