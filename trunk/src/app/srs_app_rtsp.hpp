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

#ifndef SRS_APP_RTSP_HPP
#define SRS_APP_RTSP_HPP

#include <srs_core.hpp>

#include <string>
#include <vector>
#include <map>

#include <srs_app_st.hpp>
#include <srs_app_thread.hpp>
#include <srs_app_listener.hpp>

class SrsStSocket;
class SrsRtspConn;
class SrsRtspStack;
class SrsRtspCaster;
class SrsConfDirective;
class SrsRtpPacket;
class SrsRequest;
class SrsStSocket;
class SrsRtmpClient;
class SrsRawH264Stream;
class SrsRawAacStream;
struct SrsRawAacStreamCodec;
class SrsSharedPtrMessage;
class SrsAudioFrame;
class SrsSimpleStream;
class SrsPithyPrint;
class SrsSimpleRtmpClient;

// A rtp connection which transport a stream.
class SrsRtpConn: public ISrsUdpHandler
{
private:
    SrsPithyPrint* pprint;
    SrsUdpListener* listener;
    SrsRtspConn* rtsp;
    SrsRtpPacket* cache;
    int stream_id;
    int _port;
public:
    SrsRtpConn(SrsRtspConn* r, int p, int sid);
    virtual ~SrsRtpConn();
public:
    virtual int port();
    virtual srs_error_t listen();
// Interface ISrsUdpHandler
public:
    virtual srs_error_t on_udp_packet(const sockaddr* from, const int fromlen, char* buf, int nb_buf);
};

// The audio cache, audio is grouped by frames.
struct SrsRtspAudioCache
{
    int64_t dts;
    SrsAudioFrame* audio;
    SrsSimpleStream* payload;
    
    SrsRtspAudioCache();
    virtual ~SrsRtspAudioCache();
};

// The time jitter correct for rtsp.
class SrsRtspJitter
{
private:
    int64_t previous_timestamp;
    int64_t pts;
    int delta;
public:
    SrsRtspJitter();
    virtual ~SrsRtspJitter();
public:
    virtual int64_t timestamp();
    virtual srs_error_t correct(int64_t& ts);
};

// The rtsp connection serve the fd.
class SrsRtspConn : public ISrsCoroutineHandler
{
private:
    std::string output_template;
    std::string rtsp_tcUrl;
    std::string rtsp_stream;
private:
    std::string session;
    // video stream.
    int video_id;
    std::string video_codec;
    SrsRtpConn* video_rtp;
    // audio stream.
    int audio_id;
    std::string audio_codec;
    int audio_sample_rate;
    int audio_channel;
    SrsRtpConn* audio_rtp;
private:
    srs_netfd_t stfd;
    SrsStSocket* skt;
    SrsRtspStack* rtsp;
    SrsRtspCaster* caster;
    SrsCoroutine* trd;
private:
    SrsRequest* req;
    SrsSimpleRtmpClient* sdk;
    SrsRtspJitter* vjitter;
    SrsRtspJitter* ajitter;
private:
    SrsRawH264Stream* avc;
    std::string h264_sps;
    std::string h264_pps;
private:
    SrsRawAacStream* aac;
    SrsRawAacStreamCodec* acodec;
    std::string aac_specific_config;
    SrsRtspAudioCache* acache;
public:
    SrsRtspConn(SrsRtspCaster* c, srs_netfd_t fd, std::string o);
    virtual ~SrsRtspConn();
public:
    virtual srs_error_t serve();
private:
    virtual srs_error_t do_cycle();
// internal methods
public:
    virtual srs_error_t on_rtp_packet(SrsRtpPacket* pkt, int stream_id);
// Interface ISrsOneCycleThreadHandler
public:
    virtual srs_error_t cycle();
private:
    virtual srs_error_t on_rtp_video(SrsRtpPacket* pkt, int64_t dts, int64_t pts);
    virtual srs_error_t on_rtp_audio(SrsRtpPacket* pkt, int64_t dts);
    virtual srs_error_t kickoff_audio_cache(SrsRtpPacket* pkt, int64_t dts);
private:
    virtual srs_error_t write_sequence_header();
    virtual srs_error_t write_h264_sps_pps(uint32_t dts, uint32_t pts);
    virtual srs_error_t write_h264_ipb_frame(char* frame, int frame_size, uint32_t dts, uint32_t pts);
    virtual srs_error_t write_audio_raw_frame(char* frame, int frame_size, SrsRawAacStreamCodec* codec, uint32_t dts);
    virtual srs_error_t rtmp_write_packet(char type, uint32_t timestamp, char* data, int size);
private:
    // Connect to RTMP server.
    virtual srs_error_t connect();
    // Close the connection to RTMP server.
    virtual void close();
};

// The caster for rtsp.
class SrsRtspCaster : public ISrsTcpHandler
{
private:
    std::string output;
    int local_port_min;
    int local_port_max;
    // The key: port, value: whether used.
    std::map<int, bool> used_ports;
private:
    std::vector<SrsRtspConn*> clients;
public:
    SrsRtspCaster(SrsConfDirective* c);
    virtual ~SrsRtspCaster();
public:
    // Alloc a rtp port from local ports pool.
    // @param pport output the rtp port.
    virtual srs_error_t alloc_port(int* pport);
    // Free the alloced rtp port.
    virtual void free_port(int lpmin, int lpmax);
// Interface ISrsTcpHandler
public:
    virtual srs_error_t on_tcp_client(srs_netfd_t stfd);
// internal methods.
public:
    virtual void remove(SrsRtspConn* conn);
};

#endif

