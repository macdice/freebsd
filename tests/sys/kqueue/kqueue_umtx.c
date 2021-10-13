/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/umtx.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>

ATF_TC_WITHOUT_HEAD(main);

ATF_TC_BODY(main, tc)
{
	struct timespec zero_timeout = {0, 0};
	u_long *shmem;
	int shmem_fd;
	int rv;

	shmem_fd = shm_open(SHM_ANON, O_CREAT | O_RDWR, 0);
	ATF_REQUIRE(shmem_fd >= 0);

	rv = ftruncate(shmem_fd, sizeof(*shmem));
	ATF_REQUIRE(rv == 0);

	shmem = mmap(NULL, sizeof(*shmem), PROT_READ | PROT_WRITE,
	    MAP_SHARED, shmem_fd, 0);
	ATF_REQUIRE(shmem != MAP_FAILED);

	int kq = kqueue();
	ATF_REQUIRE(kq >= 0);

	struct kevent kev;
	struct kevent out_kev;

	/* Fires immediately because value is not as expected. */
	*shmem = 0;
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_ADD,
	    NOTE_UMTX_WAIT_ULONG, 42, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(1, rv);
	ATF_CHECK_EQ(out_kev.ident, (uintptr_t) shmem);

	/* Doesn't fire immediately because value is as expected. */
	*shmem = 42;
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_ADD,
	    NOTE_UMTX_WAIT_ULONG, 42, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(0, rv);

	/*
	 * Doesn't fire even though value is not as expected, because it's
	 * already been added and we don't recheck; we're already in the umtx
	 * wait queue.
	 */
	*shmem = 42;
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_ADD,
	    NOTE_UMTX_WAIT_ULONG, 43, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(0, rv);

	/* Remove it. */
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_DELETE,
	    NOTE_UMTX_WAIT_ULONG, 0, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(0, rv);

	/* Add it again, with the wrong value.  Now it fires. */
	*shmem = 42;
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_ADD,
	    NOTE_UMTX_WAIT_ULONG, 43, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(1, rv);

	/* Try to wake it. */
	rv = _umtx_op(shmem, UMTX_OP_WAKE, 1, NULL, NULL);
	ATF_REQUIRE_EQ(0, rv);

	/* No event should be receive (it was one-shot). */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(0, rv);

	/* Add it again, with the right value.  Not woken yet. */
	*shmem = 42;
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_ADD,
	    NOTE_UMTX_WAIT_ULONG, 42, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(0, rv);

	/* Try to wake some other umtx address. */
	rv = _umtx_op(shmem + 1, UMTX_OP_WAKE, 1, NULL, NULL);
	ATF_REQUIRE_EQ(0, rv);

	/* Shouldn't be woken, because that was the wrong address. */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(0, rv);

	/* Try to wake it with the right address. */
	rv = _umtx_op(shmem, UMTX_OP_WAKE, 1, NULL, NULL);
	ATF_REQUIRE_EQ(0, rv);

	/* Should be woken. */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(1, rv);
	ATF_CHECK_EQ(out_kev.ident, (uintptr_t) shmem);

	/* Try to wake it again. */
	rv = _umtx_op(shmem, UMTX_OP_WAKE, 1, NULL, NULL);
	ATF_REQUIRE_EQ(0, rv);

	/* Shouldn't be woken, because it was one-shot. */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(0, rv);

	/* Add.  Not woken yet. */
	*shmem = 42;
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_ADD,
	    NOTE_UMTX_WAIT_ULONG, 42, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(0, rv);

	/* Wake it from another process. */
	pid_t pid = fork();
	if (pid == 0) {
		/* Child wakes. */
		_umtx_op(shmem, UMTX_OP_WAKE, 1, NULL, NULL);
		_Exit(0);
	} else {
		/* Parent waits for child. */
		ATF_REQUIRE(pid > 0);
		int status;
		wait(&status);
	}

	/* Should be woken. */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(1, rv);
	ATF_CHECK_EQ(out_kev.ident, (uintptr_t) shmem);

	/* Try to wake it again. */
	rv = _umtx_op(shmem, UMTX_OP_WAKE, 1, NULL, NULL);
	ATF_REQUIRE_EQ(0, rv);

	/* Shouldn't be woken, because it was one-shot. */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(0, rv);

	/* Add, in process-private namespace.  Not woken yet. */
	*shmem = 42;
	EV_SET(&kev, (uintptr_t) shmem, EVFILT_UMTX, EV_ADD,
	    NOTE_UMTX_WAIT_ULONG | NOTE_UMTX_WAIT_PRIVATE, 42, 0);
	rv = kevent(kq, &kev, 1, &out_kev, 1, &zero_timeout);
	ATF_REQUIRE_EQ(0, rv);

	/* Try to wake it from another process. */
	pid = fork();
	if (pid == 0) {
		/* Child wakes. */
		_umtx_op(shmem, UMTX_OP_WAKE, 1, NULL, NULL);
		_Exit(0);
	} else {
		/* Parent waits for child. */
		ATF_REQUIRE(pid > 0);
		int status;
		wait(&status);
	}

	/* Shouldn't be woken, because other process couldn't see it. */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(0, rv);

	/* This process can see it. */
	_umtx_op(shmem, UMTX_OP_WAKE_PRIVATE, 1, NULL, NULL);

	/* Should be woken. */
	rv = kevent(kq, NULL, 0, &out_kev, 1, &zero_timeout);
	ATF_CHECK_EQ(1, rv);
	ATF_CHECK_EQ(out_kev.ident, (uintptr_t) shmem);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, main);

	return (atf_no_error());
}
