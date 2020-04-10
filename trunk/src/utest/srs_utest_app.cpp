/*
The MIT License (MIT)

Copyright (c) 2013-2020 Winlin

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <srs_utest_app.hpp>

using namespace std;

#include <srs_kernel_error.hpp>
#include <srs_app_fragment.hpp>
#include <srs_app_security.hpp>
#include <srs_app_config.hpp>

#include <srs_app_st.hpp>

VOID TEST(AppCoroutineTest, Dummy)
{
    SrsDummyCoroutine dc;

    if (true) {
        EXPECT_EQ(0, dc.cid());

        srs_error_t err = dc.pull();
        EXPECT_TRUE(err != srs_success);
        EXPECT_TRUE(ERROR_THREAD_DUMMY == srs_error_code(err));
        srs_freep(err);

        err = dc.start();
        EXPECT_TRUE(err != srs_success);
        EXPECT_TRUE(ERROR_THREAD_DUMMY == srs_error_code(err));
        srs_freep(err);
    }

    if (true) {
        dc.stop();

        EXPECT_EQ(0, dc.cid());

        srs_error_t err = dc.pull();
        EXPECT_TRUE(err != srs_success);
        EXPECT_TRUE(ERROR_THREAD_DUMMY == srs_error_code(err));
        srs_freep(err);

        err = dc.start();
        EXPECT_TRUE(err != srs_success);
        EXPECT_TRUE(ERROR_THREAD_DUMMY == srs_error_code(err));
        srs_freep(err);
    }

    if (true) {
        dc.interrupt();

        EXPECT_EQ(0, dc.cid());

        srs_error_t err = dc.pull();
        EXPECT_TRUE(err != srs_success);
        EXPECT_TRUE(ERROR_THREAD_DUMMY == srs_error_code(err));
        srs_freep(err);

        err = dc.start();
        EXPECT_TRUE(err != srs_success);
        EXPECT_TRUE(ERROR_THREAD_DUMMY == srs_error_code(err));
        srs_freep(err);
    }
}

class MockCoroutineHandler : public ISrsCoroutineHandler {
public:
    SrsSTCoroutine* trd;
    srs_error_t err;
    srs_cond_t running;
    srs_cond_t exited;
    int cid;
    // Quit without error.
    bool quit;
public:
    MockCoroutineHandler() : trd(NULL), err(srs_success), cid(0), quit(false) {
        running = srs_cond_new();
        exited = srs_cond_new();
    }
    virtual ~MockCoroutineHandler() {
        srs_cond_destroy(running);
        srs_cond_destroy(exited);
    }
public:
    virtual srs_error_t cycle() {
        srs_error_t r0 = srs_success;

        srs_cond_signal(running);
        cid = _srs_context->get_id();

        while (!quit && (r0 = trd->pull()) == srs_success && err == srs_success) {
            srs_usleep(10 * SRS_UTIME_MILLISECONDS);
        }

        srs_cond_signal(exited);

        if (err != srs_success) {
            srs_freep(r0);
            return err;
        }

        return r0;
    }
};

VOID TEST(AppCoroutineTest, StartStop)
{
    if (true) {
        MockCoroutineHandler ch;
        SrsSTCoroutine sc("test", &ch);
        ch.trd = &sc;
        EXPECT_EQ(0, sc.cid());

        // Thread stop after created.
        sc.stop();

        EXPECT_EQ(0, sc.cid());

        srs_error_t err = sc.pull();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(ERROR_THREAD_TERMINATED == srs_error_code(err));
        srs_freep(err);

        // Should never reuse a disposed thread.
        err = sc.start();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(ERROR_THREAD_DISPOSED == srs_error_code(err));
        srs_freep(err);
    }

    if (true) {
        MockCoroutineHandler ch;
        SrsSTCoroutine sc("test", &ch);
        ch.trd = &sc;
        EXPECT_EQ(0, sc.cid());

        EXPECT_TRUE(srs_success == sc.start());
        EXPECT_TRUE(srs_success == sc.pull());

        srs_cond_timedwait(ch.running, 100 * SRS_UTIME_MILLISECONDS);
        EXPECT_TRUE(sc.cid() > 0);

        // Thread stop after started.
        sc.stop();

        srs_error_t err = sc.pull();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(ERROR_THREAD_INTERRUPED == srs_error_code(err));
        srs_freep(err);

        // Should never reuse a disposed thread.
        err = sc.start();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(ERROR_THREAD_DISPOSED == srs_error_code(err));
        srs_freep(err);
    }

    if (true) {
        MockCoroutineHandler ch;
        SrsSTCoroutine sc("test", &ch);
        ch.trd = &sc;
        EXPECT_EQ(0, sc.cid());

        EXPECT_TRUE(srs_success == sc.start());
        EXPECT_TRUE(srs_success == sc.pull());

        // Error when start multiple times.
        srs_error_t err = sc.start();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(ERROR_THREAD_STARTED == srs_error_code(err));
        srs_freep(err);

        err = sc.pull();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(ERROR_THREAD_STARTED == srs_error_code(err));
        srs_freep(err);
    }
}

VOID TEST(AppCoroutineTest, Cycle)
{
    if (true) {
        MockCoroutineHandler ch;
        SrsSTCoroutine sc("test", &ch);
        ch.trd = &sc;

        EXPECT_TRUE(srs_success == sc.start());
        EXPECT_TRUE(srs_success == sc.pull());

        // Set cycle to error.
        ch.err = srs_error_new(-1, "cycle");

        srs_cond_timedwait(ch.running, 100 * SRS_UTIME_MILLISECONDS);

        // The cycle error should be pulled.
        srs_error_t err = sc.pull();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(-1 == srs_error_code(err));
        srs_freep(err);
    }

    if (true) {
        MockCoroutineHandler ch;
        SrsSTCoroutine sc("test", &ch, 250);
        ch.trd = &sc;
        EXPECT_EQ(250, sc.cid());

        EXPECT_TRUE(srs_success == sc.start());
        EXPECT_TRUE(srs_success == sc.pull());

        // After running, the cid in cycle should equal to the thread.
        srs_cond_timedwait(ch.running, 100 * SRS_UTIME_MILLISECONDS);
        EXPECT_EQ(250, ch.cid);
    }

    if (true) {
        MockCoroutineHandler ch;
        SrsSTCoroutine sc("test", &ch);
        ch.trd = &sc;

        EXPECT_TRUE(srs_success == sc.start());
        EXPECT_TRUE(srs_success == sc.pull());

        srs_cond_timedwait(ch.running, 100 * SRS_UTIME_MILLISECONDS);

        // Interrupt thread, set err to interrupted.
        sc.interrupt();

        // Set cycle to error.
        ch.err = srs_error_new(-1, "cycle");

        // When thread terminated, thread will get its error.
        srs_cond_timedwait(ch.exited, 100 * SRS_UTIME_MILLISECONDS);

        // Override the error by cycle error.
        sc.stop();

        // Should be cycle error.
        srs_error_t err = sc.pull();
        EXPECT_TRUE(srs_success != err);
        EXPECT_TRUE(-1 == srs_error_code(err));
        srs_freep(err);
    }

    if (true) {
        MockCoroutineHandler ch;
        SrsSTCoroutine sc("test", &ch);
        ch.trd = &sc;

        EXPECT_TRUE(srs_success == sc.start());
        EXPECT_TRUE(srs_success == sc.pull());

        // Quit without error.
        ch.quit = true;

        // Wait for thread to done.
        srs_cond_timedwait(ch.exited, 100 * SRS_UTIME_MILLISECONDS);

        // Override the error by cycle error.
        sc.stop();

        // Should be cycle error.
        srs_error_t err = sc.pull();
        EXPECT_TRUE(srs_success == err);
        srs_freep(err);
    }
}

void* mock_st_thread_create(void *(*/*start*/)(void *arg), void */*arg*/, int /*joinable*/, int /*stack_size*/) {
    return NULL;
}

VOID TEST(AppCoroutineTest, StartThread)
{
    MockCoroutineHandler ch;
    SrsSTCoroutine sc("test", &ch);
    ch.trd = &sc;

    _ST_THREAD_CREATE_PFN ov = _pfn_st_thread_create;
    _pfn_st_thread_create = (_ST_THREAD_CREATE_PFN)mock_st_thread_create;

    srs_error_t err = sc.start();
    _pfn_st_thread_create = ov;

    EXPECT_TRUE(srs_success != err);
    EXPECT_TRUE(ERROR_ST_CREATE_CYCLE_THREAD == srs_error_code(err));
    srs_freep(err);
}

VOID TEST(AppFragmentTest, CheckDuration)
{
	if (true) {
		SrsFragment frg;
		EXPECT_EQ(-1, frg.start_dts);
		EXPECT_EQ(0, frg.dur);
		EXPECT_FALSE(frg.sequence_header);
	}

	if (true) {
		SrsFragment frg;

		frg.append(0);
		EXPECT_EQ(0, frg.duration());

		frg.append(10);
		EXPECT_EQ(10 * SRS_UTIME_MILLISECONDS, frg.duration());

		frg.append(99);
		EXPECT_EQ(99 * SRS_UTIME_MILLISECONDS, frg.duration());

		frg.append(0x7fffffffLL);
		EXPECT_EQ(0x7fffffffLL * SRS_UTIME_MILLISECONDS, frg.duration());

		frg.append(0xffffffffLL);
		EXPECT_EQ(0xffffffffLL * SRS_UTIME_MILLISECONDS, frg.duration());

		frg.append(0x20c49ba5e353f7LL);
		EXPECT_EQ(0x20c49ba5e353f7LL * SRS_UTIME_MILLISECONDS, frg.duration());
	}

	if (true) {
		SrsFragment frg;

		frg.append(0);
		EXPECT_EQ(0, frg.duration());

		frg.append(0x7fffffffffffffffLL);
		EXPECT_EQ(0, frg.duration());
	}

	if (true) {
		SrsFragment frg;

		frg.append(100);
		EXPECT_EQ(0, frg.duration());

		frg.append(10);
		EXPECT_EQ(0, frg.duration());

		frg.append(100);
		EXPECT_EQ(90 * SRS_UTIME_MILLISECONDS, frg.duration());
	}

	if (true) {
		SrsFragment frg;

		frg.append(-10);
		EXPECT_EQ(0, frg.duration());

		frg.append(-5);
		EXPECT_EQ(0, frg.duration());

		frg.append(10);
		EXPECT_EQ(10 * SRS_UTIME_MILLISECONDS, frg.duration());
	}
}

VOID TEST(AppSecurity, CheckSecurity)
{
    srs_error_t err;

    // Deny if no rules.
    if (true) {
        SrsSecurity sec; SrsRequest rr;
        HELPER_EXPECT_FAILED(sec.do_check(NULL, SrsRtmpConnUnknown, "", &rr));
    }

    // Deny if not allowed.
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnUnknown, "", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("others"); rules.get_or_create("any");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnUnknown, "", &rr));
    }

    // Deny by rule.
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "all");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnPlay, "", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "12.13.14.15");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnPlay, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "11.12.13.14");
        if (true) {
            SrsConfDirective* d = new SrsConfDirective();
            d->name = "deny";
            d->args.push_back("play");
            d->args.push_back("12.13.14.15");
            rules.directives.push_back(d);
        }
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnPlay, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "publish", "12.13.14.15");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnFMLEPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "publish", "12.13.14.15");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnFlashPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "publish", "all");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnFlashPublish, "11.12.13.14", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "publish", "12.13.14.15");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnHaivisionPublish, "12.13.14.15", &rr));
    }

    // Allowed if not denied.
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "all");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnFMLEPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnFMLEPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnPlay, "11.12.13.14", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "publish", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnPlay, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "publish", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnUnknown, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "publish", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnFlashPublish, "11.12.13.14", &rr));
    }

    // Allowed by rule.
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "play", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnPlay, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "play", "all");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnPlay, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "publish", "all");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnFMLEPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "publish", "all");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnFlashPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "publish", "all");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnHaivisionPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "publish", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnHaivisionPublish, "12.13.14.15", &rr));
    }

    // Allowed if not denied.
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "12.13.14.15");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnFMLEPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("deny", "play", "all");
        HELPER_EXPECT_SUCCESS(sec.do_check(&rules, SrsRtmpConnFMLEPublish, "12.13.14.15", &rr));
    }

    // Denied if not allowd.
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "play", "11.12.13.14");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnFMLEPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "play", "11.12.13.14");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnPlay, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "publish", "12.13.14.15");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnPlay, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "publish", "11.12.13.14");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnHaivisionPublish, "12.13.14.15", &rr));
    }
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "publish", "11.12.13.14");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnUnknown, "11.12.13.14", &rr));
    }

    // Denied if dup.
    if (true) {
        SrsSecurity sec; SrsRequest rr; SrsConfDirective rules;
        rules.get_or_create("allow", "play", "11.12.13.14");
        rules.get_or_create("deny", "play", "11.12.13.14");
        HELPER_EXPECT_FAILED(sec.do_check(&rules, SrsRtmpConnPlay, "11.12.13.14", &rr));
    }

    // SRS apply the following simple strategies one by one:
    //       1. allow all if security disabled.
    //       2. default to deny all when security enabled.
    //       3. allow if matches allow strategy.
    //       4. deny if matches deny strategy.
}

