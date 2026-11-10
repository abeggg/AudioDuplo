#pragma once
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <string>

namespace audioduplo {
struct Preferences {
    std::wstring sourceId, destinationId;
    int latencyIndex{0}, volume{100};
    static std::wstring path() {
        PWSTR root = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &root)))
            return {};
        std::wstring folder = std::wstring(root) + L"\\AudioDuplo";
        CoTaskMemFree(root);
        if (!CreateDirectoryW(folder.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
            return {};
        return folder + L"\\settings.ini";
    }
    void load() {
        const auto file = path();
        if (file.empty())
            return;
        wchar_t buffer[2048]{};
        GetPrivateProfileStringW(L"Audio", L"Source", L"", buffer, 2048, file.c_str());
        sourceId = buffer;
        GetPrivateProfileStringW(L"Audio", L"Destination", L"", buffer, 2048, file.c_str());
        destinationId = buffer;
        latencyIndex =
            std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Audio", L"Buffer", 0, file.c_str())), 0, 7);
        volume = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Audio", L"Volume", 100, file.c_str())),
                            0, 100);
    }
    bool save() const {
        const auto file = path();
        if (file.empty())
            return false;
        // Seed UTF-16 so endpoint identifiers remain lossless with the Windows INI API.
        HANDLE f = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            WORD bom = 0xfeff;
            DWORD written = 0;
            WriteFile(f, &bom, sizeof(bom), &written, nullptr);
            CloseHandle(f);
        }
        bool ok = WritePrivateProfileStringW(L"Audio", L"Source", sourceId.c_str(), file.c_str()) != FALSE;
        ok = WritePrivateProfileStringW(L"Audio", L"Destination", destinationId.c_str(), file.c_str()) && ok;
        ok = WritePrivateProfileStringW(L"Audio", L"Buffer", std::to_wstring(latencyIndex).c_str(),
                                        file.c_str()) &&
             ok;
        return WritePrivateProfileStringW(L"Audio", L"Volume", std::to_wstring(volume).c_str(),
                                          file.c_str()) &&
               ok;
    }
};
} // namespace audioduplo
