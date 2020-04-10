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

#ifndef SRS_APP_ASYNC_CALL_HPP
#define SRS_APP_ASYNC_CALL_HPP

#include <srs_core.hpp>

#include <string>
#include <vector>

#include <srs_app_thread.hpp>

// The async call for http hooks, for the http hooks will switch st-thread,
// so we must use isolate thread to avoid the thread corrupt,
// for example, when dvr call http hooks, the video receive thread got
// a video and pass it to the dvr again.
// Futhurmore, the aync call never block the main worker thread.
class ISrsAsyncCallTask
{
public:
    ISrsAsyncCallTask();
    virtual ~ISrsAsyncCallTask();
public:
    // Execute the task async.
    // This method is the actual execute method of task,
    // for example, to notify callback server.
    virtual srs_error_t call() = 0;
    // Convert task to string to describe it.
    // It's used for logger.
    virtual std::string to_string() = 0;
};

// The async callback for dvr, callback and other async worker.
// When worker call with the task, the worker will do it in isolate thread.
// That is, the task is execute/call in async mode.
class SrsAsyncCallWorker : public ISrsCoroutineHandler
{
private:
    SrsCoroutine* trd;
protected:
    std::vector<ISrsAsyncCallTask*> tasks;
    srs_cond_t wait;
    srs_mutex_t lock;
public:
    SrsAsyncCallWorker();
    virtual ~SrsAsyncCallWorker();
public:
    virtual srs_error_t execute(ISrsAsyncCallTask* t);
    virtual int count();
public:
    virtual srs_error_t start();
    virtual void stop();
// Interface ISrsReusableThreadHandler
public:
    virtual srs_error_t cycle();
private:
    virtual void flush_tasks();
};

#endif

