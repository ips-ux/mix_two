#ifdef USE_BENCH
#include <benchmark/benchmark.h>
#endif

#ifdef _WIN32
#include <ole2.h>
#endif

#include "errordialoghandler.h"
#include "mixxxtest.h"
#include "util/logging.h"

namespace {

#ifdef _WIN32
// The Qt "windows" platform plugin keeps COM initialized on the main thread
// for the lifetime of the application, but the "offscreen" plugin used by the
// tests does not. Without it, the last CoUninitialize() of a component such as
// PortAudio's ASIO host lets COM unload DLLs like Windows.Globalization.dll
// that Qt still references through cached WinRT factories (QLocale), which
// crashes the next test that creates a CoreServices instance.
class ScopedOleInitialize final {
  public:
    ScopedOleInitialize()
            : m_hr(OleInitialize(nullptr)) {
    }
    ~ScopedOleInitialize() {
        if (SUCCEEDED(m_hr)) {
            OleUninitialize();
        }
    }

  private:
    const HRESULT m_hr;
};
#endif

} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
    ScopedOleInitialize oleInitialize;
#endif

    // By default, render analyzer waveform tests to an offscreen buffer
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    }

    // Tests must not load the installed audio drivers; see
    // PortAudioEnumerator::initialize().
    if (qEnvironmentVariableIsEmpty("MIXXX_DISABLE_PORTAUDIO")) {
        qputenv("MIXXX_DISABLE_PORTAUDIO", QByteArray("1"));
    }

    // We never want to popup error dialogs when running tests.
    ErrorDialogHandler::setEnabled(false);

#ifdef USE_BENCH
    bool run_benchmarks = false;
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--benchmark") == 0) {
            run_benchmarks = true;
            break;
        } else if (strcmp(argv[i], "--trace") == 0) {
            mixxx::Logging::setLogLevel(mixxx::LogLevel::Trace);
        }
    }

    if (run_benchmarks) {
        benchmark::Initialize(&argc, argv);
        MixxxTest::ApplicationScope applicationScope(argc, argv);
        benchmark::RunSpecifiedBenchmarks();
        return 0;
    }

    // Otherwise, run the test suite:
#endif
    testing::InitGoogleTest(&argc, argv);
    MixxxTest::ApplicationScope applicationScope(argc, argv);
    return RUN_ALL_TESTS();
}
