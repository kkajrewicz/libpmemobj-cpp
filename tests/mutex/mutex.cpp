/*
 * Copyright 2016-2018, Intel Corporation
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

/*
 * obj_cpp_mutex.cpp -- cpp mutex test
 */

#include "unittest.hpp"

#include <libpmemobj++/mutex.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

#include <mutex>
#include <thread>

#define LAYOUT "cpp"

namespace nvobj = pmem::obj;

namespace
{

/* pool root structure */
struct root {
	nvobj::mutex pmutex;
	unsigned counter;
};

/* number of ops per thread */
const unsigned num_ops = 200;

/* the number of threads */
const unsigned num_threads = 30;

/*
 * increment_pint -- (internal) test the mutex with an std::lock_guard
 */
void
increment_pint(nvobj::persistent_ptr<struct root> proot)
{
	for (unsigned i = 0; i < num_ops; ++i) {
		std::lock_guard<nvobj::mutex> lock(proot->pmutex);
		(proot->counter)++;
	}
}

/*
 * decrement_pint -- (internal) test the mutex with an std::unique_lock
 */
void
decrement_pint(nvobj::persistent_ptr<struct root> proot)
{
	std::unique_lock<nvobj::mutex> lock(proot->pmutex);
	for (unsigned i = 0; i < num_ops; ++i)
		--(proot->counter);

	lock.unlock();
}

/*
 * trylock_test -- (internal) test the trylock implementation
 */
void
trylock_test(nvobj::persistent_ptr<struct root> proot)
{
	for (;;) {
		if (proot->pmutex.try_lock()) {
			(proot->counter)++;
			proot->pmutex.unlock();
			return;
		}
	}
}

/*
 * mutex_zero_test -- (internal) test the zeroing constructor
 */
void
mutex_zero_test(nvobj::pool<struct root> &pop)
{
	PMEMoid raw_mutex;

	pmemobj_alloc(pop.handle(), &raw_mutex, sizeof(PMEMmutex), 1,
		      [](PMEMobjpool *pop, void *ptr, void *) -> int {
			      PMEMmutex *mtx = static_cast<PMEMmutex *>(ptr);
			      pmemobj_memset_persist(pop, mtx, 1, sizeof(*mtx));
			      return 0;
		      },
		      nullptr);

	nvobj::mutex *placed_mtx = new (pmemobj_direct(raw_mutex)) nvobj::mutex;
	std::unique_lock<nvobj::mutex> lck(*placed_mtx);
}

/*
 * mutex_test -- (internal) launch worker threads to test the pmutex
 */
template <typename Worker>
void
mutex_test(nvobj::pool<struct root> &pop, const Worker &function)
{
	std::thread threads[num_threads];

	nvobj::persistent_ptr<struct root> proot = pop.root();

	for (unsigned i = 0; i < num_threads; ++i)
		threads[i] = std::thread(function, proot);

	for (unsigned i = 0; i < num_threads; ++i)
		threads[i].join();
}

void
test_stack()
{
	/* mutex is not allowed outside of pmem */
	try {
		nvobj::mutex stack_mutex;
		UT_ASSERT(0);
	} catch (pmem::lock_error &le) {
	} catch (...) {
		UT_ASSERT(0);
	}
}

void
test_error_handling(nvobj::pool<root> &pop)
{
	nvobj::persistent_ptr<root> proot = pop.root();

	proot->pmutex.lock();

	/* try_locking already taken lock fails with false */
	UT_ASSERT(proot->pmutex.try_lock() == false);

	proot->pmutex.unlock();
}
}

int
main(int argc, char *argv[])
{
	START();

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	nvobj::pool<struct root> pop;

	try {
		pop = nvobj::pool<struct root>::create(
			path, LAYOUT, PMEMOBJ_MIN_POOL, S_IWUSR | S_IRUSR);
	} catch (pmem::pool_error &pe) {
		UT_FATAL("!pool::create: %s %s", pe.what(), path);
	}

	mutex_zero_test(pop);

	mutex_test(pop, increment_pint);
	UT_ASSERTeq(pop.root()->counter, num_threads * num_ops);

	mutex_test(pop, decrement_pint);
	UT_ASSERTeq(pop.root()->counter, 0);

	mutex_test(pop, trylock_test);
	UT_ASSERTeq(pop.root()->counter, num_threads);

	/* pmemcheck related persist */
	pmemobj_persist(pop.handle(), &(pop.root()->counter),
			sizeof(pop.root()->counter));

	test_stack();
	test_error_handling(pop);

	pop.close();

	return 0;
}
