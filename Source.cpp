#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <cstdio>
#include <CommCtrl.h>

#define LIGNES 24
#define COLLONES 80
#define WM_APP_RXDATA 0x8001
#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "gdi32.lib") 
#pragma comment(lib, "Comctl32.lib") 

#pragma warning(disable:4996)
#pragma warning(disable:6387)
struct Cellules {
    char ch;
    COLORREF fg;
    COLORREF bg;
    bool bold;
};
struct Ecran {
    Cellules buf[LIGNES][COLLONES];
    int row = 0, col = 0;
    COLORREF curFg = RGB(255, 255, 255);
    COLORREF curBg = RGB(0, 0, 0);
    bool curBold = false;
    Ecran() { Purger(); }
    void Purger() {
        for (int r = 0; r < LIGNES; r++)
            for (int c = 0; c < COLLONES; c++) {
                buf[r][c].ch = ' ';
                buf[r][c].fg = curFg;
                buf[r][c].bg = curBg;
                buf[r][c].bold = false;
            }
        row = col = 0;
    }
    void EcrireCaractere(char ch) {
        if (row >= 0 && row < LIGNES && col >= 0 && col < COLLONES) {
            buf[row][col].ch = ch;
            buf[row][col].fg = curFg;
            buf[row][col].bg = curBg;
            buf[row][col].bold = curBold;
        }
        col++;
        if (col >= COLLONES) { col = 0; row++; }
        if (row >= LIGNES) { row = LIGNES - 1; }
    }
    void PositionnerCurseur(int r, int c) {
        row = (r >= 0 && r < LIGNES) ? r : 0;
        col = (c >= 0 && c < COLLONES) ? c : 0;
    }
    void EffacerLigne(int r, int fromCol) {
        for (int c = fromCol; c < COLLONES; c++) {
            buf[r][c].ch = ' ';
            buf[r][c].fg = curFg;
            buf[r][c].bg = curBg;
            buf[r][c].bold = false;
        }
    }
    void Attribution(const std::string& params) {
        if (params.empty()) { curFg = RGB(255, 255, 255); curBg = RGB(0, 0, 0); curBold = false; return; }
        std::stringstream ss(params);
        std::string tok;
        while (std::getline(ss, tok, ';')) {
            int code = atoi(tok.c_str());
            if (code == 0) { curFg = RGB(255, 255, 255); curBg = RGB(0, 0, 0); curBold = false; }
            else if (code == 1) { curBold = true; }
            else if (code >= 30 && code <= 37) {
                static COLORREF colors[8] = { RGB(0,0,0),RGB(255,0,0),RGB(0,255,0),RGB(255,255,0),
                                           RGB(0,0,255),RGB(255,0,255),RGB(0,255,255),RGB(255,255,255) };
                curFg = colors[code - 30];
            }
            else if (code >= 40 && code <= 47) {
                static COLORREF colors[8] = { RGB(0,0,0),RGB(255,0,0),RGB(0,255,0),RGB(255,255,0),
                                           RGB(0,0,255),RGB(255,0,255),RGB(0,255,255),RGB(255,255,255) };
                curBg = colors[code - 40];
            }
        }
    }
};
struct AppState { 
    HWND hwnd = nullptr; 
    HWND hEdit = nullptr; 
    HANDLE hCom = INVALID_HANDLE_VALUE; 
    HANDLE hRxThread = nullptr; 
    volatile bool running = false; 
};

Ecran gScreen;
HWND gHwnd;
HANDLE gCom = INVALID_HANDLE_VALUE;
HANDLE gRx = nullptr;
volatile bool gRunning = false;
AppState g;
char MasqueBoot[] = {};
HANDLE OuvrirPortCom(const std::wstring& portName, DWORD baud) {
    HANDLE h = CreateFileW(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) return h;
    DCB dcb{}; dcb.DCBlength = sizeof(DCB);
    GetCommState(h, &dcb);
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutX = FALSE; dcb.fInX = FALSE;
    SetCommState(h, &dcb);
    COMMTIMEOUTS to{};
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutMultiplier = 10;
    to.ReadTotalTimeoutConstant = 50;
    to.WriteTotalTimeoutMultiplier = 10;
    to.WriteTotalTimeoutConstant = 50;
    SetCommTimeouts(h, &to);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return h;
}

bool EnvoyerPortCom(HANDLE h, const void* data, DWORD len) {
    OVERLAPPED ov{}; ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, len, &written, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(ov.hEvent, INFINITE);
        GetOverlappedResult(h, &ov, &written, FALSE);
        ok = TRUE;
    }
    CloseHandle(ov.hEvent);
    return ok && (written == len);
}

DWORD WINAPI ProcedureRX(LPVOID) {
    std::vector<char> buf(1024);
    while (gRunning) {
        OVERLAPPED ov{}; ov.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        DWORD read = 0;
        BOOL ok = ReadFile(gCom, buf.data(), (DWORD)buf.size(), &read, &ov);
        if (!ok && GetLastError() == ERROR_IO_PENDING) {
            DWORD w = WaitForSingleObject(ov.hEvent, 200);
            if (w == WAIT_OBJECT_0) GetOverlappedResult(gCom, &ov, &read, FALSE);
            else { CancelIo(gCom); read = 0; }
        }
        CloseHandle(ov.hEvent);
        if (read > 0) {
            std::string* payload = new std::string(buf.data(), read);
            PostMessage(gHwnd, WM_APP_RXDATA, 0, (LPARAM)payload);
        }
    }
    return 0;
}

void EmulerVT100(const std::string& data) {
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] == 0x1B && i + 1 < data.size() && data[i + 1] == '[') {
            size_t j = i + 2;
            std::string params;
            while (j < data.size() && (isdigit(data[j]) || data[j] == ';')) { params.push_back(data[j]); j++; }
            if (j >= data.size()) break;
            char cmd = data[j];
            if (cmd == 'H') {
                int r = 0, c = 0; sscanf(params.c_str(), "%d;%d", &r, &c);
                gScreen.PositionnerCurseur(r - 1, c - 1);
            }
            else if (cmd == 'J') {
                gScreen.Purger();
            }
            else if (cmd == 'K') {
                gScreen.EffacerLigne(gScreen.row, gScreen.col);
            }
            else if (cmd == 'm') {
                gScreen.Attribution(params);
            }
            i = j;
        }
        else {
            gScreen.EcrireCaractere(data[i]);
        }
    }
    InvalidateRect(gHwnd, NULL, TRUE);
}

void Afficher(HDC hdc) {
    HFONT hFont = CreateFont(16, 8, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, L"Consolas");
    HFONT old = (HFONT)SelectObject(hdc, hFont);
    for (int r = 0; r < LIGNES; r++) {
        for (int c = 0; c < COLLONES; c++) {
            RECT rc = { c * 8,r * 16,(c + 1) * 8,(r + 1) * 16 };
            SetBkColor(hdc, gScreen.buf[r][c].bg);
            SetTextColor(hdc, gScreen.buf[r][c].fg);
            DrawTextA(hdc, &gScreen.buf[r][c].ch, 1, &rc, DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
        }
    }
    SelectObject(hdc, old);
    DeleteObject(hFont);
}
void appendText(HWND hEdit, const std::string& s) { int len = GetWindowTextLengthA(hEdit); SendMessageA(hEdit, EM_SETSEL, len, len); SendMessageA(hEdit, EM_REPLACESEL, FALSE, (LPARAM)s.c_str()); }
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        gHwnd = hwnd;
        g.hEdit = CreateWindowExA(WS_EX_CLIENTEDGE|WS_EX_DLGMODALFRAME, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 0, 0, 0, 0, hwnd, (HMENU)0x1FE, GetModuleHandle(nullptr), nullptr);
        HFONT hFont = (HFONT)GetStockObject(ANSI_FIXED_FONT); 
        SendMessage(g.hEdit, WM_SETFONT, (WPARAM)hFont, TRUE); 
        CreateStatusWindow(WS_CHILD | WS_VISIBLE, info, hwnd, 0x500);
        return 0;
    }
    case WM_SIZE: { 
        RECT rc; 
        GetClientRect(hwnd, &rc); 
        MoveWindow(g.hEdit, 0, 0, rc.right, rc.bottom - 40, TRUE); 
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        Afficher(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_APP_RXDATA: {
        std::string* payload = (std::string*)lParam; 
        EmulerVT100(*payload); delete payload; 
        return 0; 
    }
    case WM_CHAR: { 
        if (g.hCom != INVALID_HANDLE_VALUE) { 
            char c = (char)wParam; 
            if (c == '\r') { 
                const char* crlf = "\r\n"; 
                EnvoyerPortCom(g.hCom, crlf, 2); 
            } else { 
                EnvoyerPortCom(g.hCom, &c, 1); 
            } 
        }
        return 0;   
    }
    case WM_DESTROY: PostQuitMessage(0); return 0; 
    } 
    return DefWindowProc(hwnd,msg,wParam,lParam);
}
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) { 
    WNDCLASS wc{}; 
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst; 
    wc.hbrBackground = CreateSolidBrush(RGB(0xCC, 0xCC, 0x00));
    wc.lpszClassName = L"LCR2-VT100"; 
    wc.hIcon = LoadIcon(wc.hInstance,(LPCTSTR)0x65);
    RegisterClass(&wc); 
    HWND hwnd = CreateWindow(wc.lpszClassName, L"Terminal LCR2 VT-100", WS_OVERLAPPED |WS_CAPTION |WS_SYSMENU |WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 0x280, 0x1C0, nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd); 
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) { 
        TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    } 
    return 0; 
}