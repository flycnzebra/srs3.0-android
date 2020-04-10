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

#include <srs_core.hpp>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <sstream>
using namespace std;

#ifdef SRS_AUTO_GPERF_MP
#include <gperftools/heap-profiler.h>
#endif
#ifdef SRS_AUTO_GPERF_CP
#include <gperftools/profiler.h>
#endif

#include <unistd.h>
using namespace std;

#include <srs_kernel_error.hpp>
#include <srs_app_server.hpp>
#include <srs_app_config.hpp>
#include <srs_app_log.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_core_performance.hpp>
#include <srs_app_utility.hpp>
#include <srs_core_autofree.hpp>
#include <srs_kernel_file.hpp>

// pre-declare
srs_error_t run(SrsServer* svr);
srs_error_t run_master(SrsServer* svr);
void show_macro_features();
string srs_getenv(const char* name);

// @global log and context.
ISrsLog* _srs_log = new SrsFastLog();
ISrsThreadContext* _srs_context = new SrsThreadContext();
// @global config object for app module.
SrsConfig* _srs_config = new SrsConfig();

// @global version of srs, which can grep keyword "XCORE"
extern const char* _srs_version;

/**
 * main entrance.
 */
srs_error_t do_main(int argc, char** argv)
{
    srs_error_t err = srs_success;
    
    // TODO: support both little and big endian.
    srs_assert(srs_is_little_endian());
    
    // for gperf gmp or gcp,
    // should never enable it when not enabled for performance issue.
#ifdef SRS_AUTO_GPERF_MP
    HeapProfilerStart("gperf.srs.gmp");
#endif
#ifdef SRS_AUTO_GPERF_CP
    ProfilerStart("gperf.srs.gcp");
#endif
    
    // directly compile error when these two macro defines.
#if defined(SRS_AUTO_GPERF_MC) && defined(SRS_AUTO_GPERF_MP)
#error ("option --with-gmc confict with --with-gmp, "
    "@see: http://google-perftools.googlecode.com/svn/trunk/doc/heap_checker.html\n"
    "Note that since the heap-checker uses the heap-profiling framework internally, "
    "it is not possible to run both the heap-checker and heap profiler at the same time");
#endif
    
    // never use gmp to check memory leak.
#ifdef SRS_AUTO_GPERF_MP
#warning "gmp is not used for memory leak, please use gmc instead."
#endif
    
    // never use srs log(srs_trace, srs_error, etc) before config parse the option,
    // which will load the log config and apply it.
    if ((err = _srs_config->parse_options(argc, argv)) != srs_success) {
        return srs_error_wrap(err, "config parse options");
    }
    
    // change the work dir and set cwd.
    int r0 = 0;
    string cwd = _srs_config->get_work_dir();
    if (!cwd.empty() && cwd != "./" && (r0 = chdir(cwd.c_str())) == -1) {
        return srs_error_new(-1, "chdir to %s, r0=%d", cwd.c_str(), r0);
    }
    if ((err = _srs_config->initialize_cwd()) != srs_success) {
        return srs_error_wrap(err, "config cwd");
    }
    
    // config parsed, initialize log.
    if ((err = _srs_log->initialize()) != srs_success) {
        return srs_error_wrap(err, "log initialize");
    }
    
    // config already applied to log.
    srs_trace("%s, %s", RTMP_SIG_SRS_SERVER, RTMP_SIG_SRS_LICENSE);
    srs_trace("contributors: %s", SRS_AUTO_CONSTRIBUTORS);
    srs_trace("cwd=%s, work_dir=%s, build: %s, configure: %s, uname: %s, osx: %d",
        _srs_config->cwd().c_str(), cwd.c_str(), SRS_AUTO_BUILD_DATE, SRS_AUTO_USER_CONFIGURE, SRS_AUTO_UNAME, SRS_AUTO_OSX_BOOL);
    srs_trace("configure detail: " SRS_AUTO_CONFIGURE);
#ifdef SRS_AUTO_EMBEDED_TOOL_CHAIN
    srs_trace("crossbuild tool chain: " SRS_AUTO_EMBEDED_TOOL_CHAIN);
#endif

    // for memory check or detect.
    if (true) {
        stringstream ss;
        
#ifdef SRS_PERF_GLIBC_MEMORY_CHECK
        // ensure glibc write error to stderr.
        string lfsov = srs_getenv("LIBC_FATAL_STDERR_");
        setenv("LIBC_FATAL_STDERR_", "1", 1);
        string lfsnv = srs_getenv("LIBC_FATAL_STDERR_");
        //
        // ensure glibc to do alloc check.
        string mcov = srs_getenv("MALLOC_CHECK_");
        setenv("MALLOC_CHECK_", "1", 1);
        string mcnv = srs_getenv("MALLOC_CHECK_");
        ss << "glic mem-check env MALLOC_CHECK_ " << mcov << "=>" << mcnv << ", LIBC_FATAL_STDERR_ " << lfsov << "=>" << lfsnv << ".";
#endif
        
#ifdef SRS_AUTO_GPERF_MC
        string hcov = srs_getenv("HEAPCHECK");
        if (hcov.empty()) {
            string cpath = _srs_config->config();
            srs_warn("gmc HEAPCHECK is required, for example: env HEAPCHECK=normal ./objs/srs -c %s", cpath.c_str());
        } else {
            ss << "gmc env HEAPCHECK=" << hcov << ".";
        }
#endif
        
#ifdef SRS_AUTO_GPERF_MD
        char* TCMALLOC_PAGE_FENCE = getenv("TCMALLOC_PAGE_FENCE");
        if (!TCMALLOC_PAGE_FENCE || strcmp(TCMALLOC_PAGE_FENCE, "1")) {
            srs_warn("gmd enabled without env TCMALLOC_PAGE_FENCE=1");
        } else {
            ss << "gmd env TCMALLOC_PAGE_FENCE=" << TCMALLOC_PAGE_FENCE << ".";
        }
#endif
        
        string sss = ss.str();
        if (!sss.empty()) {
            srs_trace(sss.c_str());
        }
    }
    
    // we check the config when the log initialized.
    if ((err = _srs_config->check_config()) != srs_success) {
        return srs_error_wrap(err, "check config");
    }
    
    // features
    show_macro_features();
    
    SrsServer* svr = new SrsServer();
    SrsAutoFree(SrsServer, svr);
    
    if ((err = run(svr)) != srs_success) {
        return srs_error_wrap(err, "run");
    }
    
    return err;
}

int main(int argc, char** argv) {
    srs_error_t err = do_main(argc, argv);
    
    if (err != srs_success) {
        srs_error("Failed, %s", srs_error_desc(err).c_str());
    }
    
    int ret = srs_error_code(err);
    srs_freep(err);
    return ret;
}

/**
 * show the features by macro, the actual macro values.
 */
void show_macro_features()
{
    if (true) {
        stringstream ss;
        
        ss << "features";
        
        // rch(rtmp complex handshake)
        ss << ", rch:" << srs_bool2switch(true);
        ss << ", dash:" << "on";
        ss << ", hls:" << srs_bool2switch(true);
        ss << ", hds:" << srs_bool2switch(SRS_AUTO_HDS_BOOL);
        // hc(http callback)
        ss << ", hc:" << srs_bool2switch(true);
        // ha(http api)
        ss << ", ha:" << srs_bool2switch(true);
        // hs(http server)
        ss << ", hs:" << srs_bool2switch(true);
        // hp(http parser)
        ss << ", hp:" << srs_bool2switch(true);
        ss << ", dvr:" << srs_bool2switch(true);
        // trans(transcode)
        ss << ", trans:" << srs_bool2switch(true);
        // inge(ingest)
        ss << ", inge:" << srs_bool2switch(true);
        ss << ", stat:" << srs_bool2switch(true);
        // sc(stream-caster)
        ss << ", sc:" << srs_bool2switch(true);
        srs_trace(ss.str().c_str());
    }
    
    if (true) {
        stringstream ss;
        ss << "SRS on ";
#if defined(__amd64__)
        ss << " amd64";
#endif
#if defined(__x86_64__)
        ss << " x86_64";
#endif
#if defined(__i386__)
        ss << " i386";
#endif
#if defined(__arm__)
        ss << "arm";
#endif
#if defined(__aarch64__)
        ss << " aarch64";
#endif
#if defined(SRS_AUTO_CROSSBUILD)
        ss << "(crossbuild)";
#endif
        
        ss << ", conf:" << _srs_config->config() << ", limit:" << _srs_config->get_max_connections()
        << ", writev:" << sysconf(_SC_IOV_MAX) << ", encoding:" << (srs_is_little_endian()? "little-endian":"big-endian")
        << ", HZ:" << (int)sysconf(_SC_CLK_TCK);
        
        srs_trace(ss.str().c_str());
    }
    
    if (true) {
        stringstream ss;
        
        // mw(merged-write)
        ss << "mw sleep:" << srsu2msi(SRS_PERF_MW_SLEEP) << "ms";
        
        // mr(merged-read)
        ss << ". mr ";
#ifdef SRS_PERF_MERGED_READ
        ss << "enabled:on";
#else
        ss << "enabled:off";
#endif
        ss << ", default:" << SRS_PERF_MR_ENABLED << ", sleep:" << srsu2msi(SRS_PERF_MR_SLEEP) << "ms";
        
        srs_trace(ss.str().c_str());
    }
    
    if (true) {
        stringstream ss;
        
        // gc(gop-cache)
        ss << "gc:" << srs_bool2switch(SRS_PERF_GOP_CACHE);
        // pq(play-queue)
        ss << ", pq:" << srsu2msi(SRS_PERF_PLAY_QUEUE) << "ms";
        // cscc(chunk stream cache cid)
        ss << ", cscc:[0," << SRS_PERF_CHUNK_STREAM_CACHE << ")";
        // csa(complex send algorithm)
        ss << ", csa:";
#ifndef SRS_PERF_COMPLEX_SEND
        ss << "off";
#else
        ss << "on";
#endif
        
        // tn(TCP_NODELAY)
        ss << ", tn:";
#ifdef SRS_PERF_TCP_NODELAY
        ss << "on(may hurts performance)";
#else
        ss << "off";
#endif
        
        // ss(SO_SENDBUF)
        ss << ", ss:";
#ifdef SRS_PERF_SO_SNDBUF_SIZE
        ss << SRS_PERF_SO_SNDBUF_SIZE;
#else
        ss << "auto(guess by merged write)";
#endif
        
        srs_trace(ss.str().c_str());
    }
    
    // others
    int possible_mr_latency = 0;
#ifdef SRS_PERF_MERGED_READ
    possible_mr_latency = srsu2msi(SRS_PERF_MR_SLEEP);
#endif
    srs_trace("system default latency(ms): mw(0-%d) + mr(0-%d) + play-queue(0-%d)",
              srsu2msi(SRS_PERF_MW_SLEEP), possible_mr_latency, srsu2msi(SRS_PERF_PLAY_QUEUE));
    
#ifdef SRS_AUTO_MEM_WATCH
#warning "srs memory watcher will hurts performance. user should kill by SIGTERM or init.d script."
    srs_warn("srs memory watcher will hurts performance. user should kill by SIGTERM or init.d script.");
#endif
    
#if VERSION_MAJOR > VERSION_STABLE
    #warning "Current branch is beta."
    srs_warn("%s/%s is beta", RTMP_SIG_SRS_KEY, RTMP_SIG_SRS_VERSION);
#endif
    
#if defined(SRS_PERF_SO_SNDBUF_SIZE) && !defined(SRS_PERF_MW_SO_SNDBUF)
#error "SRS_PERF_SO_SNDBUF_SIZE depends on SRS_PERF_MW_SO_SNDBUF"
#endif
}

string srs_getenv(const char* name)
{
    char* cv = ::getenv(name);
    
    if (cv) {
        return cv;
    }
    
    return "";
}

// Detect docker by https://stackoverflow.com/a/41559867
bool _srs_in_docker = false;
srs_error_t srs_detect_docker()
{
    srs_error_t err = srs_success;

    _srs_in_docker = false;

    SrsFileReader fr;
    if ((err = fr.open("/proc/1/cgroup")) != srs_success) {
        return err;
    }

    ssize_t nn;
    char buf[1024];
    if ((err = fr.read(buf, sizeof(buf), &nn)) != srs_success) {
        return err;
    }

    if (nn <= 0) {
        return err;
    }

    string s(buf, nn);
    if (srs_string_contains(s, "/docker")) {
        _srs_in_docker = true;
    }

    return err;
}

srs_error_t run(SrsServer* svr)
{
    srs_error_t err = srs_success;

    // Ignore any error while detecting docker.
    if ((err = srs_detect_docker()) != srs_success) {
        srs_error_reset(err);
    }

    // Initialize the whole system, set hooks to handle server level events.
    if ((err = svr->initialize(NULL)) != srs_success) {
        return srs_error_wrap(err, "server initialize");
    }

    // Load daemon from config, disable it for docker.
    // @see https://github.com/ossrs/srs/issues/1594
    bool in_daemon = _srs_config->get_daemon();
    if (in_daemon && _srs_in_docker && _srs_config->disable_daemon_for_docker()) {
        srs_warn("disable daemon for docker");
        in_daemon = false;
    }

    // If not daemon, directly run master.
    if (!in_daemon) {
        if ((err = run_master(svr)) != srs_success) {
            return srs_error_wrap(err, "run master");
        }
        return srs_success;
    }
    
    srs_trace("start daemon mode...");
    
    int pid = fork();
    
    if(pid < 0) {
        return srs_error_new(-1, "fork father process");
    }
    
    // grandpa
    if(pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
        srs_trace("grandpa process exit.");
        exit(0);
    }
    
    // father
    pid = fork();
    
    if(pid < 0) {
        return srs_error_new(-1, "fork child process");
    }
    
    if(pid > 0) {
        srs_trace("father process exit");
        exit(0);
    }
    
    // son
    srs_trace("son(daemon) process running.");
    
    if ((err = run_master(svr)) != srs_success) {
        return srs_error_wrap(err, "daemon run master");
    }
    
    return err;
}

srs_error_t run_master(SrsServer* svr)
{
    srs_error_t err = srs_success;
    
    if ((err = svr->initialize_st()) != srs_success) {
        return srs_error_wrap(err, "initialize st");
    }
    
    if ((err = svr->initialize_signal()) != srs_success) {
        return srs_error_wrap(err, "initialize signal");
    }
    
    if ((err = svr->acquire_pid_file()) != srs_success) {
        return srs_error_wrap(err, "acquire pid file");
    }
    
    if ((err = svr->listen()) != srs_success) {
        return srs_error_wrap(err, "listen");
    }
    
    if ((err = svr->register_signal()) != srs_success) {
        return srs_error_wrap(err, "register signal");
    }
    
    if ((err = svr->http_handle()) != srs_success) {
        return srs_error_wrap(err, "http handle");
    }
    
    if ((err = svr->ingest()) != srs_success) {
        return srs_error_wrap(err, "ingest");
    }
    
    if ((err = svr->cycle()) != srs_success) {
        return srs_error_wrap(err, "main cycle");
    }
    
    return err;
}

