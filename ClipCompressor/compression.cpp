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
    AVFormatContext* inputFormatContext = nullptr;
    if (avformat_open_input(&inputFormatContext, inputFilePath, nullptr, nullptr) != 0) {
        OutputDebugString(L"Could not open input file.\n");
        return;
    }
    OutputDebugString(L"Input file opened.\n");

    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        OutputDebugString(L"Could not find stream info.\n");
        avformat_close_input(&inputFormatContext);
        return;
    }
    OutputDebugString(L"Stream info found.\n");

    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    for (unsigned int i = 0; i < static_cast<unsigned int>(inputFormatContext->nb_streams); i++) {
        if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
        }
        else if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
        }
    }

    if (videoStreamIndex == -1) {
        OutputDebugString(L"No video stream found.\n");
        avformat_close_input(&inputFormatContext);
        return;
    }
    OutputDebugString(L"Video stream found.\n");

    // Video Decoder Setup
    const AVCodec* videoDecoder = avcodec_find_decoder(inputFormatContext->streams[videoStreamIndex]->codecpar->codec_id);
    AVCodecContext* videoDecoderContext = avcodec_alloc_context3(videoDecoder);
    avcodec_parameters_to_context(videoDecoderContext, inputFormatContext->streams[videoStreamIndex]->codecpar);
    avcodec_open2(videoDecoderContext, videoDecoder, nullptr);

    // Video Encoder Setup
    const AVCodec* videoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodecContext* videoEncoderContext = avcodec_alloc_context3(videoEncoder);

    // Set bitrate (testing to get below 10MB)
    videoEncoderContext->bit_rate = 500000;  // Set to target video bitrate
    videoEncoderContext->rc_min_rate = videoEncoderContext->bit_rate;  // Same as target bitrate
    videoEncoderContext->rc_max_rate = videoEncoderContext->bit_rate;  // Same as target bitrate
    videoEncoderContext->rc_buffer_size = videoEncoderContext->bit_rate * 1.5;
    videoEncoderContext->gop_size = 120;  // Keyframe every number of frames (higher more compression)
    videoEncoderContext->width = 1280; // Scaled resolution width
    videoEncoderContext->height = 720; // Scaled resolution height
    videoEncoderContext->time_base = inputFormatContext->streams[videoStreamIndex]->time_base;  // Copy timebase
    videoEncoderContext->framerate = videoDecoderContext->framerate; // Set framerate correctly
    videoEncoderContext->pix_fmt = AV_PIX_FMT_YUV420P;  // Most common pixel format
    avcodec_open2(videoEncoderContext, videoEncoder, nullptr);

    // Output file setup
    AVFormatContext* outputFormatContext = nullptr;
    if (avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, outputFilePath) < 0) {
        OutputDebugString(L"Could not create output context.\n");
        return;
    }
    OutputDebugString(L"Output context created.\n");

    // Add video stream to output file
    AVStream* videoOutStream = avformat_new_stream(outputFormatContext, nullptr);
    avcodec_parameters_from_context(videoOutStream->codecpar, videoEncoderContext);
    videoOutStream->time_base = videoEncoderContext->time_base;  // Copy timebase from video stream

    // Add audio stream (copying audio without re-encoding)
    AVStream* audioOutStream = nullptr;
    if (audioStreamIndex != -1) {
        audioOutStream = avformat_new_stream(outputFormatContext, nullptr);
        avcodec_parameters_copy(audioOutStream->codecpar, inputFormatContext->streams[audioStreamIndex]->codecpar);
        audioOutStream->time_base = inputFormatContext->streams[audioStreamIndex]->time_base;  // Copy timebase from audio stream
    }

    // Open output file for writing
    if (avio_open(&outputFormatContext->pb, outputFilePath, AVIO_FLAG_WRITE) < 0) {
        OutputDebugString(L"Could not open output file for writing.\n");
        return;
    }
    OutputDebugString(L"Output file opened for writing.\n");

    if (avformat_write_header(outputFormatContext, nullptr) < 0) {
        OutputDebugString(L"Could not write header to output file.\n");
        return;
    }
    OutputDebugString(L"Header written.\n");

    // Packet and frame buffers for reading and writing frames
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* scaledFrame = av_frame_alloc(); // Scaled frame
    scaledFrame->format = videoEncoderContext->pix_fmt;
    scaledFrame->width = videoEncoderContext->width;
    scaledFrame->height = videoEncoderContext->height;
    av_frame_get_buffer(scaledFrame, 0);

    struct SwsContext* swsContext = sws_getContext(
        videoDecoderContext->width, videoDecoderContext->height, videoDecoderContext->pix_fmt,
        videoEncoderContext->width, videoEncoderContext->height, videoEncoderContext->pix_fmt,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    int frameCount = 0;  // Debugging variable to track the number of frames processed
    int totalFrames = inputFormatContext->streams[videoStreamIndex]->nb_frames; // Total frames to process for progress bar

    // Process the frames
    while (av_read_frame(inputFormatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            avcodec_send_packet(videoDecoderContext, packet);
            while (avcodec_receive_frame(videoDecoderContext, frame) >= 0) {
                // Scale the frame
                sws_scale(swsContext, frame->data, frame->linesize, 0, videoDecoderContext->height,
                    scaledFrame->data, scaledFrame->linesize);

                // Calculate timestamp for the scaled frame (to ensure correct playback speed)
                scaledFrame->pts = frame->pts;  // Copy the pts from the original frame

                avcodec_send_frame(videoEncoderContext, scaledFrame);
                while (avcodec_receive_packet(videoEncoderContext, packet) >= 0) {
                    // Ensures correct timestamps for video
                    packet->stream_index = videoOutStream->index;
                    // Remove timestamp rescaling for video if timebase is already correct
                    av_interleaved_write_frame(outputFormatContext, packet);
                    av_packet_unref(packet);  // Unref after writing
                }
                frameCount++;  // Increment frame count
                if (frameCount % 10 == 0) {
                    int progress = (frameCount * 100) / totalFrames;
                    UpdateProgress(hWnd, progress, isCompressionStarted);  // Update the progress bar
                }
            }
        }
        else if (packet->stream_index == audioStreamIndex && audioOutStream != nullptr) {
            // Copy the audio packet without re-encoding
            packet->stream_index = audioOutStream->index;
            // Remove timestamp rescaling for audio if timebase is already correct
            av_interleaved_write_frame(outputFormatContext, packet);
        }

        av_packet_unref(packet); // Unref after processing video/audio packets
    }

    // Finalize the output
    av_write_trailer(outputFormatContext);

    //  Debug output frame count
    std::wstring frameCountStr = L"Total frames processed: " + std::to_wstring(frameCount) + L"\n";
    OutputDebugString(frameCountStr.c_str());

    MessageBox(hWnd, L"File successfully compressed!", L"Compression Complete", MB_OK | MB_ICONINFORMATION);
    UpdateProgress(hWnd, 100, isCompressionStarted);  // Update the progress bar to 100%

    // Cleanup
    sws_freeContext(swsContext);
    avcodec_free_context(&videoDecoderContext);
    avcodec_free_context(&videoEncoderContext);
    avformat_close_input(&inputFormatContext);
    avio_close(outputFormatContext->pb);
    avformat_free_context(outputFormatContext);
    av_frame_free(&frame);
    av_frame_free(&scaledFrame);
    av_packet_free(&packet);

    OutputDebugString(L"Processing finished!\n");

}
