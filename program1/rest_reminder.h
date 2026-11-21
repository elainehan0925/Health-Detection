// Show silent Windows rest notifications without blocking camera processing or opening the camera preview.
#pragma once
#include "rest_policy.h"
#include <cmath>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

struct RestReminder {
    double nextAllowedAt = 0;
    double nextAttemptAt = 0;
#ifdef _WIN32
    HWND window = nullptr;
    NOTIFYICONDATAW icon{};

    ~RestReminder() {
        if (window) {
            Shell_NotifyIconW(NIM_DELETE, &icon);
            DestroyWindow(window);
        }
    }

    bool show(double missing) {
        if (!window) {
            window = CreateWindowExW(0, L"STATIC", L"Eye Monitor notifications", 0,
                                     0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
            if (!window) return false;
            icon.cbSize = sizeof(icon);
            icon.hWnd = window;
            icon.uID = 1;
            icon.uFlags = NIF_ICON | NIF_TIP;
            icon.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
            lstrcpynW(icon.szTip, L"Eye Monitor", 128);
            if (!Shell_NotifyIconW(NIM_ADD, &icon)) {
                DestroyWindow(window);
                window = nullptr;
                return false;
            }
        }
        std::wstring message = L"Recorded rest in the last 20 minutes is below 60 seconds. Consider a break of at least " +
                               std::to_wstring(static_cast<int>(std::ceil(missing))) + L" more seconds.";
        icon.uFlags = NIF_INFO | NIF_REALTIME;
        icon.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
        lstrcpynW(icon.szInfoTitle, L"Eye Monitor - rest reminder", 64);
        lstrcpynW(icon.szInfo, message.c_str(), 256);
        return Shell_NotifyIconW(NIM_MODIFY, &icon) != 0;
    }
#else
    bool show(double) { return false; }
#endif

    bool shouldNotify(double now, bool due) const {
        return due && now >= nextAllowedAt && now >= nextAttemptAt;
    }

    bool update(double now, const RestPolicy::View& policy) {
#ifdef _WIN32
        if (window) {
            MSG message;
            while (PeekMessageW(&message, window, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
#endif
        if (!shouldNotify(now, policy.due)) return true;
        if (!show(policy.missing)) {
            nextAttemptAt = now + 60;
            return false;
        }
        nextAllowedAt = now + 1200;
        return true;
    }

    void reset() {
        nextAllowedAt = 0;
        nextAttemptAt = 0;
    }
};
