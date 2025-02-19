
#include "framework.h"
#include "main.h"
#include "compression.h"
#include <commdlg.h>
#include <shlwapi.h> // To use PathFindFileName
#include <windows.h>
#include <iostream>
#include <CommCtrl.h> // For progress bar   
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
WCHAR g_selectedFilePath[260];				  //  save selected file path for compression
HBRUSH hbrBackground;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CLIPCOMPRESSOR, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIPCOMPRESSOR));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CLIPCOMPRESSOR));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_CLIPCOMPRESSOR);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in global variable
    int width = 310;
    int height = 340;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        960, 620, width, height, nullptr, nullptr, hInstance, nullptr);

    HWND hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        10, 200, 275, 30, hWnd, (HMENU)IDC_PROGRESS_BAR, hInstance, NULL);
    ShowWindow(hProgressBar, SW_HIDE);  // Hide progress bar initially


    hbrBackground = CreateSolidBrush(RGB(160, 160, 160));

    // Browse button
    HWND hButtonBrowse = CreateWindowW(L"BUTTON", L"Browse",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        220, 40, 65, 30, hWnd, (HMENU)IDC_BUTTON_BROWSE, hInstance, NULL);

    // text box to display filename
    HWND hStaticFilename = CreateWindowW(L"STATIC", L"",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        10, 40, 200, 30, hWnd, (HMENU)IDC_STATIC_FILENAME, hInstance, NULL);

    // editable text box for output filename
    HWND hEditOutputFilename = CreateWindowW(L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        10, 120, 275, 30, hWnd, (HMENU)IDC_EDIT_OUTPUT_FILENAME, hInstance, NULL);

    // compression button
    HWND hButtonCompress = CreateWindowW(L"BUTTON", L"Compress",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        10, 160, 100, 30, hWnd, (HMENU)IDC_BUTTON_COMPRESS, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDC_BUTTON_BROWSE:
        {
            OPENFILENAME ofn;       // common dialog box structure
            WCHAR selFile[260];       // buffer for selected file name

            // Initialize OPENFILENAME
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = selFile;
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = sizeof(selFile);
            ofn.lpstrFilter = L"MP4 Files\0*.mp4\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            // Display the Open dialog box
            if (GetOpenFileName(&ofn) == TRUE) {
                WCHAR* fileName = PathFindFileName(selFile);
                SetWindowText(GetDlgItem(hWnd, IDC_STATIC_FILENAME), fileName);
                wcscpy_s(g_selectedFilePath, selFile);   // save selected file path for compression

                WCHAR outputFileName[260];
                wcscpy_s(outputFileName, fileName);
                PathRemoveExtension(outputFileName);
                wcscat_s(outputFileName, L"_Compressed.mp4"); //add suffix to prevent overwriting the original file

                SetWindowText(GetDlgItem(hWnd, IDC_EDIT_OUTPUT_FILENAME), outputFileName); // Second text box that is editable

                // Debugging message to confirm file selection
                OutputDebugString(L"Selected File: ");
                OutputDebugString(selFile);
                OutputDebugString(L"\n");
            }
        }
        break;
        case IDC_EDIT_OUTPUT_FILENAME:
            if (wmEvent == EN_KILLFOCUS) {
                WCHAR outputFileName[260];
                GetWindowText(GetDlgItem(hWnd, IDC_EDIT_OUTPUT_FILENAME), outputFileName, 260);

                // Makes the filename always end with .mp4
                if (wcslen(outputFileName) < 4 || _wcsicmp(outputFileName + wcslen(outputFileName) - 4, L".mp4") != 0) {
                    wcscat_s(outputFileName, L".mp4");
                    SetWindowText(GetDlgItem(hWnd, IDC_EDIT_OUTPUT_FILENAME), outputFileName);
                }
            }
            break;
        case IDC_BUTTON_COMPRESS:
        {
            WCHAR outputFilePath[260];
            GetWindowText(GetDlgItem(hWnd, IDC_EDIT_OUTPUT_FILENAME), outputFilePath, 260);

            // Check if the file already exists
            if (PathFileExists(outputFilePath)) {
                int msgboxID = MessageBox(
                    hWnd,
                    L"A file with the same name already exists. Do you want to overwrite it?",
                    L"File Exists",
                    MB_ICONWARNING | MB_YESNO
                );
                if (msgboxID == IDNO) {
                    break;
                }
            }
            // Convert WCHAR to char
            char inputFile[260];
            char outputFile[260];
            WideCharToMultiByte(CP_ACP, 0, g_selectedFilePath, -1, inputFile, 260, NULL, NULL);
            WideCharToMultiByte(CP_ACP, 0, outputFilePath, -1, outputFile, 260, NULL, NULL);


            // Debugging messages to confirm file paths
            OutputDebugString(L"Input File: ");
            OutputDebugString(g_selectedFilePath);
            OutputDebugString(L"\n");
            OutputDebugString(L"Output File: ");
            OutputDebugString(outputFilePath);
            OutputDebugString(L"\n");
            OutputDebugString(L"Starting compression process\n");

            long targetSizeMB = 10; // Set target file size in MB (currently unused)
            ProcessVideo(inputFile, outputFile, hWnd);
        }
        break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        // SetBkColor(hdcStatic, RGB(160, 160, 160)); 
        return (INT_PTR)hbrBackground;
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        SetBkMode(hdc, TRANSPARENT);

        const wchar_t* text1 = L"File to be compressed:";
        TextOut(hdc, 10, 20, text1, wcslen(text1));
        const wchar_t* text2 = L"Output file name:";
        TextOut(hdc, 10, 100, text2, wcslen(text2));
        //const wchar_t* text3 = L"Additional line of text:";
        //TextOut(hdc, 10, 180, text3, wcslen(text3));

        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        DeleteObject(hbrBackground); // Clean up the brush
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Updating Progress Bar Function
void UpdateProgress(HWND hWnd, int progress, bool& isCompressionInProgress) {
    HWND hProgressBar = GetDlgItem(hWnd, IDC_PROGRESS_BAR);

    // Show progress bar when compression starts
    if (!isCompressionInProgress && progress > 0) {
        ShowWindow(hProgressBar, SW_SHOW);
        isCompressionInProgress = true;    //  Compression is in progress
    }

    // Hide progress bar when compression finishes
    if (progress == 100) {
        ShowWindow(hProgressBar, SW_HIDE);
        isCompressionInProgress = false;   // Reset bool after compression finishes
    }
    SendMessage(hProgressBar, PBM_SETPOS, (WPARAM)progress, 0);    // Update progress bar during compression
}
// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
