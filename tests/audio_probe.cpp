#include "AudioEngine.hpp"
#include <iostream>
#include <chrono>
#include <thread>
// Manual integration probe. No route starts without explicit indexes.
int wmain(int argc, wchar_t **argv) {
    HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(co))
        return 1;
    int result = 0;
    {
        auto devices = audioduplo::EnumerateRenderDevices();
        for (size_t i = 0; i < devices.size(); ++i)
            std::wcout << i << L": " << devices[i].name << (devices[i].isDefault ? L" [default]" : L"")
                       << L'\n';
        if (argc == 3) {
            int s = _wtoi(argv[1]), d = _wtoi(argv[2]);
            if (s < 0 || d < 0 || s >= static_cast<int>(devices.size()) ||
                d >= static_cast<int>(devices.size()))
                result = 2;
            else {
                audioduplo::AudioEngine e;
                if (!e.start(devices[s].id, devices[d].id, audioduplo::LatencyChoice::Automatic))
                    result = 3;
                else {
                    for (int i = 0; i < 12; ++i) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        auto st = e.snapshot();
                        std::wcout << L"ready=" << st.ready << L" capture=" << st.capturedFrames
                                   << L" render=" << st.renderedFrames << L" buffer=" << st.bufferMs
                                   << L" underruns=" << st.underruns << L" error=" << st.lastError << L'\n';
                        if (!st.running) {
                            result = 4;
                            break;
                        }
                    }
                    if (!e.snapshot().ready)
                        result = 5;
                    e.stop();
                }
            }
        }
    }
    CoUninitialize();
    return result;
}
