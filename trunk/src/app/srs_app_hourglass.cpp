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

#include <srs_app_hourglass.hpp>

using namespace std;

#include <srs_kernel_error.hpp>
#include <srs_app_st.hpp>
#include <srs_kernel_log.hpp>

ISrsHourGlass::ISrsHourGlass()
{
}

ISrsHourGlass::~ISrsHourGlass()
{
}

SrsHourGlass::SrsHourGlass(ISrsHourGlass* h, srs_utime_t resolution)
{
    handler = h;
    _resolution = resolution;
    total_elapse = 0;
}

SrsHourGlass::~SrsHourGlass()
{
}

srs_error_t SrsHourGlass::tick(int type, srs_utime_t interval)
{
    srs_error_t err = srs_success;
    
    if (_resolution > 0 && (interval % _resolution) != 0) {
        return srs_error_new(ERROR_SYSTEM_HOURGLASS_RESOLUTION,
            "invalid interval=%dms, resolution=%dms", srsu2msi(interval), srsu2msi(_resolution));
    }
    
    ticks[type] = interval;
    
    return err;
}

srs_error_t SrsHourGlass::cycle()
{
    srs_error_t err = srs_success;
    
    map<int, srs_utime_t>::iterator it;
    for (it = ticks.begin(); it != ticks.end(); ++it) {
        int type = it->first;
        srs_utime_t interval = it->second;
        
        if (interval == 0 || (total_elapse % interval) == 0) {
            if ((err = handler->notify(type, interval, total_elapse)) != srs_success) {
                return srs_error_wrap(err, "notify");
            }
        }
    }

    // TODO: FIXME: Maybe we should use wallclock.
    total_elapse += _resolution;
    srs_usleep(_resolution);
    
    return err;
}
