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

#include <srs_app_rtsp.hpp>

#include <algorithm>
using namespace std;

#include <srs_app_config.hpp>
#include <srs_kernel_error.hpp>
#include <srs_rtsp_stack.hpp>
#include <srs_app_st.hpp>
#include <srs_kernel_log.hpp>
#include <srs_app_utility.hpp>
#include <srs_core_autofree.hpp>
#include <srs_kernel_buffer.hpp>
#include <srs_kernel_stream.hpp>
#include <srs_rtmp_stack.hpp>
#include <srs_protocol_amf0.hpp>
#include <srs_protocol_utility.hpp>
#include <srs_kernel_utility.hpp>
#include <srs_raw_avc.hpp>
#include <srs_kernel_codec.hpp>
#include <srs_app_pithy_print.hpp>
#include <srs_app_rtmp_conn.hpp>
#include <srs_protocol_utility.hpp>
#include <srs_protocol_format.hpp>

SrsRtpConn::SrsRtpConn(SrsRtspConn* r, int p, int sid)
{
    rtsp = r;
    _port = p;
    stream_id = sid;
    // TODO: support listen at <[ip:]port>
    listener = new SrsUdpListener(this, srs_any_address_for_listener(), p);
    cache = new SrsRtpPacket();
    pprint = SrsPithyPrint::create_caster();
}

SrsRtpConn::~SrsRtpConn()
{
    srs_freep(listener);
    srs_freep(cache);
    srs_freep(pprint);
}

int SrsRtpConn::port()
{
    return _port;
}

srs_error_t SrsRtpConn::listen()
{
    return listener->listen();
}

srs_error_t SrsRtpConn::on_udp_packet(const sockaddr* from, const int fromlen, char* buf, int nb_buf)
{
    srs_error_t err = srs_success;
    
    pprint->elapse();
    
    if (true) {
        SrsBuffer stream(buf, nb_buf);
        
        SrsRtpPacket pkt;
        if ((err = pkt.decode(&stream)) != srs_success) {
            return srs_error_wrap(err, "decode");
        }
        
        if (pkt.chunked) {
            if (!cache) {
                cache = new SrsRtpPacket();
            }
            cache->copy(&pkt);
            cache->payload->append(pkt.payload->bytes(), pkt.payload->length());
            if (!cache->completed && pprint->can_print()) {
                srs_trace("<- " SRS_CONSTS_LOG_STREAM_CASTER " rtsp: rtp chunked %dB, age=%d, vt=%d/%u, sts=%u/%#x/%#x, paylod=%dB",
                          nb_buf, pprint->age(), cache->version, cache->payload_type, cache->sequence_number, cache->timestamp, cache->ssrc,
                          cache->payload->length()
                          );
                return err;
            }
        } else {
            srs_freep(cache);
            cache = new SrsRtpPacket();
            cache->reap(&pkt);
        }
    }
    
    if (pprint->can_print()) {
        srs_trace("<- " SRS_CONSTS_LOG_STREAM_CASTER " rtsp: rtp #%d %dB, age=%d, vt=%d/%u, sts=%u/%u/%#x, paylod=%dB, chunked=%d",
                  stream_id, nb_buf, pprint->age(), cache->version, cache->payload_type, cache->sequence_number, cache->timestamp, cache->ssrc,
                  cache->payload->length(), cache->chunked
                  );
    }
    
    // always free it.
    SrsAutoFree(SrsRtpPacket, cache);
    
    if ((err = rtsp->on_rtp_packet(cache, stream_id)) != srs_success) {
        return srs_error_wrap(err, "process rtp packet");
    }
    
    return err;
}

SrsRtspAudioCache::SrsRtspAudioCache()
{
    dts = 0;
    audio = NULL;
    payload = NULL;
}

SrsRtspAudioCache::~SrsRtspAudioCache()
{
    srs_freep(audio);
    srs_freep(payload);
}

SrsRtspJitter::SrsRtspJitter()
{
    delta = 0;
    previous_timestamp = 0;
    pts = 0;
}

SrsRtspJitter::~SrsRtspJitter()
{
}

int64_t SrsRtspJitter::timestamp()
{
    return pts;
}

srs_error_t SrsRtspJitter::correct(int64_t& ts)
{
    srs_error_t err = srs_success;
    
    if (previous_timestamp == 0) {
        previous_timestamp = ts;
    }
    
    delta = srs_max(0, (int)(ts - previous_timestamp));
    if (delta > 90000) {
        delta = 0;
    }
    
    previous_timestamp = ts;
    
    ts = pts + delta;
    pts = ts;
    
    return err;
}

SrsRtspConn::SrsRtspConn(SrsRtspCaster* c, srs_netfd_t fd, std::string o)
{
    output_template = o;
    
    session = "";
    video_rtp = NULL;
    audio_rtp = NULL;
    
    caster = c;
    stfd = fd;
    skt = new SrsStSocket();
    rtsp = new SrsRtspStack(skt);
    trd = new SrsSTCoroutine("rtsp", this);
    
    req = NULL;
    sdk = NULL;
    vjitter = new SrsRtspJitter();
    ajitter = new SrsRtspJitter();
    
    avc = new SrsRawH264Stream();
    aac = new SrsRawAacStream();
    acodec = new SrsRawAacStreamCodec();
    acache = new SrsRtspAudioCache();
}

SrsRtspConn::~SrsRtspConn()
{
    close();
    
    srs_close_stfd(stfd);
    
    srs_freep(video_rtp);
    srs_freep(audio_rtp);
    
    srs_freep(trd);
    srs_freep(skt);
    srs_freep(rtsp);
    
    srs_freep(sdk);
    srs_freep(req);
    
    srs_freep(vjitter);
    srs_freep(ajitter);
    srs_freep(acodec);
    srs_freep(acache);
}

srs_error_t SrsRtspConn::serve()
{
    srs_error_t err = srs_success;
    
    if ((err = skt->initialize(stfd)) != srs_success) {
        return srs_error_wrap(err, "socket initialize");
    }
    
    if ((err = trd->start()) != srs_success) {
        return srs_error_wrap(err, "rtsp connection");
    }
    
    return err;
}

srs_error_t SrsRtspConn::do_cycle()
{
    srs_error_t err = srs_success;
    
    // retrieve ip of client.
    std::string ip = srs_get_peer_ip(srs_netfd_fileno(stfd));
    if (ip.empty() && !_srs_config->empty_ip_ok()) {
        srs_warn("empty ip for fd=%d", srs_netfd_fileno(stfd));
    }
    srs_trace("rtsp: serve %s", ip.c_str());
    
    // consume all rtsp messages.
    while (true) {
        if ((err = trd->pull()) != srs_success) {
            return srs_error_wrap(err, "rtsp cycle");
        }
        
        SrsRtspRequest* req = NULL;
        if ((err = rtsp->recv_message(&req)) != srs_success) {
            return srs_error_wrap(err, "recv message");
        }
        SrsAutoFree(SrsRtspRequest, req);
        srs_info("rtsp: got rtsp request");
        
        if (req->is_options()) {
            SrsRtspOptionsResponse* res = new SrsRtspOptionsResponse((int)req->seq);
            res->session = session;
            if ((err = rtsp->send_message(res)) != srs_success) {
                return  srs_error_wrap(err, "response option");
            }
        } else if (req->is_announce()) {
            if (rtsp_tcUrl.empty()) {
                rtsp_tcUrl = req->uri;
            }
            size_t pos = string::npos;
            if ((pos = rtsp_tcUrl.rfind(".sdp")) != string::npos) {
                rtsp_tcUrl = rtsp_tcUrl.substr(0, pos);
            }
            srs_parse_rtmp_url(rtsp_tcUrl, rtsp_tcUrl, rtsp_stream);
            
            srs_assert(req->sdp);
            video_id = ::atoi(req->sdp->video_stream_id.c_str());
            audio_id = ::atoi(req->sdp->audio_stream_id.c_str());
            video_codec = req->sdp->video_codec;
            audio_codec = req->sdp->audio_codec;
            audio_sample_rate = ::atoi(req->sdp->audio_sample_rate.c_str());
            audio_channel = ::atoi(req->sdp->audio_channel.c_str());
            h264_sps = req->sdp->video_sps;
            h264_pps = req->sdp->video_pps;
            aac_specific_config = req->sdp->audio_sh;
            srs_trace("rtsp: video(#%d, %s, %s/%s), audio(#%d, %s, %s/%s, %dHZ %dchannels), %s/%s",
                      video_id, video_codec.c_str(), req->sdp->video_protocol.c_str(), req->sdp->video_transport_format.c_str(),
                      audio_id, audio_codec.c_str(), req->sdp->audio_protocol.c_str(), req->sdp->audio_transport_format.c_str(),
                      audio_sample_rate, audio_channel, rtsp_tcUrl.c_str(), rtsp_stream.c_str()
                      );
            
            SrsRtspResponse* res = new SrsRtspResponse((int)req->seq);
            res->session = session;
            if ((err = rtsp->send_message(res)) != srs_success) {
                return srs_error_wrap(err, "response announce");
            }
        } else if (req->is_setup()) {
            srs_assert(req->transport);
            int lpm = 0;
            if ((err = caster->alloc_port(&lpm)) != srs_success) {
                return srs_error_wrap(err, "alloc port");
            }
            
            SrsRtpConn* rtp = NULL;
            if (req->stream_id == video_id) {
                srs_freep(video_rtp);
                rtp = video_rtp = new SrsRtpConn(this, lpm, video_id);
            } else {
                srs_freep(audio_rtp);
                rtp = audio_rtp = new SrsRtpConn(this, lpm, audio_id);
            }
            if ((err = rtp->listen()) != srs_success) {
                return srs_error_wrap(err, "rtp listen");
            }
            srs_trace("rtsp: #%d %s over %s/%s/%s %s client-port=%d-%d, server-port=%d-%d",
                req->stream_id, (req->stream_id == video_id)? "Video":"Audio",
                req->transport->transport.c_str(), req->transport->profile.c_str(), req->transport->lower_transport.c_str(),
                req->transport->cast_type.c_str(), req->transport->client_port_min, req->transport->client_port_max,
                lpm, lpm + 1);
            
            // create session.
            if (session.empty()) {
                session = "O9EaZ4bf"; // TODO: FIXME: generate session id.
            }
            
            SrsRtspSetupResponse* res = new SrsRtspSetupResponse((int)req->seq);
            res->client_port_min = req->transport->client_port_min;
            res->client_port_max = req->transport->client_port_max;
            res->local_port_min = lpm;
            res->local_port_max = lpm + 1;
            res->session = session;
            if ((err = rtsp->send_message(res)) != srs_success) {
                return srs_error_wrap(err, "response setup");
            }
        } else if (req->is_record()) {
            SrsRtspResponse* res = new SrsRtspResponse((int)req->seq);
            res->session = session;
            if ((err = rtsp->send_message(res)) != srs_success) {
                return srs_error_wrap(err, "response record");
            }
        }
    }
    
    return err;
}

srs_error_t SrsRtspConn::on_rtp_packet(SrsRtpPacket* pkt, int stream_id)
{
    srs_error_t err = srs_success;
    
    // ensure rtmp connected.
    if ((err = connect()) != srs_success) {
        return srs_error_wrap(err, "connect");
    }
    
    if (stream_id == video_id) {
        // rtsp tbn is ts tbn.
        int64_t pts = pkt->timestamp;
        if ((err = vjitter->correct(pts)) != srs_success) {
            return srs_error_wrap(err, "jitter");
        }
        
        // TODO: FIXME: set dts to pts, please finger out the right dts.
        int64_t dts = pts;
        
        return on_rtp_video(pkt, dts, pts);
    } else {
        // rtsp tbn is ts tbn.
        int64_t pts = pkt->timestamp;
        if ((err = ajitter->correct(pts)) != srs_success) {
            return srs_error_wrap(err, "jitter");
        }
        
        return on_rtp_audio(pkt, pts);
    }
    
    return err;
}

srs_error_t SrsRtspConn::cycle()
{
    // serve the rtsp client.
    srs_error_t err = do_cycle();
    
    caster->remove(this);
    
    if (err == srs_success) {
        srs_trace("client finished.");
    } else if (srs_is_client_gracefully_close(err)) {
        srs_warn("client disconnect peer. code=%d", srs_error_code(err));
        srs_freep(err);
    }
    
    if (video_rtp) {
        caster->free_port(video_rtp->port(), video_rtp->port() + 1);
    }
    
    if (audio_rtp) {
        caster->free_port(audio_rtp->port(), audio_rtp->port() + 1);
    }
    
    return err;
}

srs_error_t SrsRtspConn::on_rtp_video(SrsRtpPacket* pkt, int64_t dts, int64_t pts)
{
    srs_error_t err = srs_success;
    
    if ((err = kickoff_audio_cache(pkt, dts)) != srs_success) {
        return srs_error_wrap(err, "kickoff audio cache");
    }
    
    char* bytes = pkt->payload->bytes();
    int length = pkt->payload->length();
    uint32_t fdts = (uint32_t)(dts / 90);
    uint32_t fpts = (uint32_t)(pts / 90);
    if ((err = write_h264_ipb_frame(bytes, length, fdts, fpts)) != srs_success) {
        return srs_error_wrap(err, "write ibp frame");
    }
    
    return err;
}

srs_error_t SrsRtspConn::on_rtp_audio(SrsRtpPacket* pkt, int64_t dts)
{
    srs_error_t err = srs_success;
    
    if ((err = kickoff_audio_cache(pkt, dts)) != srs_success) {
        return srs_error_wrap(err, "kickoff audio cache");
    }
    
    // cache current audio to kickoff.
    acache->dts = dts;
    acache->audio = pkt->audio;
    acache->payload = pkt->payload;
    
    pkt->audio = NULL;
    pkt->payload = NULL;
    
    return err;
}

srs_error_t SrsRtspConn::kickoff_audio_cache(SrsRtpPacket* pkt, int64_t dts)
{
    srs_error_t err = srs_success;
    
    // nothing to kick off.
    if (!acache->payload) {
        return err;
    }
    
    if (dts - acache->dts > 0 && acache->audio->nb_samples > 0) {
        int64_t delta = (dts - acache->dts) / acache->audio->nb_samples;
        for (int i = 0; i < acache->audio->nb_samples; i++) {
            char* frame = acache->audio->samples[i].bytes;
            int nb_frame = acache->audio->samples[i].size;
            int64_t timestamp = (acache->dts + delta * i) / 90;
            acodec->aac_packet_type = 1;
            if ((err = write_audio_raw_frame(frame, nb_frame, acodec, (uint32_t)timestamp)) != srs_success) {
                return srs_error_wrap(err, "write audio raw frame");
            }
        }
    }
    
    acache->dts = 0;
    srs_freep(acache->audio);
    srs_freep(acache->payload);
    
    return err;
}

srs_error_t SrsRtspConn::write_sequence_header()
{
    srs_error_t err = srs_success;
    
    // use the current dts.
    int64_t dts = vjitter->timestamp() / 90;
    
    // send video sps/pps
    if ((err = write_h264_sps_pps((uint32_t)dts, (uint32_t)dts)) != srs_success) {
        return srs_error_wrap(err, "write sps/pps");
    }
    
    // generate audio sh by audio specific config.
    if (true) {
        std::string sh = aac_specific_config;
        
        SrsFormat* format = new SrsFormat();
        SrsAutoFree(SrsFormat, format);
        
        if ((err = format->on_aac_sequence_header((char*)sh.c_str(), (int)sh.length())) != srs_success) {
            return srs_error_wrap(err, "on aac sequence header");
        }
        
        SrsAudioCodecConfig* dec = format->acodec;
        
        acodec->sound_format = SrsAudioCodecIdAAC;
        acodec->sound_type = (dec->aac_channels == 2)? SrsAudioChannelsStereo : SrsAudioChannelsMono;
        acodec->sound_size = SrsAudioSampleBits16bit;
        acodec->aac_packet_type = 0;
        
        static int srs_aac_srates[] = {
            96000, 88200, 64000, 48000,
            44100, 32000, 24000, 22050,
            16000, 12000, 11025,  8000,
            7350,     0,     0,    0
        };
        switch (srs_aac_srates[dec->aac_sample_rate]) {
            case 11025:
                acodec->sound_rate = SrsAudioSampleRate11025;
                break;
            case 22050:
                acodec->sound_rate = SrsAudioSampleRate22050;
                break;
            case 44100:
                acodec->sound_rate = SrsAudioSampleRate44100;
                break;
            default:
                break;
        };
        
        if ((err = write_audio_raw_frame((char*)sh.data(), (int)sh.length(), acodec, (uint32_t)dts)) != srs_success) {
            return srs_error_wrap(err, "write audio raw frame");
        }
    }
    
    return err;
}

srs_error_t SrsRtspConn::write_h264_sps_pps(uint32_t dts, uint32_t pts)
{
    srs_error_t err = srs_success;
    
    // h264 raw to h264 packet.
    std::string sh;
    if ((err = avc->mux_sequence_header(h264_sps, h264_pps, dts, pts, sh)) != srs_success) {
        return srs_error_wrap(err, "mux sequence header");
    }
    
    // h264 packet to flv packet.
    int8_t frame_type = SrsVideoAvcFrameTypeKeyFrame;
    int8_t avc_packet_type = SrsVideoAvcFrameTraitSequenceHeader;
    char* flv = NULL;
    int nb_flv = 0;
    if ((err = avc->mux_avc2flv(sh, frame_type, avc_packet_type, dts, pts, &flv, &nb_flv)) != srs_success) {
        return srs_error_wrap(err, "mux avc to flv");
    }
    
    // the timestamp in rtmp message header is dts.
    uint32_t timestamp = dts;
    if ((err = rtmp_write_packet(SrsFrameTypeVideo, timestamp, flv, nb_flv)) != srs_success) {
        return srs_error_wrap(err, "write packet");
    }
    
    return err;
}

srs_error_t SrsRtspConn::write_h264_ipb_frame(char* frame, int frame_size, uint32_t dts, uint32_t pts)
{
    srs_error_t err = srs_success;
    
    // 5bits, 7.3.1 NAL unit syntax,
    // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
    //  7: SPS, 8: PPS, 5: I Frame, 1: P Frame
    SrsAvcNaluType nal_unit_type = (SrsAvcNaluType)(frame[0] & 0x1f);
    
    // for IDR frame, the frame is keyframe.
    SrsVideoAvcFrameType frame_type = SrsVideoAvcFrameTypeInterFrame;
    if (nal_unit_type == SrsAvcNaluTypeIDR) {
        frame_type = SrsVideoAvcFrameTypeKeyFrame;
    }
    
    std::string ibp;
    if ((err = avc->mux_ipb_frame(frame, frame_size, ibp)) != srs_success) {
        return srs_error_wrap(err, "mux ibp frame");
    }
    
    int8_t avc_packet_type = SrsVideoAvcFrameTraitNALU;
    char* flv = NULL;
    int nb_flv = 0;
    if ((err = avc->mux_avc2flv(ibp, frame_type, avc_packet_type, dts, pts, &flv, &nb_flv)) != srs_success) {
        return srs_error_wrap(err, "mux avc to flv");
    }
    
    // the timestamp in rtmp message header is dts.
    uint32_t timestamp = dts;
    return rtmp_write_packet(SrsFrameTypeVideo, timestamp, flv, nb_flv);
}

srs_error_t SrsRtspConn::write_audio_raw_frame(char* frame, int frame_size, SrsRawAacStreamCodec* codec, uint32_t dts)
{
    srs_error_t err = srs_success;
    
    char* data = NULL;
    int size = 0;
    if ((err = aac->mux_aac2flv(frame, frame_size, codec, dts, &data, &size)) != srs_success) {
        return srs_error_wrap(err, "mux aac to flv");
    }
    
    return rtmp_write_packet(SrsFrameTypeAudio, dts, data, size);
}

srs_error_t SrsRtspConn::rtmp_write_packet(char type, uint32_t timestamp, char* data, int size)
{
    srs_error_t err = srs_success;
    
    if ((err = connect()) != srs_success) {
        return srs_error_wrap(err, "connect");
    }
    
    SrsSharedPtrMessage* msg = NULL;
    
    if ((err = srs_rtmp_create_msg(type, timestamp, data, size, sdk->sid(), &msg)) != srs_success) {
        return srs_error_wrap(err, "create message");
    }
    srs_assert(msg);
    
    // send out encoded msg.
    if ((err = sdk->send_and_free_message(msg)) != srs_success) {
        close();
        return srs_error_wrap(err, "write message");
    }
    
    return err;
}

srs_error_t SrsRtspConn::connect()
{
    srs_error_t err = srs_success;
    
    // Ignore when connected.
    if (sdk) {
        return err;
    }
    
    // generate rtmp url to connect to.
    std::string url;
    if (!req) {
        std::string schema, host, vhost, app, param;
        int port;
        srs_discovery_tc_url(rtsp_tcUrl, schema, host, vhost, app, rtsp_stream, port, param);

        // generate output by template.
        std::string output = output_template;
        output = srs_string_replace(output, "[app]", app);
        output = srs_string_replace(output, "[stream]", rtsp_stream);
    }
    
    // connect host.
    srs_utime_t cto = SRS_CONSTS_RTMP_TIMEOUT;
    srs_utime_t sto = SRS_CONSTS_RTMP_PULSE;
    sdk = new SrsSimpleRtmpClient(url, cto, sto);
    
    if ((err = sdk->connect()) != srs_success) {
        close();
        return srs_error_wrap(err, "connect %s failed, cto=%dms, sto=%dms.", url.c_str(), srsu2msi(cto), srsu2msi(sto));
    }
    
    // publish.
    if ((err = sdk->publish(SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE)) != srs_success) {
        close();
        return srs_error_wrap(err, "publish %s failed", url.c_str());
    }
    
    return write_sequence_header();
}

void SrsRtspConn::close()
{
    srs_freep(sdk);
}

SrsRtspCaster::SrsRtspCaster(SrsConfDirective* c)
{
    // TODO: FIXME: support reload.
    output = _srs_config->get_stream_caster_output(c);
    local_port_min = _srs_config->get_stream_caster_rtp_port_min(c);
    local_port_max = _srs_config->get_stream_caster_rtp_port_max(c);
}

SrsRtspCaster::~SrsRtspCaster()
{
    std::vector<SrsRtspConn*>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        SrsRtspConn* conn = *it;
        srs_freep(conn);
    }
    clients.clear();
    used_ports.clear();
}

srs_error_t SrsRtspCaster::alloc_port(int* pport)
{
    srs_error_t err = srs_success;
    
    // use a pair of port.
    for (int i = local_port_min; i < local_port_max - 1; i += 2) {
        if (!used_ports[i]) {
            used_ports[i] = true;
            used_ports[i + 1] = true;
            *pport = i;
            break;
        }
    }
    srs_info("rtsp: alloc port=%d-%d", *pport, *pport + 1);
    
    return err;
}

void SrsRtspCaster::free_port(int lpmin, int lpmax)
{
    for (int i = lpmin; i < lpmax; i++) {
        used_ports[i] = false;
    }
    srs_trace("rtsp: free rtp port=%d-%d", lpmin, lpmax);
}

srs_error_t SrsRtspCaster::on_tcp_client(srs_netfd_t stfd)
{
    srs_error_t err = srs_success;
    
    SrsRtspConn* conn = new SrsRtspConn(this, stfd, output);
    
    if ((err = conn->serve()) != srs_success) {
        srs_freep(conn);
        return srs_error_wrap(err, "serve conn");
    }
    
    clients.push_back(conn);
    
    return err;
}

void SrsRtspCaster::remove(SrsRtspConn* conn)
{
    std::vector<SrsRtspConn*>::iterator it = find(clients.begin(), clients.end(), conn);
    if (it != clients.end()) {
        clients.erase(it);
    }
    srs_info("rtsp: remove connection from caster.");
    
    srs_freep(conn);
}

