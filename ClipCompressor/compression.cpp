#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include "compression.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

bool isCompressionStarted = false;  // Flag to check if compression has started

void ProcessVideo(const char* inputFilePath, const char* outputFilePath, HWND hWnd) {
    avformat_network_init();
    OutputDebugString(L"Initialization started.\n");

    // Open input file
    AVFormatContext* inputFormat = nullptr;
    if (avformat_open_input(&inputFormat, inputFilePath, nullptr, nullptr) != 0) {
        OutputDebugString(L"Could not open input file.\n");
        return;
    }
    OutputDebugString(L"Input file opened.\n");

    if (avformat_find_stream_info(inputFormat, nullptr) < 0) {
        OutputDebugString(L"Could not find stream info.\n");
        avformat_close_input(&inputFormat);
        return;
    }
    OutputDebugString(L"Stream info found.\n");

    int vidStreamIndex = -1;
    int audioStreamIndex = -1;
    for (unsigned int i = 0; i < static_cast<unsigned int>(inputFormat->nb_streams); i++) {
        if (inputFormat->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vidStreamIndex = i;
        }
        else if (inputFormat->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
        }
    }

    if (vidStreamIndex == -1) {
        OutputDebugString(L"No video stream found.\n");
        avformat_close_input(&inputFormat);
        return;
    }
    OutputDebugString(L"Video stream found.\n");

    // Calculate the duration of the video in seconds
    int64_t duration = inputFormat->duration / AV_TIME_BASE;

    // Calculate the target bitrate for file (temporary until custom settings are implemented)
    int64_t targetSize = 9 * 1024 * 1024;  // 9MB in bytes (10 causes file to be larger than 10MB)
    int targetBitrate = static_cast<int>((targetSize * 8) / duration);  // in bits per second

    // Allocate bitrates for video and audio
    int videoBitrate = static_cast<int>(targetBitrate * 0.8);  // % of total bitrate
    int audioBitrate = static_cast<int>(targetBitrate * 0.2);

    // Video Decoder Setup
    const AVCodec* vDecoder = avcodec_find_decoder(inputFormat->streams[vidStreamIndex]->codecpar->codec_id);
    AVCodecContext* vidDecoderCont = avcodec_alloc_context3(vDecoder);
    avcodec_parameters_to_context(vidDecoderCont, inputFormat->streams[vidStreamIndex]->codecpar);
    avcodec_open2(vidDecoderCont, vDecoder, nullptr);

    // Video Encoder Setup
    const AVCodec* videoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodecContext* vidEncoderCont = avcodec_alloc_context3(videoEncoder);

    // Set video bitrate
    vidEncoderCont->bit_rate = videoBitrate;
    vidEncoderCont->rc_min_rate = vidEncoderCont->bit_rate;
    vidEncoderCont->rc_max_rate = vidEncoderCont->bit_rate;
    vidEncoderCont->rc_buffer_size = vidEncoderCont->bit_rate * 1.5;
    vidEncoderCont->gop_size = 120;  // Keyframe every number of frames (higher more compression)
    vidEncoderCont->width = 1280;
    vidEncoderCont->height = 720;
    vidEncoderCont->time_base = inputFormat->streams[vidStreamIndex]->time_base;  // Copy timebase
    vidEncoderCont->framerate = vidDecoderCont->framerate; // Set framerate correctly
    vidEncoderCont->pix_fmt = AV_PIX_FMT_YUV420P;
    avcodec_open2(vidEncoderCont, videoEncoder, nullptr);

    // Audio Decoder Setup
    const AVCodec* aDecoder = avcodec_find_decoder(inputFormat->streams[audioStreamIndex]->codecpar->codec_id);
    AVCodecContext* audDecoderCont = avcodec_alloc_context3(aDecoder);
    avcodec_parameters_to_context(audDecoderCont, inputFormat->streams[audioStreamIndex]->codecpar);
    avcodec_open2(audDecoderCont, aDecoder, nullptr);

    // Audio Encoder Setup
    const AVCodec* audioEncoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
    AVCodecContext* audEncoderCont = avcodec_alloc_context3(audioEncoder);

    // Set audio bitrate
    audEncoderCont->bit_rate = audioBitrate;
    audEncoderCont->sample_rate = 44100;
    av_channel_layout_default(&audEncoderCont->ch_layout, 2);
    audEncoderCont->ch_layout.nb_channels = av_popcount64(audEncoderCont->ch_layout.u.mask);
    audEncoderCont->sample_fmt = AV_SAMPLE_FMT_FLTP; // Set to a common format like AV_SAMPLE_FMT_FLTP
    avcodec_open2(audEncoderCont, audioEncoder, nullptr);

    // Output file setup
    AVFormatContext* outputFormat = nullptr;
    if (avformat_alloc_output_context2(&outputFormat, nullptr, nullptr, outputFilePath) < 0) {
        OutputDebugString(L"Could not create output context.\n");
        return;
    }
    OutputDebugString(L"Output context created.\n");

    // Add video stream to output file
    AVStream* vidOutStream = avformat_new_stream(outputFormat, nullptr);
    avcodec_parameters_from_context(vidOutStream->codecpar, vidEncoderCont);
    vidOutStream->time_base = vidEncoderCont->time_base;  // Copy timebase from video stream

    // Add audio stream to output file
    AVStream* audioOutStream = nullptr;
    if (audioStreamIndex != -1) {
        audioOutStream = avformat_new_stream(outputFormat, nullptr);
        avcodec_parameters_from_context(audioOutStream->codecpar, audEncoderCont);
        audioOutStream->time_base = { 1, audEncoderCont->sample_rate };  // Set timebase for audio stream
    }

    // Open output file for writing
    if (avio_open(&outputFormat->pb, outputFilePath, AVIO_FLAG_WRITE) < 0) {
        OutputDebugString(L"Could not open output file for writing.\n");
        return;
    }
    OutputDebugString(L"Output file opened for writing.\n");

    if (avformat_write_header(outputFormat, nullptr) < 0) {
        OutputDebugString(L"Could not write header to output file.\n");
        return;
    }
    OutputDebugString(L"Header written.\n");

    // Packet and frame buffers for reading and writing frames
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* scaledFrame = av_frame_alloc(); // Scaled frame
    scaledFrame->format = vidEncoderCont->pix_fmt;
    scaledFrame->width = vidEncoderCont->width;
    scaledFrame->height = vidEncoderCont->height;
    av_frame_get_buffer(scaledFrame, 0);

    struct SwsContext* swsContext = sws_getContext(
        vidDecoderCont->width, vidDecoderCont->height, vidDecoderCont->pix_fmt,
        vidEncoderCont->width, vidEncoderCont->height, vidEncoderCont->pix_fmt,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    int frameCount = 0;  // Debugging variable to track the number of frames processed
    int totalFrames = inputFormat->streams[vidStreamIndex]->nb_frames; // Total frames to process for progress bar

    // Process the frames
    while (av_read_frame(inputFormat, packet) >= 0) {
        if (packet->stream_index == vidStreamIndex) {
            avcodec_send_packet(vidDecoderCont, packet);
            while (avcodec_receive_frame(vidDecoderCont, frame) >= 0) {
                // Scale the frame
                sws_scale(swsContext, frame->data, frame->linesize, 0, vidDecoderCont->height,
                    scaledFrame->data, scaledFrame->linesize);

                // Calculate timestamp for the scaled frame (to ensure correct playback speed)
                scaledFrame->pts = av_rescale_q(frame->pts, inputFormat->streams[vidStreamIndex]->time_base, vidEncoderCont->time_base);

                avcodec_send_frame(vidEncoderCont, scaledFrame);
                while (avcodec_receive_packet(vidEncoderCont, packet) >= 0) {
                    // Ensures correct timestamps for video
                    packet->stream_index = vidOutStream->index;
                    av_packet_rescale_ts(packet, vidEncoderCont->time_base, vidOutStream->time_base);
                    av_interleaved_write_frame(outputFormat, packet);
                    av_packet_unref(packet);  // Unref after writing
                }
                frameCount++;  // Increment frame count
                if (frameCount % 10 == 0) {
                    int progress = (frameCount * 100) / totalFrames;
                    UpdateProgress(hWnd, progress, isCompressionStarted);  // Update the progress bar
                }
            }
        }
        // Audio processing
        else if (packet->stream_index == audioStreamIndex && audioOutStream != nullptr) {
            if (avcodec_send_packet(audDecoderCont, packet) < 0) {
                OutputDebugString(L"Error sending audio packet for decoding.\n");
            }
            while (avcodec_receive_frame(audDecoderCont, frame) >= 0) {
                OutputDebugString(L"Audio frame received for encoding.\n");
                frame->pts = av_rescale_q(frame->pts, inputFormat->streams[audioStreamIndex]->time_base, audEncoderCont->time_base);
                if (avcodec_send_frame(audEncoderCont, frame) < 0) {
                    OutputDebugString(L"Error sending audio frame for encoding.\n");
                }
                while (avcodec_receive_packet(audEncoderCont, packet) >= 0) {
                    OutputDebugString(L"Audio packet received from encoder.\n");
                    // Ensures correct timestamps for audio
                    packet->stream_index = audioOutStream->index;
                    av_packet_rescale_ts(packet, audEncoderCont->time_base, audioOutStream->time_base);
                    if (av_interleaved_write_frame(outputFormat, packet) < 0) {
                        OutputDebugString(L"Error writing audio packet to output file.\n");
                    }
                    av_packet_unref(packet);  // Unref after writing
                }
            }
        }

        av_packet_unref(packet); // Unref after processing video/audio packets
    }

    // Finalize the output
    av_write_trailer(outputFormat);

    //  Debug output frame count
    std::wstring frameCountStr = L"Total frames processed: " + std::to_wstring(frameCount) + L"\n";
    OutputDebugString(frameCountStr.c_str());
    UpdateProgress(hWnd, 100, isCompressionStarted);  // Update the progress bar to 100%

    // Cleanup
    sws_freeContext(swsContext);
    avcodec_free_context(&vidDecoderCont);
    avcodec_free_context(&vidEncoderCont);
    avcodec_free_context(&audDecoderCont);
    avcodec_free_context(&audEncoderCont);
    avformat_close_input(&inputFormat);
    avio_close(outputFormat->pb);
    avformat_free_context(outputFormat);
    av_frame_free(&frame);
    av_frame_free(&scaledFrame);
    av_packet_free(&packet);

    MessageBox(hWnd, L"File successfully compressed!", L"Compression Complete", MB_OK | MB_ICONINFORMATION);
    OutputDebugString(L"Processing finished!\n");

}
