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

#ifndef SRS_APP_SECURITY_HPP
#define SRS_APP_SECURITY_HPP

#include <srs_core.hpp>

#include <string>

#include <srs_rtmp_stack.hpp>

class SrsConfDirective;

// The security apply on vhost.
// @see https://github.com/ossrs/srs/issues/211
class SrsSecurity
{
public:
    SrsSecurity();
    virtual ~SrsSecurity();
public:
    // Security check the client apply by vhost security strategy
    // @param type the client type, publish or play.
    // @param ip the ip address of client.
    // @param req the request object of client.
    virtual srs_error_t check(SrsRtmpConnType type, std::string ip, SrsRequest* req);
private:
    virtual srs_error_t do_check(SrsConfDirective* rules, SrsRtmpConnType type, std::string ip, SrsRequest* req);
    virtual srs_error_t allow_check(SrsConfDirective* rules, SrsRtmpConnType type, std::string ip);
    virtual srs_error_t deny_check(SrsConfDirective* rules, SrsRtmpConnType type, std::string ip);
};

#endif

