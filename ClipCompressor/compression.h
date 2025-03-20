
#ifndef COMPRESSION_H
#define COMPRESSION_H



void ProcessVideo(const char* inputFilePath, const char* outputFilePath, HWND hWnd, const wchar_t* resolution);
void UpdateProgress(HWND hWnd, int progress, bool& isCompressionStarted);
#endif#pragma once
