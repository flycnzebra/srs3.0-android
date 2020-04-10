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

#ifndef SRS_APP_HTTP_API_HPP
#define SRS_APP_HTTP_API_HPP

#include <srs_core.hpp>

class SrsStSocket;
class ISrsHttpMessage;
class SrsHttpParser;
class SrsHttpHandler;
class SrsServer;

#include <srs_app_st.hpp>
#include <srs_app_conn.hpp>
#include <srs_http_stack.hpp>
#include <srs_app_reload.hpp>

// For http root.
class SrsGoApiRoot : public ISrsHttpHandler
{
public:
    SrsGoApiRoot();
    virtual ~SrsGoApiRoot();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiApi : public ISrsHttpHandler
{
public:
    SrsGoApiApi();
    virtual ~SrsGoApiApi();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiV1 : public ISrsHttpHandler
{
public:
    SrsGoApiV1();
    virtual ~SrsGoApiV1();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiVersion : public ISrsHttpHandler
{
public:
    SrsGoApiVersion();
    virtual ~SrsGoApiVersion();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiSummaries : public ISrsHttpHandler
{
public:
    SrsGoApiSummaries();
    virtual ~SrsGoApiSummaries();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiRusages : public ISrsHttpHandler
{
public:
    SrsGoApiRusages();
    virtual ~SrsGoApiRusages();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiSelfProcStats : public ISrsHttpHandler
{
public:
    SrsGoApiSelfProcStats();
    virtual ~SrsGoApiSelfProcStats();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiSystemProcStats : public ISrsHttpHandler
{
public:
    SrsGoApiSystemProcStats();
    virtual ~SrsGoApiSystemProcStats();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiMemInfos : public ISrsHttpHandler
{
public:
    SrsGoApiMemInfos();
    virtual ~SrsGoApiMemInfos();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiAuthors : public ISrsHttpHandler
{
public:
    SrsGoApiAuthors();
    virtual ~SrsGoApiAuthors();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiFeatures : public ISrsHttpHandler
{
public:
    SrsGoApiFeatures();
    virtual ~SrsGoApiFeatures();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiRequests : public ISrsHttpHandler
{
public:
    SrsGoApiRequests();
    virtual ~SrsGoApiRequests();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiVhosts : public ISrsHttpHandler
{
public:
    SrsGoApiVhosts();
    virtual ~SrsGoApiVhosts();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiStreams : public ISrsHttpHandler
{
public:
    SrsGoApiStreams();
    virtual ~SrsGoApiStreams();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiClients : public ISrsHttpHandler
{
public:
    SrsGoApiClients();
    virtual ~SrsGoApiClients();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiRaw : virtual public ISrsHttpHandler, virtual public ISrsReloadHandler
{
private:
    SrsServer* server;
private:
    bool raw_api;
    bool allow_reload;
    bool allow_query;
    bool allow_update;
public:
    SrsGoApiRaw(SrsServer* svr);
    virtual ~SrsGoApiRaw();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
// Interface ISrsReloadHandler
public:
    virtual srs_error_t on_reload_http_api_raw_api();
};

class SrsGoApiClusters : public ISrsHttpHandler
{
public:
    SrsGoApiClusters();
    virtual ~SrsGoApiClusters();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsGoApiError : public ISrsHttpHandler
{
public:
    SrsGoApiError();
    virtual ~SrsGoApiError();
public:
    virtual srs_error_t serve_http(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
};

class SrsHttpApi : virtual public SrsConnection, virtual public ISrsReloadHandler
{
private:
    SrsHttpParser* parser;
    SrsHttpCorsMux* cors;
    SrsHttpServeMux* mux;
public:
    SrsHttpApi(IConnectionManager* cm, srs_netfd_t fd, SrsHttpServeMux* m, std::string cip);
    virtual ~SrsHttpApi();
// Interface ISrsKbpsDelta
public:
    virtual void remark(int64_t* in, int64_t* out);
protected:
    virtual srs_error_t do_cycle();
private:
    virtual srs_error_t process_request(ISrsHttpResponseWriter* w, ISrsHttpMessage* r);
// Interface ISrsReloadHandler
public:
    virtual srs_error_t on_reload_http_api_crossdomain();
};

#endif

