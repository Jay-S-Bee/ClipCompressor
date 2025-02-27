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

    // Video Decoder Setup
    const AVCodec* vDecoder = avcodec_find_decoder(inputFormat->streams[vidStreamIndex]->codecpar->codec_id);
    AVCodecContext* vidDecoderCont = avcodec_alloc_context3(vDecoder);
    avcodec_parameters_to_context(vidDecoderCont, inputFormat->streams[vidStreamIndex]->codecpar);
    avcodec_open2(vidDecoderCont, vDecoder, nullptr);

    // Video Encoder Setup
    const AVCodec* videoEncoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    AVCodecContext* vidEncoderCont = avcodec_alloc_context3(videoEncoder);

    // Set bitrate (testing to get below 10MB)
    vidEncoderCont->bit_rate = 1000000;  // Set to target video bitrate
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

    // Add audio stream (copying audio without re-encoding)
    AVStream* audioOutStream = nullptr;
    if (audioStreamIndex != -1) {
        audioOutStream = avformat_new_stream(outputFormat, nullptr);
        avcodec_parameters_copy(audioOutStream->codecpar, inputFormat->streams[audioStreamIndex]->codecpar);
        audioOutStream->time_base = inputFormat->streams[audioStreamIndex]->time_base;  // Copy timebase from audio stream
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
                scaledFrame->pts = frame->pts;  // Copy the pts from the original frame

                avcodec_send_frame(vidEncoderCont, scaledFrame);
                while (avcodec_receive_packet(vidEncoderCont, packet) >= 0) {
                    // Ensures correct timestamps for video
                    packet->stream_index = vidOutStream->index;
                    // Remove timestamp rescaling for video if timebase is already correct
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
        else if (packet->stream_index == audioStreamIndex && audioOutStream != nullptr) {
            // Copy the audio packet without re-encoding
            packet->stream_index = audioOutStream->index;
            // Remove timestamp rescaling for audio if timebase is already correct
            av_interleaved_write_frame(outputFormat, packet);
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
    avformat_close_input(&inputFormat);
    avio_close(outputFormat->pb);
    avformat_free_context(outputFormat);
    av_frame_free(&frame);
    av_frame_free(&scaledFrame);
    av_packet_free(&packet);

    MessageBox(hWnd, L"File successfully compressed!", L"Compression Complete", MB_OK | MB_ICONINFORMATION);
    OutputDebugString(L"Processing finished!\n");

}
