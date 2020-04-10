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

#ifndef SRS_APP_HTTP_HOOKS_HPP
#define SRS_APP_HTTP_HOOKS_HPP

#include <srs_core.hpp>

#include <string>

class SrsHttpUri;
class SrsStSocket;
class SrsRequest;
class SrsHttpParser;
class SrsHttpClient;

// the http hooks, http callback api,
// for some event, such as on_connect, call
// a http api(hooks).
// TODO: Refine to global variable.
class SrsHttpHooks
{
private:
    SrsHttpHooks();
public:
    virtual ~SrsHttpHooks();
public:
    // The on_connect hook, when client connect to srs.
    // @param url the api server url, to valid the client.
    //         ignore if empty.
    static srs_error_t on_connect(std::string url, SrsRequest* req);
    // The on_close hook, when client disconnect to srs, where client is valid by on_connect.
    // @param url the api server url, to process the event.
    //         ignore if empty.
    static void on_close(std::string url, SrsRequest* req, int64_t send_bytes, int64_t recv_bytes);
    // The on_publish hook, when client(encoder) start to publish stream
    // @param url the api server url, to valid the client.
    //         ignore if empty.
    static srs_error_t on_publish(std::string url, SrsRequest* req);
    // The on_unpublish hook, when client(encoder) stop publish stream.
    // @param url the api server url, to process the event.
    //         ignore if empty.
    static void on_unpublish(std::string url, SrsRequest* req);
    // The on_play hook, when client start to play stream.
    // @param url the api server url, to valid the client.
    //         ignore if empty.
    static srs_error_t on_play(std::string url, SrsRequest* req);
    // The on_stop hook, when client stop to play the stream.
    // @param url the api server url, to process the event.
    //         ignore if empty.
    static void on_stop(std::string url, SrsRequest* req);
    // The on_dvr hook, when reap a dvr file.
    // @param url the api server url, to process the event.
    //         ignore if empty.
    // @param file the file path, can be relative or absolute path.
    // @param cid the source connection cid, for the on_dvr is async call.
    static srs_error_t on_dvr(int cid, std::string url, SrsRequest* req, std::string file);
    // When hls reap segment, callback.
    // @param url the api server url, to process the event.
    //         ignore if empty.
    // @param file the ts file path, can be relative or absolute path.
    // @param ts_url the ts url, which used for m3u8.
    // @param m3u8 the m3u8 file path, can be relative or absolute path.
    // @param m3u8_url the m3u8 url, which is used for the http mount path.
    // @param sn the seq_no, the sequence number of ts in hls/m3u8.
    // @param duration the segment duration in srs_utime_t.
    // @param cid the source connection cid, for the on_dvr is async call.
    static srs_error_t on_hls(int cid, std::string url, SrsRequest* req, std::string file, std::string ts_url,
        std::string m3u8, std::string m3u8_url, int sn, srs_utime_t duration);
    // When hls reap segment, callback.
    // @param url the api server url, to process the event.
    //         ignore if empty.
    // @param ts_url the ts uri, used to replace the variable [ts_url] in url.
    // @param nb_notify the max bytes to read from notify server.
    // @param cid the source connection cid, for the on_dvr is async call.
    static srs_error_t on_hls_notify(int cid, std::string url, SrsRequest* req, std::string ts_url, int nb_notify);
    // Discover co-workers for origin cluster.
    static srs_error_t discover_co_workers(std::string url, std::string& host, int& port);
private:
    static srs_error_t do_post(SrsHttpClient* hc, std::string url, std::string req, int& code, std::string& res);
};

#endif

