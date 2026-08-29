#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <winhttp.h>

#include <string>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define IDC_CHECKID 1001
#define IDC_START   1002
#define IDC_REPORT  1003
#define IDC_STATUS  1004
#define IDC_PROGRESS 1005

static HWND hCheckId = nullptr;
static HWND hStatus = nullptr;
static HWND hProgress = nullptr;
static HWND hReport = nullptr;
static std::wstring reportUrl;

static std::wstring Trim(const std::wstring& s) {
    const size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    const size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

static bool ValidCheckId(const std::wstring& id) {
    return id.size() >= 8 &&
           id.rfind(L"FAC-", 0) == 0;
}

static void SetStatus(const wchar_t* text) {
    SetWindowTextW(hStatus, text);
}

static DWORD WINAPI CheckThread(LPVOID) {
    wchar_t buffer[256]{};
    GetWindowTextW(hCheckId, buffer, 256);
    std::wstring checkId = Trim(buffer);

    if (!ValidCheckId(checkId)) {
        MessageBoxW(nullptr,
            L"Introdu un Check ID valid, de forma FAC-XXXXXXXXXX.",
            L"FARALAG AC",
            MB_ICONWARNING | MB_OK);
        EnableWindow(GetDlgItem(GetParent(hCheckId), IDC_START), TRUE);
        return 0;
    }

    SetStatus(L"Connecting to FARALAG AC...");
    SendMessageW(hProgress, PBM_SETPOS, 25, 0);
    Sleep(350);

    HINTERNET session = WinHttpOpen(
        L"FARALAG-AC-GUI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    bool online = false;

    if (session) {
        HINTERNET connect = WinHttpConnect(
            session, L"faralag.ro",
            INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (connect) {
            HINTERNET request = WinHttpOpenRequest(
                connect, L"GET",
                L"/ac/api/health.php",
                nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);

            if (request) {
                if (WinHttpSendRequest(
                        request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                        nullptr, 0, 0, 0)) {
                    online = WinHttpReceiveResponse(request, nullptr) == TRUE;
                }
                WinHttpCloseHandle(request);
            }
            WinHttpCloseHandle(connect);
        }
        WinHttpCloseHandle(session);
    }

    SendMessageW(hProgress, PBM_SETPOS, 75, 0);

    if (online) {
        reportUrl = L"https://faralag.ro/ac/check/?id=" + checkId;
        SetStatus(L"Connection OK. Ready to scan.");
        EnableWindow(hReport, TRUE);
        MessageBoxW(nullptr,
            L"Conexiunea cu FARALAG AC este OK.\n\n"
            L"Motorul de scanare va fi adaugat in urmatoarea etapa.",
            L"FARALAG AC",
            MB_ICONINFORMATION | MB_OK);
    } else {
        SetStatus(L"Could not connect to FARALAG AC.");
        MessageBoxW(nullptr,
            L"Nu am putut contacta faralag.ro.\nVerifica conexiunea la internet.",
            L"FARALAG AC",
            MB_ICONERROR | MB_OK);
    }

    SendMessageW(hProgress, PBM_SETPOS, 100, 0);
    EnableWindow(GetDlgItem(GetParent(hCheckId), IDC_START), TRUE);
    return 0;
}

static void OpenReport() {
    if (!reportUrl.empty())
        ShellExecuteW(nullptr, L"open", reportUrl.c_str(),
                      nullptr, nullptr, SW_SHOWNORMAL);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND title = CreateWindowW(
            L"STATIC", L"FARALAG AC",
            WS_CHILD | WS_VISIBLE,
            30, 25, 400, 35,
            hwnd, nullptr, nullptr, nullptr);
        SendMessageW(title, WM_SETFONT, (WPARAM)font, TRUE);

        HWND sub = CreateWindowW(
            L"STATIC", L"Anti-Cheat Scanner",
            WS_CHILD | WS_VISIBLE,
            32, 60, 300, 22,
            hwnd, nullptr, nullptr, nullptr);
        SendMessageW(sub, WM_SETFONT, (WPARAM)font, TRUE);

        CreateWindowW(
            L"STATIC", L"Check ID",
            WS_CHILD | WS_VISIBLE,
            32, 105, 120, 22,
            hwnd, nullptr, nullptr, nullptr);

        hCheckId = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            32, 130, 396, 34,
            hwnd, (HMENU)IDC_CHECKID, nullptr, nullptr);

        hProgress = CreateWindowExW(
            0, PROGRESS_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE,
            32, 185, 396, 20,
            hwnd, (HMENU)IDC_PROGRESS, nullptr, nullptr);

        hStatus = CreateWindowW(
            L"STATIC", L"Ready.",
            WS_CHILD | WS_VISIBLE,
            32, 220, 396, 25,
            hwnd, (HMENU)IDC_STATUS, nullptr, nullptr);

        CreateWindowW(
            L"BUTTON", L"START SCAN",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            32, 265, 190, 42,
            hwnd, (HMENU)IDC_START, nullptr, nullptr);

        hReport = CreateWindowW(
            L"BUTTON", L"VIEW ONLINE REPORT",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
            238, 265, 190, 42,
            hwnd, (HMENU)IDC_REPORT, nullptr, nullptr);

        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_START:
            EnableWindow(GetDlgItem(hwnd, IDC_START), FALSE);
            SetStatus(L"Starting...");
            CreateThread(nullptr, 0, CheckThread, nullptr, 0, nullptr);
            return 0;

        case IDC_REPORT:
            OpenReport();
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    const wchar_t CLASS_NAME[] = L"FARALAG_AC_GUI_V1";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"FARALAG AC",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        480, 365,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
