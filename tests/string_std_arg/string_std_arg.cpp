/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "unittest.hpp"

#include <libpmemobj++/experimental/string.hpp>
#include <libpmemobj++/make_persistent.hpp>

#include <string>

namespace nvobj = pmem::obj;
namespace pmem_exp = nvobj::experimental;

using S = pmem_exp::string;

struct root {
	nvobj::persistent_ptr<S> s1, s2, s3;
};

int
main(int argc, char *argv[])
{
	START();

	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << " file-name" << std::endl;
		return 1;
	}
	auto path = argv[1];
	auto pop = nvobj::pool<root>::create(
		path, "StringTest", PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);

	auto r = pop.root();

	try {
		std::string s1(""), s2("0123456789"), 
			s3("01234567890123456789012345678901234567890123456789012345678901234567890123456789");

		nvobj::transaction::run(pop, [&] {
			r->s1 = nvobj::make_persistent<S>(s1);
			r->s2 = nvobj::make_persistent<S>(s2);
			r->s3 = nvobj::make_persistent<S>(s3);
		});

		/* validate constructors from std::string */
		UT_ASSERT(*r->s1 == s1);
		UT_ASSERT(*r->s2 == s2);
		UT_ASSERT(*r->s3 == s3);

		/* test comparison operators */
		UT_ASSERT(s1 == *r->s1);
		UT_ASSERT(s2 == *r->s2);
		UT_ASSERT(s3 == *r->s3);

		UT_ASSERT(s2 != *r->s3);
		UT_ASSERT(*r->s3 != s2);

		UT_ASSERT(s2 >= *r->s2);
		UT_ASSERT(s3 >= *r->s2);
		UT_ASSERT(*r->s2 >= s2);
		UT_ASSERT(*r->s3 >= s2);

		UT_ASSERT(s2 <= *r->s2);
		UT_ASSERT(s2 <= *r->s3);
		UT_ASSERT(*r->s2 <= s2);
		UT_ASSERT(*r->s2 <= s3);

		UT_ASSERT(s3 > *r->s2);
		UT_ASSERT(*r->s3 > s2);
		UT_ASSERT(s2 < *r->s3);
		UT_ASSERT(*r->s2 < s3);

		nvobj::transaction::run(pop, [&] {
			nvobj::delete_persistent<S>(r->s1);
			nvobj::delete_persistent<S>(r->s2);
			nvobj::delete_persistent<S>(r->s3);
		});
	} catch (std::exception &e) {
		UT_FATALexc(e);
	}

	pop.close();

	return 0;
}
