
#include "framework.h"
#include "main.h"
#include "compression.h"
#include <commdlg.h>
#include <shlwapi.h> // To use PathFindFileName
#include <windows.h>
#include <iostream>
#include <CommCtrl.h> // For progress bar   
#include <Shellapi.h> // For DragAcceptFiles
#define MAX_LOADSTRING 100

// Global Variables
HINSTANCE hInst;                               
WCHAR szTitle[MAX_LOADSTRING];                  
WCHAR szWindowClass[MAX_LOADSTRING];            
WCHAR g_selectedFilePath[260];				  //  save selected file path for compression
HWND settingsBox, targetSizeSel, resSelect, encSelect;



// Forward declarations of functions included in this code module
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

    // Perform application initialization
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL accelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIPCOMPRESSOR));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, accelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wndClass;

    wndClass.cbSize = sizeof(WNDCLASSEX);

    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CLIPCOMPRESSOR));
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2); //possibly change or remove this
    wndClass.lpszMenuName = MAKEINTRESOURCEW(IDC_CLIPCOMPRESSOR);
    wndClass.lpszClassName = szWindowClass;
    wndClass.hIconSm = LoadIcon(wndClass.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wndClass);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in global variable
    int width = 600;
    int height = 400;
    HWND mainWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        960, 620, width, height, nullptr, nullptr, hInstance, nullptr);
    DragAcceptFiles(mainWnd, TRUE);

    HWND progressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        10, 200, 275, 30, mainWnd, (HMENU)IDC_PROGRESS_BAR, hInstance, NULL);
    ShowWindow(progressBar, SW_HIDE);  // Hide progress bar initially

    // Browse button
    HWND buttonBrowse = CreateWindowW(L"BUTTON", L"Browse",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        220, 40, 65, 30, mainWnd, (HMENU)IDC_BUTTON_BROWSE, hInstance, NULL);

    // text box to display filename
    HWND textFilename = CreateWindowW(L"STATIC", L"",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        10, 40, 200, 30, mainWnd, (HMENU)IDC_STATIC_FILENAME, hInstance, NULL);

    // editable text box for output filename
    HWND editOutputName = CreateWindowW(L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT,
        10, 120, 275, 30, mainWnd, (HMENU)IDC_EDIT_OUTPUT_FILENAME, hInstance, NULL);

    // compression button
    HWND buttonCompress = CreateWindowW(L"BUTTON", L"Compress",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        10, 160, 100, 30, mainWnd, (HMENU)IDC_BUTTON_COMPRESS, hInstance, NULL);

    // Settings Box
    settingsBox = CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        290, 10, 260, 280, mainWnd, NULL, hInstance, NULL);

    // Number input for target file size
    targetSizeSel = CreateWindowW(L"EDIT", L"10",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_NUMBER,
        90, 20, 30, 20, settingsBox, NULL, hInstance, NULL);

    // Dropdown for different resolutions
    resSelect = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        90, 45, 100, 100, settingsBox, NULL, hInstance, NULL);
    SendMessage(resSelect, CB_ADDSTRING, 0, (LPARAM)L"480p");
    SendMessage(resSelect, CB_ADDSTRING, 0, (LPARAM)L"720p");
    SendMessage(resSelect, CB_ADDSTRING, 0, (LPARAM)L"1080p");
    SendMessage(resSelect, CB_SETCURSEL, 1, 0); // selects 720p on startup by default

    // Encoder (not final)
    encSelect = CreateWindowW(L"BUTTON", L"h264",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        10, 90, 100, 30, settingsBox, NULL, hInstance, NULL);

    if (!mainWnd)
    {
        return FALSE;
    }

    ShowWindow(mainWnd, nCmdShow);
    UpdateWindow(mainWnd);

    return TRUE;
}



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
            OPENFILENAME ofn;
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
            WCHAR outputFileName[260];
            GetWindowText(GetDlgItem(hWnd, IDC_EDIT_OUTPUT_FILENAME), outputFileName, 260);

            WCHAR outputFilePath[260];
            wcscpy_s(outputFilePath, g_selectedFilePath);
            PathRemoveFileSpec(outputFilePath);
            PathAppend(outputFilePath, outputFileName);

            // Check if the file already exists, overwrite warning
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

            // Retrieve the selected resolution
            int resIndex = SendMessage(resSelect, CB_GETCURSEL, 0, 0);
            WCHAR resolution[10];
            SendMessage(resSelect, CB_GETLBTEXT, resIndex, (LPARAM)resolution);

            // Debugging messages to confirm file paths
            OutputDebugString(L"Input File: ");
            OutputDebugString(g_selectedFilePath);
            OutputDebugString(L"\n");
            OutputDebugString(L"Output File: ");
            OutputDebugString(outputFilePath);
            OutputDebugString(L"\n");
            OutputDebugString(L"Starting compression process\n");

            ProcessVideo(inputFile, outputFile, hWnd, resolution);
        }
        break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
   case WM_DROPFILES:
   {
       HDROP fileDrop = (HDROP)wParam;
       WCHAR selFile[260];
       DragQueryFile(fileDrop, 0, selFile, 260);
       DragFinish(fileDrop);

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
   break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        SetBkMode(hdc, TRANSPARENT);
        //SetBkColor(hdc, DARK_BACKGROUND);
       // SetTextColor(hdc, DARK_TEXT); // not in use until fix dark mode

        const wchar_t* text1 = L"File to be compressed:";
        TextOut(hdc, 10, 20, text1, wcslen(text1));
        const wchar_t* text2 = L"Output file name:";
        TextOut(hdc, 10, 100, text2, wcslen(text2));
        const wchar_t* text3 = L"Target Size:        MB";
        TextOut(hdc, 300, 30, text3, wcslen(text3));
        const wchar_t* text4 = L"Resolution:";
        TextOut(hdc, 300, 60, text4, wcslen(text4));

        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Updating Progress Bar Function
void UpdateProgress(HWND hWnd, int progress, bool& isCompressionInProgress) {
    HWND progressBar = GetDlgItem(hWnd, IDC_PROGRESS_BAR);

    // Show progress bar when compression starts
    if (!isCompressionInProgress && progress > 0) {
        ShowWindow(progressBar, SW_SHOW);
        isCompressionInProgress = true;    //  Compression is in progress
    }

    // Hide progress bar when compression finishes
    if (progress == 100) {
        ShowWindow(progressBar, SW_HIDE);
        isCompressionInProgress = false;   // Reset bool after compression finishes
    }
    SendMessage(progressBar, PBM_SETPOS, (WPARAM)progress, 0);    // Update progress bar during compression
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
