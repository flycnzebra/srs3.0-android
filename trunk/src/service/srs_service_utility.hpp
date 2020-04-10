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

#ifndef SRS_SERVICE_UTILITY_HPP
#define SRS_SERVICE_UTILITY_HPP

#include <srs_core.hpp>

#include <arpa/inet.h>
#include <string>
#include <vector>

#include <srs_service_st.hpp>

class ISrsHttpMessage;

// Whether the url is starts with http:// or https://
extern bool srs_string_is_http(std::string url);
extern bool srs_string_is_rtmp(std::string url);

// Whether string is digit number
//      is_digit("0")  === true
//      is_digit("0000000000")  === true
//      is_digit("1234567890")  === true
//      is_digit("0123456789")  === true
//      is_digit("1234567890a") === false
//      is_digit("a1234567890") === false
//      is_digit("10e3") === false
//      is_digit("!1234567890") === false
//      is_digit("") === false
extern bool srs_is_digit_number(std::string str);

// Get local ip, fill to @param ips
extern std::vector<std::string>& srs_get_local_ips();

// Get local public ip, empty string if no public internet address found.
extern std::string srs_get_public_internet_address();

// Detect whether specified device is internet public address.
extern bool srs_net_device_is_internet(std::string ifname);
extern bool srs_net_device_is_internet(const sockaddr* addr);

// Get the original ip from query and header by proxy.
extern std::string srs_get_original_ip(ISrsHttpMessage* r);

#endif

