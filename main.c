#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdbool.h>

// Explicitly use Unicode version
#ifndef UNICODE
#define UNICODE
#endif

// Constants
#define TRAY_ICON_ID 1
#define WM_TRAYICON (WM_USER + 1)

// Menu item IDs
#define ID_TRAY_SYNC 1001
#define ID_TRAY_SETTINGS 1002
#define ID_TRAY_ABOUT 1003
#define ID_TRAY_EXIT 1004

// Global variables
HWND g_hwnd;
NOTIFYICONDATAW g_nid;  // Use NOTIFYICONDATAW instead of NOTIFYICONDATA
WCHAR g_oneDrivePath[MAX_PATH] = L"";
WCHAR g_iniFilePath[MAX_PATH] = L"";

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitTrayIcon(HWND hwnd);
void ShowContextMenu(HWND hwnd, POINT pt);
void SyncOneDrive();
void ShowSettings();
void ShowAbout();
void LoadSettings();
void SaveSettings();

// Main application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OneDriveSyncHelperClass";
    
    RegisterClassW(&wc);
    
    // Create a hidden window
    g_hwnd = CreateWindowExW(
        0, L"OneDriveSyncHelperClass", L"OneDrive Sync Helper",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300, NULL, NULL, hInstance, NULL
    );
    
    if (!g_hwnd) {
        MessageBoxW(NULL, L"Window creation failed", L"Error", MB_ICONERROR);
        return 1;
    }
    
    // Initialize settings path
    GetEnvironmentVariableW(L"APPDATA", g_iniFilePath, MAX_PATH);
    wcscat(g_iniFilePath, L"\\OneDriveSyncHelper");
    
    // Create directory if it doesn't exist
    CreateDirectoryW(g_iniFilePath, NULL);
    
    // Complete the path to the INI file
    wcscat(g_iniFilePath, L"\\settings.ini");
    
    // Load settings
    LoadSettings();
    
    // Initialize system tray icon
    InitTrayIcon(g_hwnd);
    
    // Show system tray icon
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    
    // Show balloon tip on startup
    g_nid.uFlags |= NIF_INFO;
    wcscpy(g_nid.szInfoTitle, L"OneDrive Sync Helper");
    wcscpy(g_nid.szInfo, L"Application is running in the background");
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 3000;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Remove tray icon before exit
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    
    return (int)msg.wParam;
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                // Get cursor position
                POINT pt;
                GetCursorPos(&pt);
                
                // Show context menu
                ShowContextMenu(hwnd, pt);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                // Double-click, trigger sync
                SyncOneDrive();
            }
            return 0;
            
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_TRAY_SYNC:
                    SyncOneDrive();
                    break;
                case ID_TRAY_SETTINGS:
                    ShowSettings();
                    break;
                case ID_TRAY_ABOUT:
                    ShowAbout();
                    break;
                case ID_TRAY_EXIT:
                    DestroyWindow(hwnd);
                    break;
            }
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Initialize system tray icon
void InitTrayIcon(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ICON_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    
    // Load icon - we'll use a system icon for simplicity
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    
    // Set tooltip
    wcscpy(g_nid.szTip, L"OneDrive Sync Helper");
}

// Show context menu
void ShowContextMenu(HWND hwnd, POINT pt) {
    HMENU hMenu = CreatePopupMenu();
    
    // Add menu items - using InsertMenuW for Unicode
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_SYNC, L"Sync OneDrive");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, L"Settings");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_ABOUT, L"About");
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");
    
    // Display the menu
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

// Sync OneDrive by creating and deleting a trigger file
void SyncOneDrive() {
    // Check if OneDrive path is set
    if (wcslen(g_oneDrivePath) == 0) {
        MessageBoxW(NULL, L"OneDrive path not set. Please configure in settings.", L"Sync Error", MB_ICONERROR);
        ShowSettings();
        return;
    }
    
    // Check if path exists
    DWORD dwAttrib = GetFileAttributesW(g_oneDrivePath);
    if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(NULL, L"OneDrive folder does not exist", L"Sync Error", MB_ICONERROR);
        return;
    }
    
    // Create trigger file path
    WCHAR triggerFilePath[MAX_PATH];
    wcscpy(triggerFilePath, g_oneDrivePath);
    wcscat(triggerFilePath, L"\\.forsync");
    
    // Show balloon notification
    g_nid.uFlags |= NIF_INFO;
    wcscpy(g_nid.szInfoTitle, L"OneDrive Sync Helper");
    wcscpy(g_nid.szInfo, L"Triggering OneDrive sync...");
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 2000;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    
    // Create empty file
    HANDLE hFile = CreateFileW(
        triggerFilePath, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(NULL, L"Cannot create trigger file", L"Sync Error", MB_ICONERROR);
        return;
    }
    
    CloseHandle(hFile);
    
    // Wait a short time
    Sleep(500);
    
    // Delete the file
    if (!DeleteFileW(triggerFilePath)) {
        MessageBoxW(NULL, L"Cannot delete trigger file", L"Sync Error", MB_ICONERROR);
        return;
    }
    
    // Show success notification
    g_nid.uFlags |= NIF_INFO;
    wcscpy(g_nid.szInfoTitle, L"OneDrive Sync Helper");
    wcscpy(g_nid.szInfo, L"Sync triggered successfully!");
    g_nid.dwInfoFlags = NIIF_INFO;
    g_nid.uTimeout = 3000;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// Show settings dialog - simpler version without TASKDIALOGCONFIG
void ShowSettings() {
    // Simplified version using standard dialog
    MessageBoxW(NULL, L"Please select your OneDrive folder in the next dialog", L"OneDrive Settings", MB_OK | MB_ICONINFORMATION);
    
    // Create a folder browser dialog
    BROWSEINFOW bi = {0};
    bi.hwndOwner = g_hwnd;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = NULL;  // Not using this parameter, can cause errors
    bi.lpszTitle = L"Please select your OneDrive folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl != NULL) {
        // Get the selected folder's path
        SHGetPathFromIDListW(pidl, g_oneDrivePath);
        
        // Save settings
        SaveSettings();
        
        // Show confirmation
        g_nid.uFlags |= NIF_INFO;
        wcscpy(g_nid.szInfoTitle, L"OneDrive Sync Helper");
        wcscpy(g_nid.szInfo, L"Settings saved");
        g_nid.dwInfoFlags = NIIF_INFO;
        g_nid.uTimeout = 2000;
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
        
        // Free memory
        CoTaskMemFree(pidl);
    }
}

// Show about dialog
void ShowAbout() {
    MessageBoxW(
        NULL,
        L"OneDrive Sync Helper v1.0\n\n"
        L"A simple tool to trigger OneDrive synchronization.\n"
        L"Forces OneDrive to scan for changes by creating and deleting a temporary file.",
        L"About OneDrive Sync Helper",
        MB_OK | MB_ICONINFORMATION
    );
}

// Load settings from INI file
void LoadSettings() {
    GetPrivateProfileStringW(L"Settings", L"OneDrivePath", L"", g_oneDrivePath, MAX_PATH, g_iniFilePath);
    
    // If path is empty, try to auto-detect
    if (wcslen(g_oneDrivePath) == 0) {
        WCHAR userProfile[MAX_PATH];
        GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH);
        
        // Common OneDrive paths
        WCHAR possiblePaths[3][MAX_PATH];
        wcscpy(possiblePaths[0], userProfile);
        wcscat(possiblePaths[0], L"\\OneDrive");
        
        wcscpy(possiblePaths[1], userProfile);
        wcscat(possiblePaths[1], L"\\OneDrive - Personal");
        
        wcscpy(possiblePaths[2], userProfile);
        wcscat(possiblePaths[2], L"\\OneDrive - Business");
        
        // Check each path
        for (int i = 0; i < 3; i++) {
            DWORD dwAttrib = GetFileAttributesW(possiblePaths[i]);
            if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                wcscpy(g_oneDrivePath, possiblePaths[i]);
                break;
            }
        }
    }
}

// Save settings to INI file
void SaveSettings() {
    WritePrivateProfileStringW(L"Settings", L"OneDrivePath", g_oneDrivePath, g_iniFilePath);
}