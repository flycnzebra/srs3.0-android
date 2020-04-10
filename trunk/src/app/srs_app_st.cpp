/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2020 Winlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <srs_app_st.hpp>

#include <st.h>
#include <string>
using namespace std;

#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_utility.hpp>
#include <srs_app_log.hpp>

ISrsCoroutineHandler::ISrsCoroutineHandler()
{
}

ISrsCoroutineHandler::~ISrsCoroutineHandler()
{
}

SrsCoroutine::SrsCoroutine()
{
}

SrsCoroutine::~SrsCoroutine()
{
}

SrsDummyCoroutine::SrsDummyCoroutine()
{
}

SrsDummyCoroutine::~SrsDummyCoroutine()
{
}

srs_error_t SrsDummyCoroutine::start()
{
    return srs_error_new(ERROR_THREAD_DUMMY, "dummy coroutine");
}

void SrsDummyCoroutine::stop()
{
}

void SrsDummyCoroutine::interrupt()
{
}

srs_error_t SrsDummyCoroutine::pull()
{
    return srs_error_new(ERROR_THREAD_DUMMY, "dummy pull");
}

int SrsDummyCoroutine::cid()
{
    return 0;
}

_ST_THREAD_CREATE_PFN _pfn_st_thread_create = (_ST_THREAD_CREATE_PFN)st_thread_create;

SrsSTCoroutine::SrsSTCoroutine(string n, ISrsCoroutineHandler* h, int cid)
{
    name = n;
    handler = h;
    context = cid;
    trd = NULL;
    trd_err = srs_success;
    started = interrupted = disposed = cycle_done = false;
}

SrsSTCoroutine::~SrsSTCoroutine()
{
    stop();
    
    srs_freep(trd_err);
}

srs_error_t SrsSTCoroutine::start()
{
    srs_error_t err = srs_success;
    
    if (started || disposed) {
        if (disposed) {
            err = srs_error_new(ERROR_THREAD_DISPOSED, "disposed");
        } else {
            err = srs_error_new(ERROR_THREAD_STARTED, "started");
        }

        if (trd_err == srs_success) {
            trd_err = srs_error_copy(err);
        }
        
        return err;
    }
    
    if ((trd = (srs_thread_t)_pfn_st_thread_create(pfn, this, 1, 0)) == NULL) {
        err = srs_error_new(ERROR_ST_CREATE_CYCLE_THREAD, "create failed");
        
        srs_freep(trd_err);
        trd_err = srs_error_copy(err);
        
        return err;
    }
    
    started = true;

    return err;
}

void SrsSTCoroutine::stop()
{
    if (disposed) {
        return;
    }
    disposed = true;
    
    interrupt();

    // When not started, the rd is NULL.
    if (trd) {
        void* res = NULL;
        int r0 = st_thread_join((st_thread_t)trd, &res);
        srs_assert(!r0);

        srs_error_t err_res = (srs_error_t)res;
        if (err_res != srs_success) {
            // When worker cycle done, the error has already been overrided,
            // so the trd_err should be equal to err_res.
            srs_assert(trd_err == err_res);
        }
    }
    
    // If there's no error occur from worker, try to set to terminated error.
    if (trd_err == srs_success && !cycle_done) {
        trd_err = srs_error_new(ERROR_THREAD_TERMINATED, "terminated");
    }
    
    return;
}

void SrsSTCoroutine::interrupt()
{
    if (!started || interrupted || cycle_done) {
        return;
    }
    interrupted = true;
    
    if (trd_err == srs_success) {
        trd_err = srs_error_new(ERROR_THREAD_INTERRUPED, "interrupted");
    }
    
    st_thread_interrupt((st_thread_t)trd);
}

srs_error_t SrsSTCoroutine::pull()
{
    return srs_error_copy(trd_err);
}

int SrsSTCoroutine::cid()
{
    return context;
}

srs_error_t SrsSTCoroutine::cycle()
{
    if (_srs_context) {
        if (context) {
            _srs_context->set_id(context);
        } else {
            context = _srs_context->generate_id();
        }
    }
    
    srs_error_t err = handler->cycle();
    if (err != srs_success) {
        return srs_error_wrap(err, "coroutine cycle");
    }

    // Set cycle done, no need to interrupt it.
    cycle_done = true;
    
    return err;
}

void* SrsSTCoroutine::pfn(void* arg)
{
    SrsSTCoroutine* p = (SrsSTCoroutine*)arg;

    srs_error_t err = p->cycle();

    // Set the err for function pull to fetch it.
    // @see https://github.com/ossrs/srs/pull/1304#issuecomment-480484151
    if (err != srs_success) {
        srs_freep(p->trd_err);
        // It's ok to directly use it, because it's returned by st_thread_join.
        p->trd_err = err;
    }

    return (void*)err;
}

