
#ifndef COMPRESSION_H
#define COMPRESSION_H



void ProcessVideo(const char* inputFilePath, const char* outputFilePath, HWND hWnd);
void UpdateProgress(HWND hWnd, int progress, bool& isCompressionStarted);
#endif#pragma once
