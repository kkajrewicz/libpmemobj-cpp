//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Copyright 2019, Intel Corporation
//
// Modified to test pmem::obj containers
//

#include "unittest.hpp"

#include <libpmemobj++/experimental/string.hpp>

namespace nvobj = pmem::obj;
namespace pmem_exp = pmem::obj::experimental;
using S = pmem_exp::string;

struct root {
	nvobj::persistent_ptr<S> s;
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
		path, "string_test", PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);

	auto r = pop.root();

	try {
		nvobj::transaction::run(pop, [&] {
			r->s = nvobj::make_persistent<S>("hello world");
		});

		*r->s = {};
		UT_ASSERT(r->s->empty());

		nvobj::transaction::run(
			pop, [&] { nvobj::delete_persistent<S>(r->s); });
	} catch (std::exception &e) {
		UT_FATALexc(e);
	}

	try {
		nvobj::persistent_ptr<S> tmp;
		nvobj::transaction::run(pop, [&] {
			r->s = nvobj::make_persistent<S>("hello world");
			tmp = nvobj::make_persistent<S>("abc", 2U);
		});

		*r->s = *tmp;
		UT_ASSERT(*r->s == "ab");

		nvobj::transaction::run(pop, [&] {
			nvobj::delete_persistent<S>(r->s);
			nvobj::delete_persistent<S>(tmp);
		});
	} catch (std::exception &e) {
		UT_FATALexc(e);
	}

	pop.close();

	return 0;
}
