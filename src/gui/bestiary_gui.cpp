#include<wx/wx.h>
#include<wx/clipbrd.h>
#include<wx/base64.h>
#include<wx/config.h>
#include<wx/dcbuffer.h>
#include<wx/filename.h>
#include<wx/filedlg.h>
#include<wx/graphics.h>
#include<wx/hyperlink.h>
#include<wx/mstream.h>
#include<wx/scrolwin.h>
#include<wx/simplebook.h>
#include<wx/stdpaths.h>
#include<wx/textdlg.h>
#include<wx/timer.h>
#include<wx/vector.h>
#include<algorithm>
#include<cmath>
#include<cstdio>
#include<cstdlib>
#include<cstdint>
#include<deque>
#include<memory>
#include<set>
#include<string>
#include<vector>

#include"help_page_content.h"
#include"embedded_banner.h"
#include"embedded_icon.h"

extern "C" {
#include"neko.h"
}

#ifdef __WXMSW__
#  define WIN32_LEAN_AND_MEAN
#  include<windows.h>
#  include<uxtheme.h>
#else
#  include<errno.h>
#  include<fcntl.h>
#  include<pty.h>
#  include<signal.h>
#  include<sys/ioctl.h>
#  include<sys/wait.h>
#  include<termios.h>
#  include<unistd.h>
#endif

namespace {

static wxColour ColourFromRgb(uint32_t rgb) {
    return wxColour((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

namespace theme {
static constexpr uint32_t kFrameBg       = 0x0D1117u;
static constexpr uint32_t kPanelBg       = 0x161B22u;
static constexpr uint32_t kPanelRaisedBg = 0x1D2430u;
static constexpr uint32_t kTabBg         = 0x11161Du;
static constexpr uint32_t kTabActiveBg   = 0x1B2430u;
static constexpr uint32_t kButtonBg      = 0x202938u;
static constexpr uint32_t kButtonAltBg   = 0x273347u;
static constexpr uint32_t kBorder        = 0x2D3748u;
static constexpr uint32_t kAccent        = 0x56B6C2u;
static constexpr uint32_t kAccentStrong  = 0x7AD7E0u;
static constexpr uint32_t kText          = 0xE6EDF3u;
static constexpr uint32_t kMutedText     = 0x9BA7B4u;
static constexpr uint32_t kTerminalBg    = 0x0B0F14u;
static constexpr uint32_t kTerminalText  = 0xD7DEE6u;
static constexpr uint32_t kTerminalCursor = 0x8BD5FFu;
static constexpr uint32_t kSelectionBg   = 0x23435Fu;
static constexpr uint32_t kSearchHighlightBg = 0x7AD7E0u;
static constexpr uint32_t kSearchHighlightText = 0x0B0F14u;
}

static wxFont BestiaryTerminalFont() {
    wxFont font(wxFontInfo(11).Family(wxFONTFAMILY_TELETYPE));
#ifdef __WXMSW__
    font.SetFaceName("Consolas");
#elif defined(__WXGTK__)
    font.SetFaceName("DejaVu Sans Mono");
#endif
    return font;
}

static void StyleDarkWindow(wxWindow* window, uint32_t bgRgb) {
    if (!window) return;
    window->SetBackgroundColour(ColourFromRgb(bgRgb));
    window->SetForegroundColour(ColourFromRgb(theme::kText));
}

static void StyleDarkLabel(wxStaticText* label, bool muted = false) {
    if (!label) return;
    label->SetForegroundColour(ColourFromRgb(muted ? theme::kMutedText : theme::kText));
}

static void StyleDarkButton(wxButton* button, bool accent = false) {
    if (!button) return;
    button->SetBackgroundColour(ColourFromRgb(accent ? theme::kButtonAltBg : theme::kButtonBg));
    button->SetForegroundColour(ColourFromRgb(theme::kText));
#ifdef __WXMSW__
    // Strip the native Windows visual style from this button so the OS doesn't
    // repaint a white themed background on hover/focus over our dark colours
    // (which left light text unreadable on a light background).
    HWND hwnd = (HWND)button->GetHandle();
    if (hwnd) ::SetWindowTheme(hwnd, L"", L"");
#endif
}

static void StyleDarkHyperlink(wxHyperlinkCtrl* link) {
    if (!link) return;
    link->SetBackgroundColour(ColourFromRgb(theme::kPanelBg));
    link->SetForegroundColour(ColourFromRgb(theme::kAccentStrong));
    link->SetNormalColour(ColourFromRgb(theme::kAccentStrong));
    link->SetHoverColour(ColourFromRgb(theme::kAccent));
    link->SetVisitedColour(ColourFromRgb(theme::kMutedText));
}

static void StyleDarkTextCtrl(wxTextCtrl* text) {
    if (!text) return;
    text->SetBackgroundColour(ColourFromRgb(theme::kPanelBg));
    text->SetForegroundColour(ColourFromRgb(theme::kText));
}

static wxString BestiaryExecutablePath() {
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    wxFileName bestiary(exe.GetPath(), "bestiary-cli");
#ifdef __WXMSW__
    bestiary.SetExt("exe");
#endif
    return bestiary.GetFullPath();
}

static bool EnsureWritableDirectory(const wxString& dir) {
    if (dir.empty()) return false;
    if (wxFileName::DirExists(dir)) return true;

    wxLogNull quiet;
    return wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)
        || wxFileName::DirExists(dir);
}

static wxString BestiarySessionDirectory() {
    wxString dir = wxStandardPaths::Get().GetUserLocalDataDir();
    if (EnsureWritableDirectory(dir)) return dir;

    dir = wxStandardPaths::Get().GetUserDataDir();
    if (EnsureWritableDirectory(dir)) return dir;

    dir = wxFileName(wxFileName::GetTempDir(), "Bestiary").GetFullPath();
    if (EnsureWritableDirectory(dir)) return dir;

    return wxString();
}

static wxImage LoadEmbeddedBannerImage() {
    wxMemoryBuffer decoded = wxBase64Decode(BestiaryEmbeddedBanner::kBannerBase64,
                                            wxNO_LEN,
                                            wxBase64DecodeMode_Strict);
    if (decoded.IsEmpty()) return wxImage();

    wxMemoryInputStream stream(decoded.GetData(), decoded.GetDataLen());
    wxImage image(stream, wxBITMAP_TYPE_PNG);
    return image.IsOk() ? image : wxImage();
}

static wxImage LoadEmbeddedIconImage() {
    wxMemoryBuffer decoded = wxBase64Decode(BestiaryEmbeddedIcon::kIconBase64,
                                            wxNO_LEN,
                                            wxBase64DecodeMode_Strict);
    if (decoded.IsEmpty()) return wxImage();

    wxMemoryInputStream stream(decoded.GetData(), decoded.GetDataLen());
    wxImage image(stream, wxBITMAP_TYPE_PNG);
    return image.IsOk() ? image : wxImage();
}

static wxIconBundle BuildEmbeddedIconBundle() {
    wxIconBundle bundle;
    wxImage source = LoadEmbeddedIconImage();
    if (!source.IsOk()) return bundle;

    static const int kSizes[] = {16, 24, 32, 48, 64, 128, 256};
    for (int size : kSizes) {
        wxImage scaled = source.Scale(size, size, wxIMAGE_QUALITY_HIGH);
        if (!scaled.IsOk()) continue;
        wxIcon icon;
        icon.CopyFromBitmap(wxBitmap(scaled));
        if (icon.IsOk()) bundle.AddIcon(icon);
    }
    return bundle;
}

enum class GraphRequestKind {
    Explicit = 0,
    Implicit = 1,
    Vertical = 2
};

struct GraphRequest {
    GraphRequestKind kind = GraphRequestKind::Explicit;
    long long target = 0;
    wxString label;
    wxString serialized;
};

enum class KumaPlotRequestKind {
    Box = 0,
    Histogram = 1,
    Frequency = 2,
    Bar = 3,
    Density = 4,
    Dot = 5,
    ECDF = 6,
    QQ = 7,
    Scatter = 8,
    Regression = 9
};

struct KumaPlotRequest {
    KumaPlotRequestKind kind = KumaPlotRequestKind::Scatter;
    wxString title;
    wxString xLabel;
    wxString yLabel;
    wxString payload;
};

// ============================================================
// PtySession: spawns a child on a real pseudo-terminal so the
// child's stdin/stdout look like a TTY (readline activates).
// ============================================================
class PtySession {
public:
    PtySession() = default;
    ~PtySession() { Close(); }

    PtySession(const PtySession&) = delete;
    PtySession& operator=(const PtySession&) = delete;

        const wxString& LastError() const { return m_lastError; }

    bool Spawn(const wxString& exe, const wxArrayString& args, int cols, int rows) {
        if (cols < 1) cols = 80;
        if (rows < 1) rows = 24;
        m_lastError.clear();
#ifdef __WXMSW__
        return SpawnWindows(exe, args, cols, rows);
#else
        return SpawnPosix(exe, args, cols, rows);
#endif
    }

    int Read(char* buf, int size) {
#ifdef __WXMSW__
        if (!m_outRead) return -1;
        DWORD avail = 0;
        if (!PeekNamedPipe(m_outRead, nullptr, 0, nullptr, &avail, nullptr)) return -1;
        if (avail == 0) return 0;
        DWORD toRead = (DWORD)size < avail ? (DWORD)size : avail;
        DWORD n = 0;
        if (!ReadFile(m_outRead, buf, toRead, &n, nullptr)) return -1;
        return (int)n;
#else
        if (m_master < 0) return -1;
        ssize_t n = ::read(m_master, buf, size);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -1;
        }
        if (n == 0) return -1;
        return (int)n;
#endif
    }

    bool Write(const char* data, size_t len) {
#ifdef __WXMSW__
        if (!m_inWrite) return false;
        while (len > 0) {
            DWORD n = 0;
            if (!WriteFile(m_inWrite, data, (DWORD)len, &n, nullptr)) return false;
            if (n == 0) return false;
            data += n; len -= n;
        }
        return true;
#else
        if (m_master < 0) return false;
        while (len > 0) {
            ssize_t n = ::write(m_master, data, len);
            if (n < 0) {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
                return false;
            }
            data += n; len -= n;
        }
        return true;
#endif
    }

    bool Resize(int cols, int rows) {
        if (cols < 1 || rows < 1) return false;
#ifdef __WXMSW__
        ConPtyApi& api = GetConPtyApi();
        if (!m_hpc || !api.resize) return false;
        COORD sz = { (SHORT)cols, (SHORT)rows };
        return SUCCEEDED(api.resize(m_hpc, sz));
#else
        if (m_master < 0) return false;
        struct winsize ws;
        ws.ws_col = (unsigned short)cols;
        ws.ws_row = (unsigned short)rows;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        return ioctl(m_master, TIOCSWINSZ, &ws) == 0;
#endif
    }

    bool IsAlive() {
#ifdef __WXMSW__
        if (!m_pi.hProcess) return false;
        DWORD ec = 0;
        if (!GetExitCodeProcess(m_pi.hProcess, &ec)) return false;
        return ec == STILL_ACTIVE;
#else
        if (m_child <= 0) return false;
        int status = 0;
        pid_t r = waitpid(m_child, &status, WNOHANG);
        if (r == 0) return true;
        if (r == m_child || r < 0) { m_child = -1; return false; }
        return false;
#endif
    }

    void Close() {
#ifdef __WXMSW__
        ConPtyApi& api = GetConPtyApi();
        if (m_hpc && api.close) { api.close(m_hpc); m_hpc = nullptr; }
        if (m_inWrite)  { CloseHandle(m_inWrite);  m_inWrite  = nullptr; }
        if (m_outRead)  { CloseHandle(m_outRead);  m_outRead  = nullptr; }
        if (m_pi.hProcess) {
            // Give the child a moment to exit gracefully after the pty closed.
            if (WaitForSingleObject(m_pi.hProcess, 200) == WAIT_TIMEOUT)
                TerminateProcess(m_pi.hProcess, 1);
            WaitForSingleObject(m_pi.hProcess, 500);
            CloseHandle(m_pi.hProcess);
            CloseHandle(m_pi.hThread);
            m_pi = {};
        }
#else
        if (m_master >= 0) { ::close(m_master); m_master = -1; }
        if (m_child > 0) {
            ::kill(m_child, SIGHUP);
            for (int i = 0; i < 25; i++) {
                int status = 0;
                pid_t r = waitpid(m_child, &status, WNOHANG);
                if (r == m_child || r < 0) { m_child = -1; break; }
                usleep(10000);
            }
            if (m_child > 0) {
                ::kill(m_child, SIGKILL);
                int status = 0;
                waitpid(m_child, &status, 0);
                m_child = -1;
            }
        }
#endif
    }

private:
#ifdef __WXMSW__
    using CreatePseudoConsoleFn = HRESULT (WINAPI*)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
    using ResizePseudoConsoleFn = HRESULT (WINAPI*)(HPCON, COORD);
    using ClosePseudoConsoleFn = void (WINAPI*)(HPCON);

    struct ConPtyApi {
        CreatePseudoConsoleFn create = nullptr;
        ResizePseudoConsoleFn resize = nullptr;
        ClosePseudoConsoleFn close = nullptr;
        bool initialized = false;
    };

    static ConPtyApi& GetConPtyApi() {
        static ConPtyApi api;
        if (!api.initialized) {
            HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
            api.create = kernel32
                ? reinterpret_cast<CreatePseudoConsoleFn>(GetProcAddress(kernel32, "CreatePseudoConsole"))
                : nullptr;
            api.resize = kernel32
                ? reinterpret_cast<ResizePseudoConsoleFn>(GetProcAddress(kernel32, "ResizePseudoConsole"))
                : nullptr;
            api.close = kernel32
                ? reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(kernel32, "ClosePseudoConsole"))
                : nullptr;
            api.initialized = true;
        }
        return api;
    }

    static wxString DescribeWindowsError(DWORD code) {
        wchar_t* raw = nullptr;
        DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD len = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&raw), 0, nullptr);
        if (len == 0 || !raw) return wxString::Format("Windows error %lu", (unsigned long)code);

        wxString text(raw);
        LocalFree(raw);
        while (!text.empty() && (text.Last() == '\r' || text.Last() == '\n' || text.Last() == ' '))
            text.RemoveLast();
        return text;
    }

    static wxString ConPtyUnavailableMessage() {
        return "Embedded terminal support requires Windows 10 version 1809 or newer.";
    }

    bool SpawnWindows(const wxString& exe, const wxArrayString& args, int cols, int rows) {
        ConPtyApi& api = GetConPtyApi();
        if (!api.create || !api.resize || !api.close) {
            m_lastError = ConPtyUnavailableMessage();
            return false;
        }

        HANDLE inRead = nullptr, inWrite = nullptr;
        HANDLE outRead = nullptr, outWrite = nullptr;
        if (!CreatePipe(&inRead, &inWrite, nullptr, 0)) {
            m_lastError = wxString::Format("CreatePipe(stdin) failed: %s", DescribeWindowsError(GetLastError()));
            return false;
        }
        if (!CreatePipe(&outRead, &outWrite, nullptr, 0)) {
            m_lastError = wxString::Format("CreatePipe(stdout) failed: %s", DescribeWindowsError(GetLastError()));
            CloseHandle(inRead);
            CloseHandle(inWrite);
            return false;
        }

        COORD sz = { (SHORT)cols, (SHORT)rows };
        HRESULT hr = api.create(sz, inRead, outWrite, 0, &m_hpc);
        CloseHandle(inRead);
        CloseHandle(outWrite);
        if (FAILED(hr)) {
            m_lastError = wxString::Format("CreatePseudoConsole failed: %s", DescribeWindowsError(HRESULT_CODE(hr)));
            CloseHandle(inWrite);
            CloseHandle(outRead);
            return false;
        }

        STARTUPINFOEXW si = {};
        si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        SIZE_T attrSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
        si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrSize);
        if (!si.lpAttributeList) {
            m_lastError = "HeapAlloc failed while preparing Windows process attributes.";
            goto fail;
        }
        if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize)) {
            m_lastError = wxString::Format("InitializeProcThreadAttributeList failed: %s",
                                           DescribeWindowsError(GetLastError()));
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            si.lpAttributeList = nullptr;
            goto fail;
        }
        if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                        m_hpc, sizeof(m_hpc), nullptr, nullptr))
        {
            m_lastError = wxString::Format("UpdateProcThreadAttribute failed: %s",
                                           DescribeWindowsError(GetLastError()));
            goto fail;
        }

        {
            wxString cmd = wxString::Format("\"%s\"", exe);
            for (const auto& a : args) cmd += wxString::Format(" \"%s\"", a);
            std::wstring wcmd(cmd.wc_str(), cmd.length());
            wcmd.push_back(L'\0');

            SetEnvironmentVariableW(L"BESTIARY_GUI", L"1");
            BOOL ok = CreateProcessW(nullptr, &wcmd[0], nullptr, nullptr, FALSE,
                                      EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
                                      &si.StartupInfo, &m_pi);
            DeleteProcThreadAttributeList(si.lpAttributeList);
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
            si.lpAttributeList = nullptr;
            if (!ok) {
                m_lastError = wxString::Format("CreateProcessW failed: %s",
                                               DescribeWindowsError(GetLastError()));
                goto fail;
            }
        }

        m_inWrite = inWrite;
        m_outRead = outRead;
        return true;

    fail:
        if (si.lpAttributeList) {
            DeleteProcThreadAttributeList(si.lpAttributeList);
            HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        }
        if (m_hpc && api.close) { api.close(m_hpc); m_hpc = nullptr; }
        CloseHandle(inWrite);
        CloseHandle(outRead);
        return false;
    }

    HPCON m_hpc = nullptr;
    HANDLE m_inWrite = nullptr;
    HANDLE m_outRead = nullptr;
    PROCESS_INFORMATION m_pi = {};
#else
    bool SpawnPosix(const wxString& exe, const wxArrayString& args, int cols, int rows) {
        struct winsize ws;
        ws.ws_col = (unsigned short)cols;
        ws.ws_row = (unsigned short)rows;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;

        int master = -1;
        pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
        if (pid < 0) return false;
        if (pid == 0) {
            // Child
            setenv("TERM", "xterm-256color", 1);
            setenv("LANG", "C.UTF-8", 0);
            setenv("BESTIARY_GUI", "1", 1);

            std::string exeStr(exe.utf8_str());
            std::vector<std::string> argStrs;
            argStrs.reserve(args.size());
            for (const auto& a : args) argStrs.emplace_back(a.utf8_str());

            std::vector<char*> argv;
            argv.reserve(argStrs.size() + 2);
            argv.push_back(const_cast<char*>(exeStr.c_str()));
            for (auto& s : argStrs) argv.push_back(const_cast<char*>(s.c_str()));
            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            _exit(127);
        }

        int fl = fcntl(master, F_GETFL);
        fcntl(master, F_SETFL, fl | O_NONBLOCK);
        m_master = master;
        m_child = pid;
        return true;
    }

    int   m_master = -1;
    pid_t m_child  = -1;
#endif

    wxString m_lastError;
};

// ============================================================
// TerminalView: a wxWindow that hosts a child process on a pty
// and renders its output via a minimal VT100/CSI emulator.
// ============================================================
class TerminalView : public wxWindow {
public:
    static constexpr int kMinFontSize = 6;
    static constexpr int kMaxFontSize = 24;
    static constexpr size_t kMaxScrollbackLines = 5000;
    static constexpr int kPaddingX = 16;
    static constexpr int kPaddingY = 12;

    TerminalView(wxWindow* parent)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxWANTS_CHARS | wxBORDER_NONE),
          m_timer(this) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ColourFromRgb(theme::kPanelBg));
        SetForegroundColour(ColourFromRgb(theme::kTerminalText));
        m_font = BestiaryTerminalFont();
        m_screen.assign((size_t)m_cols * m_rows, Cell{});

        Bind(wxEVT_PAINT,            &TerminalView::OnPaint,      this);
        Bind(wxEVT_SIZE,             &TerminalView::OnSize,       this);
        Bind(wxEVT_CHAR,             &TerminalView::OnChar,       this);
        Bind(wxEVT_KEY_DOWN,         &TerminalView::OnKeyDown,    this);
        Bind(wxEVT_LEFT_DOWN,        &TerminalView::OnLeftDown,   this);
        Bind(wxEVT_LEFT_UP,          &TerminalView::OnLeftUp,     this);
        Bind(wxEVT_MOTION,           &TerminalView::OnMouseMove,  this);
        Bind(wxEVT_MOUSEWHEEL,       &TerminalView::OnMouseWheel, this);
        Bind(wxEVT_TIMER,            &TerminalView::OnTimer,      this);
        Bind(wxEVT_SET_FOCUS,        [this](wxFocusEvent& e){ Refresh(false); e.Skip(); });
        Bind(wxEVT_KILL_FOCUS,       [this](wxFocusEvent& e){ Refresh(false); e.Skip(); });
        Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&){});
    }

    ~TerminalView() override {
        m_timer.Stop();
        m_pty.Close();
    }

    bool Spawn(const wxString& exe, const wxArrayString& args = wxArrayString()) {
        EnsureCharSize();
        if (!m_pty.Spawn(exe, args, m_cols, m_rows)) {
            wxString message = wxString::Format("Could not start: %s\n", exe);
            if (!m_pty.LastError().empty()) message += m_pty.LastError() + "\n";
            FeedString(message);
            Refresh(false);
            return false;
        }
        m_running = true;
        m_timer.Start(25);
        return true;
    }

    void FocusInput() { SetFocus(); }

    void SetCloseCallback(std::function<void()> closeCallback) {
        m_closeCallback = std::move(closeCallback);
    }

    void SetGraphRequestCallback(std::function<void(const GraphRequest&)> graphRequestCallback) {
        m_graphRequestCallback = std::move(graphRequestCallback);
    }

    void SetKumaPlotRequestCallback(std::function<void(const KumaPlotRequest&)> kumaPlotRequestCallback) {
        m_kumaPlotRequestCallback = std::move(kumaPlotRequestCallback);
    }

    void RunScript(const wxString& path) {
        wxString cmd = wxString::Format("\\run{\"%s\"}\r", path);
        wxScopedCharBuffer u8 = cmd.ToUTF8();
        m_suppressNextInputRecord = true;
        m_pty.Write(u8.data(), u8.length());
    }

    const std::vector<wxString>& CommandHistory() const {
        return m_commandHistory;
    }

    void SetCommandHistory(const std::vector<wxString>& history) {
        m_commandHistory = history;
    }

    std::vector<wxString> TranscriptLines() const {
        std::vector<PhysicalLine> lines = CollectPhysicalLines();
        std::vector<wxString> out;
        wxString logical;
        bool haveLogical = false;
        bool seenContent = false;

        for (size_t i = 0; i < lines.size(); i++) {
            bool continuesNext = i + 1 < lines.size() && lines[i + 1].wrappedFromPrevious;
            wxString text = PhysicalLineText(lines[i], continuesNext);
            if (lines[i].wrappedFromPrevious) {
                if (!haveLogical) {
                    logical = text;
                    haveLogical = true;
                } else {
                    logical += text;
                }
                continue;
            }

            if (haveLogical) {
                if (!seenContent && logical.empty()) {
                    // Skip leading blank terminal rows
                } else {
                    if (!logical.empty()) seenContent = true;
                    if (seenContent) out.push_back(logical);
                }
            }
            logical = text;
            haveLogical = true;
        }

        if (haveLogical) {
            if (!seenContent && logical.empty()) {
                // Skip leading blank terminal rows
            } else {
                if (!logical.empty()) seenContent = true;
                if (seenContent) out.push_back(logical);
            }
        }

        while (!out.empty() && out.back().empty()) out.pop_back();
        if (!out.empty()) {
            wxString last = out.back();
            last.Trim(true).Trim(false);
            if (last == ">") out.pop_back();
        }
        return out;
    }

    void SetTranscriptLines(const std::vector<wxString>& lines) {
        if (lines.empty()) return;
        ResetTerminal();
        for (const wxString& line : lines) {
            FeedString(line);
            FeedString("\r\n");
        }
        JumpToLiveView();
        Refresh(false);
    }

    static constexpr int kBaseFontPointSize = 11;

    int GetZoomDelta() const { return m_zoomDelta; }

    void SetZoomDelta(int delta) {
        delta = std::clamp(delta, kMinFontSize - kBaseFontPointSize,
                                   kMaxFontSize - kBaseFontPointSize);
        if (delta == m_zoomDelta && m_font.GetPointSize() == kBaseFontPointSize + delta) return;
        m_zoomDelta = delta;
        m_font = BestiaryTerminalFont();
        m_font.SetPointSize(kBaseFontPointSize + m_zoomDelta);
        m_charW = 0;
        m_charH = 0;
        UpdateGeometry(true);
    }

private:
    struct Cell {
        wxChar  ch    = L' ';
        uint32_t fg   = theme::kTerminalText;
        uint32_t bg   = theme::kTerminalBg;
        uint8_t  attrs = 0; // 1=bold, 2=underline, 4=reverse
    };

    struct PhysicalLine {
        std::vector<Cell> cells;
        bool wrappedFromPrevious = false;
    };

    struct BufferPos {
        int line = 0;
        int col  = 0;
    };

    enum class State { Ground, Esc, Csi, Osc, Charset };

    // ---------- layout / painting ----------

    void EnsureCharSize() {
        if (m_charW > 0 && m_charH > 0) return;
        wxClientDC dc(this);
        dc.SetFont(m_font);

        wxCoord charW = dc.GetCharWidth();
        wxCoord charH = dc.GetCharHeight();

        if (charW <= 0 || charH <= 0) {
            wxCoord width = 0;
            wxCoord height = 0;
            wxCoord descent = 0;
            wxCoord leading = 0;
            dc.GetTextExtent("MMMMMMMMMM", &width, &height, &descent, &leading);
            charW = width > 0 ? width / 10 : 9;
            charH = height > 0 ? height : 18;
        }

        m_charW = std::max(8, (int)charW);
        m_charH = std::max(14, (int)charH);
        m_textOffsetY = 0;
    }

    void OnSize(wxSizeEvent& evt) {
        UpdateGeometry(true);
        evt.Skip();
    }

    void ResizeBuffer(int cols, int rows) {
        ClearSelection(false);
        std::vector<PhysicalLine> reflowed = ReflowPhysicalLines(CollectPhysicalLines(), cols,
                                                                 (int)m_history.size() + m_curRow,
                                                                 m_curCol);

        m_cols       = cols;
        m_rows       = rows;
        m_screen.assign((size_t)m_cols * m_rows, Cell{});
        m_screenWrappedFromPrevious.assign((size_t)m_rows, false);

        if ((int)reflowed.size() < m_rows) {
            reflowed.resize((size_t)m_rows, MakeBlankPhysicalLine(m_cols));
        }

        int cursorPhysicalIndex = m_pendingCursorPhysicalIndex;
        int historyCount = std::max(0, (int)reflowed.size() - m_rows);
        if (historyCount > (int)kMaxScrollbackLines) {
            int drop = historyCount - (int)kMaxScrollbackLines;
            reflowed.erase(reflowed.begin(), reflowed.begin() + drop);
            cursorPhysicalIndex = std::max(0, cursorPhysicalIndex - drop);
            historyCount = (int)kMaxScrollbackLines;
        }

        m_history.clear();
        for (int i = 0; i < historyCount; i++) m_history.push_back(std::move(reflowed[(size_t)i]));
        for (int r = 0; r < m_rows; r++) SetScreenLine(r, reflowed[(size_t)(historyCount + r)]);

        m_curRow     = std::clamp(cursorPhysicalIndex - historyCount, 0, m_rows - 1);
        m_curCol     = std::clamp(m_pendingCursorCol, 0, m_cols - 1);
        m_scrollTop  = 0;
        m_scrollBot  = rows - 1;
        ClampViewport();
    }

    static wxColour ToColour(uint32_t rgb) {
        return wxColour((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    static bool IsBefore(const BufferPos& lhs, const BufferPos& rhs) {
        return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.col < rhs.col);
    }

    int TotalBufferLines() const {
        return (int)m_history.size() + m_rows;
    }

    int FirstVisibleLine() const {
        return std::max(0, TotalBufferLines() - m_rows - m_viewOffset);
    }

    void ClampViewport() {
        m_viewOffset = std::clamp(m_viewOffset, 0, (int)m_history.size());
    }

    Cell GetBufferCell(int line, int col) const {
        col = std::clamp(col, 0, m_cols - 1);
        if (line < (int)m_history.size()) return m_history[(size_t)line].cells[(size_t)col];
        int screenRow = std::clamp(line - (int)m_history.size(), 0, m_rows - 1);
        return m_screen[(size_t)screenRow * m_cols + col];
    }

    std::vector<Cell> CopyScreenLineCells(int row) const {
        auto begin = m_screen.begin() + (size_t)row * m_cols;
        return std::vector<Cell>(begin, begin + m_cols);
    }

    PhysicalLine MakeBlankPhysicalLine(int cols, bool wrappedFromPrevious = false) const {
        PhysicalLine line;
        line.cells.assign((size_t)cols, Cell{});
        line.wrappedFromPrevious = wrappedFromPrevious;
        return line;
    }

    void SetScreenLine(int row, const PhysicalLine& line) {
        size_t offset = (size_t)row * m_cols;
        std::copy(line.cells.begin(), line.cells.end(), m_screen.begin() + offset);
        m_screenWrappedFromPrevious[(size_t)row] = line.wrappedFromPrevious;
    }

    bool IsBlankCell(const Cell& cell) const {
        return cell.ch == L' ' && cell.fg == theme::kTerminalText
            && cell.bg == theme::kTerminalBg && cell.attrs == 0;
    }

    bool ScreenRowHasContent(int row) const {
        size_t offset = (size_t)row * m_cols;
        for (int col = 0; col < m_cols; col++) {
            if (!IsBlankCell(m_screen[offset + (size_t)col])) return true;
        }
        return false;
    }

    int LastScreenRowToPreserve() const {
        int lastRow = m_curRow;
        for (int row = m_rows - 1; row > m_curRow; row--) {
            if (m_screenWrappedFromPrevious[(size_t)row] || ScreenRowHasContent(row)) {
                lastRow = row;
                break;
            }
        }
        return lastRow;
    }

    int PhysicalLineContentLength(const PhysicalLine& line, bool continuesNext, int minLength = 0) const {
        if (continuesNext) return std::max(minLength, (int)line.cells.size());
        int length = (int)line.cells.size();
        while (length > minLength && IsBlankCell(line.cells[(size_t)(length - 1)])) length--;
        return length;
    }

    wxString PhysicalLineText(const PhysicalLine& line, bool continuesNext) const {
        wxString text;
        int length = PhysicalLineContentLength(line, continuesNext);
        text.reserve((size_t)length);
        for (int i = 0; i < length; i++) text += line.cells[(size_t)i].ch;
        return text;
    }

    std::vector<PhysicalLine> CollectPhysicalLines() const {
        std::vector<PhysicalLine> lines;
        int lastScreenRow = LastScreenRowToPreserve();
        lines.reserve(m_history.size() + (size_t)(lastScreenRow + 1));
        for (const PhysicalLine& line : m_history) lines.push_back(line);
        for (int row = 0; row <= lastScreenRow; row++) {
            PhysicalLine line;
            line.cells = CopyScreenLineCells(row);
            line.wrappedFromPrevious = m_screenWrappedFromPrevious[(size_t)row];
            lines.push_back(std::move(line));
        }
        return lines;
    }

    std::vector<PhysicalLine> ReflowPhysicalLines(const std::vector<PhysicalLine>& lines,
                                                  int newCols,
                                                  int cursorPhysicalIndex,
                                                  int cursorCol) {
        std::vector<std::vector<Cell>> logicalLines;
        logicalLines.reserve(lines.size());

        int cursorLogicalIndex = 0;
        int cursorLogicalOffset = 0;

        for (size_t i = 0; i < lines.size(); i++) {
            if (logicalLines.empty() || !lines[i].wrappedFromPrevious) logicalLines.emplace_back();

            bool continuesNext = i + 1 < lines.size() && lines[i + 1].wrappedFromPrevious;
            int minLength = ((int)i == cursorPhysicalIndex) ? cursorCol : 0;
            int keepLength = PhysicalLineContentLength(lines[i], continuesNext, minLength);

            if ((int)i == cursorPhysicalIndex) {
                cursorLogicalIndex = (int)logicalLines.size() - 1;
                cursorLogicalOffset = (int)logicalLines.back().size() + std::min(cursorCol, keepLength);
            }

            logicalLines.back().insert(logicalLines.back().end(),
                                       lines[i].cells.begin(),
                                       lines[i].cells.begin() + keepLength);
        }

        std::vector<PhysicalLine> reflowed;
        reflowed.reserve(lines.size());
        m_pendingCursorPhysicalIndex = 0;
        m_pendingCursorCol = 0;

        for (size_t logicalIndex = 0; logicalIndex < logicalLines.size(); logicalIndex++) {
            const auto& logical = logicalLines[logicalIndex];
            int chunkCount = logical.empty() ? 1 : std::max(1, ((int)logical.size() + newCols - 1) / newCols);
            int baseIndex = (int)reflowed.size();

            for (int chunk = 0; chunk < chunkCount; chunk++) {
                PhysicalLine line = MakeBlankPhysicalLine(newCols, chunk > 0);
                size_t start = (size_t)chunk * (size_t)newCols;
                if (start < logical.size()) {
                    size_t length = std::min((size_t)newCols, logical.size() - start);
                    std::copy_n(logical.begin() + (ptrdiff_t)start, length, line.cells.begin());
                }
                reflowed.push_back(std::move(line));
            }

            if ((int)logicalIndex == cursorLogicalIndex) {
                int cursorChunk = chunkCount > 0 ? std::min(chunkCount - 1, cursorLogicalOffset / newCols) : 0;
                m_pendingCursorPhysicalIndex = baseIndex + cursorChunk;
                m_pendingCursorCol = cursorLogicalOffset - cursorChunk * newCols;
                if (m_pendingCursorCol >= newCols) m_pendingCursorCol = newCols - 1;
            }
        }

        if (reflowed.empty()) reflowed.push_back(MakeBlankPhysicalLine(newCols));
        return reflowed;
    }

    void AppendScrollbackLine(PhysicalLine line) {
        m_history.push_back(std::move(line));
        if (m_history.size() <= kMaxScrollbackLines) {
            if (m_viewOffset > 0) m_viewOffset++;
        } else {
            m_history.pop_front();
            ClearSelection(false);
        }
        ClampViewport();
    }

    void UpdateGeometry(bool resizePty) {
        m_charW = 0;
        m_charH = 0;
        EnsureCharSize();
        wxSize sz = GetClientSize();
        int contentWidth = std::max(1, sz.x - 2 * kPaddingX);
        int contentHeight = std::max(1, sz.y - 2 * kPaddingY);
        int newCols = std::max(20, contentWidth / std::max(1, m_charW));
        int newRows = std::max(5,  contentHeight / std::max(1, m_charH));
        if (newCols != m_cols || newRows != m_rows) {
            ResizeBuffer(newCols, newRows);
            if (resizePty) m_pty.Resize(m_cols, m_rows);
        }
        Refresh(false);
    }

    void ScrollViewport(int lines) {
        int maxOffset = (int)m_history.size();
        int next = std::clamp(m_viewOffset + lines, 0, maxOffset);
        if (next == m_viewOffset) return;
        m_viewOffset = next;
        Refresh(false);
    }

    void JumpToLiveView() {
        if (m_viewOffset == 0) return;
        m_viewOffset = 0;
        Refresh(false);
    }


    BufferPos ScreenPointToBufferPos(const wxPoint& pt) const {
        BufferPos pos;
        int localY = std::max(0, pt.y - kPaddingY);
        int localX = std::max(0, pt.x - kPaddingX);
        pos.line = FirstVisibleLine() + std::clamp(localY / std::max(1, m_charH), 0, m_rows - 1);
        pos.col  = std::clamp(localX / std::max(1, m_charW), 0, m_cols - 1);
        return pos;
    }

    void ClearSelection(bool repaint = true) {
        bool hadSelection = m_hasSelection || m_selecting;
        m_hasSelection = false;
        m_selecting = false;
        if (hadSelection && repaint) Refresh(false);
    }

    bool NormalizedSelection(BufferPos& start, BufferPos& end) const {
        if (!m_hasSelection) return false;
        start = m_selAnchor;
        end = m_selFocus;
        if (IsBefore(end, start)) std::swap(start, end);
        return true;
    }

    bool IsCellSelected(int line, int col) const {
        BufferPos start, end;
        if (!NormalizedSelection(start, end)) return false;
        if (line < start.line || line > end.line) return false;
        if (start.line == end.line) return col >= start.col && col <= end.col;
        if (line == start.line) return col >= start.col;
        if (line == end.line) return col <= end.col;
        return true;
    }

    wxString SelectedText() const {
        BufferPos start, end;
        if (!NormalizedSelection(start, end)) return wxEmptyString;

        wxString out;
        for (int line = start.line; line <= end.line; line++) {
            int beginCol = (line == start.line) ? start.col : 0;
            int endCol   = (line == end.line) ? end.col : (m_cols - 1);
            wxString chunk;
            for (int col = beginCol; col <= endCol; col++) chunk += GetBufferCell(line, col).ch;
            while (!chunk.empty() && chunk.Last() == L' ') chunk.RemoveLast();
            out += chunk;
            if (line != end.line) out += '\n';
        }
        return out;
    }

    void CopySelection() {
        if (!m_hasSelection) return;
        wxString text = SelectedText();
        if (text.empty()) return;
        if (!wxTheClipboard->Open()) return;
        wxTheClipboard->SetData(new wxTextDataObject(text));
        wxTheClipboard->Close();
    }

    void PasteClipboard() {
        if (!wxTheClipboard->Open()) return;
        if (!wxTheClipboard->IsSupported(wxDF_TEXT) && !wxTheClipboard->IsSupported(wxDF_UNICODETEXT)) {
            wxTheClipboard->Close();
            return;
        }

        wxTextDataObject data;
        bool ok = wxTheClipboard->GetData(data);
        wxTheClipboard->Close();
        if (!ok) return;

        JumpToLiveView();
        wxScopedCharBuffer u8 = data.GetText().ToUTF8();
        if (u8.data() && u8.length() > 0) m_pty.Write(u8.data(), u8.length());
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        wxSize sz = GetClientSize();
        dc.SetBrush(wxBrush(ColourFromRgb(theme::kPanelBg)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(0, 0, sz.x, sz.y);

        wxRect terminalRect(kPaddingX / 2,
                            kPaddingY / 2,
                            std::max(1, sz.x - kPaddingX),
                            std::max(1, sz.y - kPaddingY));
        dc.SetPen(wxPen(ColourFromRgb(theme::kBorder), 1));
        dc.SetBrush(wxBrush(ColourFromRgb(theme::kTerminalBg)));
        dc.DrawRoundedRectangle(terminalRect, 10);

        int firstVisibleLine = FirstVisibleLine();
        for (int r = 0; r < m_rows; r++) {
            int line = firstVisibleLine + r;
            int c = 0;
            while (c < m_cols) {
                Cell start = GetBufferCell(line, c);
                bool startSelected = IsCellSelected(line, c);
                int run = 1;
                while (c + run < m_cols) {
                    Cell nx = GetBufferCell(line, c + run);
                    if (nx.fg != start.fg || nx.bg != start.bg || nx.attrs != start.attrs) break;
                    if (IsCellSelected(line, c + run) != startSelected) break;
                    run++;
                }
                uint32_t fg = start.fg, bg = start.bg;
                if (start.attrs & 4) std::swap(fg, bg);
                if (startSelected) {
                    fg = 0xFFFFFFu;
                    bg = theme::kSelectionBg;
                }

                int x = kPaddingX + c * m_charW;
                int y = kPaddingY + r * m_charH;
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(ColourFromRgb(bg)));
                dc.DrawRectangle(x, y, run * m_charW, m_charH);

                wxFont f = m_font;
                f.SetWeight((start.attrs & 1) ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
                f.SetUnderlined((start.attrs & 2) != 0);
                dc.SetFont(f);
                dc.SetTextForeground(ColourFromRgb(fg));
                dc.SetBackgroundMode(wxTRANSPARENT);

                wxString s;
                s.reserve(run);
                for (int k = 0; k < run; k++) s += GetBufferCell(line, c + k).ch;
                dc.DrawText(s, x, y + m_textOffsetY);
                c += run;
            }
        }

        // Cursor
        int cx = kPaddingX + m_curCol * m_charW;
        int cy = kPaddingY + (m_curRow + (m_viewOffset == 0 ? 0 : -m_viewOffset)) * m_charH;
        if (HasFocus() && m_viewOffset == 0) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(ColourFromRgb(theme::kTerminalCursor)));
            dc.DrawRectangle(cx, cy, m_charW, m_charH);
            const Cell& cell = m_screen[(size_t)m_curRow * m_cols + m_curCol];
            dc.SetTextForeground(ColourFromRgb(cell.bg));
            dc.SetBackgroundMode(wxTRANSPARENT);
            dc.SetFont(m_font);
            dc.DrawText(wxString(cell.ch), cx, cy + m_textOffsetY);
        } else if (m_viewOffset == 0) {
            dc.SetPen(wxPen(ColourFromRgb(theme::kBorder)));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(cx, cy, m_charW, m_charH);
        }
    }

    // ---------- pty I/O ----------

    void OnTimer(wxTimerEvent&) {
        if (!DrainPty() && !m_pty.IsAlive() && m_running) {
            m_running = false;
            FeedString("\r\n[Bestiary process exited]\r\n");
            m_timer.Stop();
            Refresh(false);
        }
    }

    bool DrainPty() {
        char buf[4096];
        bool any = false;
        for (int i = 0; i < 16; i++) {
            int n = m_pty.Read(buf, sizeof(buf));
            if (n <= 0) {
                if (n < 0) m_running = false;
                break;
            }
            ClearSelection(false);
            for (int k = 0; k < n; k++) Feed((unsigned char)buf[k]);
            any = true;
        }
        if (any) Refresh(false);
        return any;
    }

    void FeedString(const wxString& s) {
        wxScopedCharBuffer u8 = s.ToUTF8();
        for (size_t i = 0; i < u8.length(); i++) Feed((unsigned char)u8.data()[i]);
    }

    // ---------- VT parser ----------

    void Feed(unsigned char b) {
        switch (m_state) {
        case State::Ground:
            if (b == 0x1b) { m_state = State::Esc; m_paramBuf.clear(); m_csiQMark = false; }
            else if (b < 0x20 || b == 0x7f) HandleControl(b);
            else FeedUtf8(b);
            break;
        case State::Esc:
            if      (b == '[') m_state = State::Csi;
            else if (b == ']') { m_state = State::Osc; m_paramBuf.clear(); }
            else if (b == '(' || b == ')') m_state = State::Charset;
            else if (b == '7') { m_savedRow = m_curRow; m_savedCol = m_curCol; m_state = State::Ground; }
            else if (b == '8') { m_curRow = m_savedRow; m_curCol = m_savedCol; m_state = State::Ground; }
            else if (b == 'D') { LineFeed();          m_state = State::Ground; }
            else if (b == 'E') { m_curCol = 0; LineFeed(); m_state = State::Ground; }
            else if (b == 'M') { ReverseLineFeed();   m_state = State::Ground; }
            else if (b == 'c') { ResetTerminal();     m_state = State::Ground; }
            else                m_state = State::Ground;
            break;
        case State::Csi:
            if (b == '?' && m_paramBuf.empty()) m_csiQMark = true;
            else if ((b >= '0' && b <= '9') || b == ';') m_paramBuf += (char)b;
            else if (b >= 0x40 && b <= 0x7e) { HandleCsi((char)b); m_state = State::Ground; }
            else m_paramBuf += (char)b;
            break;
        case State::Osc:
            if (b == 0x07) { HandleOsc(m_paramBuf); m_state = State::Ground; m_paramBuf.clear(); }
            else if (b == 0x1b) { m_state = State::Esc; m_paramBuf.clear(); }
            else m_paramBuf += (char)b;
            break;
        case State::Charset:
            m_state = State::Ground;
            break;
        }
    }

    void HandleOsc(const std::string& payload) {
        static const std::string graphPrefix = "777;BESTIARY_GRAPH\t";
        static const std::string plotPrefix = "777;BESTIARY_KUMA_PLOT\t";
        static const std::string inputPrefix = "777;BESTIARY_INPUT\t";
        if (payload.rfind(inputPrefix, 0) == 0) {
            wxString input = DecodeHexPayload(payload.substr(inputPrefix.size()));
            if (m_suppressNextInputRecord) m_suppressNextInputRecord = false;
            else if (!input.empty()) m_commandHistory.push_back(input);
            return;
        }
        if (payload.rfind(graphPrefix, 0) != 0 && payload.rfind(plotPrefix, 0) != 0) return;

        std::vector<std::string> fields;
        bool isGraph = payload.rfind(graphPrefix, 0) == 0;
        const std::string& prefix = isGraph ? graphPrefix : plotPrefix;
        size_t start = prefix.size();
        while (start <= payload.size()) {
            size_t at = payload.find('\t', start);
            if (at == std::string::npos) {
                fields.push_back(payload.substr(start));
                break;
            }
            fields.push_back(payload.substr(start, at - start));
            start = at + 1;
        }
        if (isGraph) {
            if (fields.size() < 4 || !m_graphRequestCallback) return;

            GraphRequest request;
            int kind = std::atoi(fields[0].c_str());
            if (kind == 1) request.kind = GraphRequestKind::Implicit;
            else if (kind == 2) request.kind = GraphRequestKind::Vertical;
            else request.kind = GraphRequestKind::Explicit;
            request.target = std::strtoll(fields[1].c_str(), nullptr, 10);
            request.label = wxString::FromUTF8(fields[2].c_str());
            request.serialized = wxString::FromUTF8(fields[3].c_str());
            m_graphRequestCallback(request);
            return;
        }

        if (fields.size() < 5 || !m_kumaPlotRequestCallback) return;
        KumaPlotRequest request;
        int kind = std::atoi(fields[0].c_str());
        if (kind == 0) request.kind = KumaPlotRequestKind::Box;
        else if (kind == 1) request.kind = KumaPlotRequestKind::Histogram;
        else if (kind == 2) request.kind = KumaPlotRequestKind::Frequency;
        else if (kind == 3) request.kind = KumaPlotRequestKind::Bar;
        else if (kind == 4) request.kind = KumaPlotRequestKind::Density;
        else if (kind == 5) request.kind = KumaPlotRequestKind::Dot;
        else if (kind == 6) request.kind = KumaPlotRequestKind::ECDF;
        else if (kind == 7) request.kind = KumaPlotRequestKind::QQ;
        else if (kind == 9) request.kind = KumaPlotRequestKind::Regression;
        else request.kind = KumaPlotRequestKind::Scatter;
        request.title = wxString::FromUTF8(fields[1].c_str());
        request.xLabel = wxString::FromUTF8(fields[2].c_str());
        request.yLabel = wxString::FromUTF8(fields[3].c_str());
        request.payload = wxString::FromUTF8(fields[4].c_str());
        m_kumaPlotRequestCallback(request);
    }

    static int HexValue(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        return -1;
    }

    static wxString DecodeHexPayload(const std::string& hex) {
        std::string bytes;
        bytes.reserve(hex.size() / 2);
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            int hi = HexValue(hex[i]);
            int lo = HexValue(hex[i + 1]);
            if (hi < 0 || lo < 0) return wxString();
            bytes.push_back((char)((hi << 4) | lo));
        }
        return wxString::FromUTF8(bytes.c_str());
    }

    void FeedUtf8(unsigned char b) {
        if (m_utf8Remaining > 0) {
            if ((b & 0xC0) == 0x80) {
                m_utf8Codepoint = (m_utf8Codepoint << 6) | (b & 0x3F);
                if (--m_utf8Remaining == 0) PutChar((wxChar)m_utf8Codepoint);
            } else {
                m_utf8Remaining = 0;
                PutChar(L'?');
                FeedUtf8(b);
            }
        } else if (b < 0x80) {
            PutChar((wxChar)b);
        } else if ((b & 0xE0) == 0xC0) { m_utf8Codepoint = b & 0x1F; m_utf8Remaining = 1; }
        else if   ((b & 0xF0) == 0xE0) { m_utf8Codepoint = b & 0x0F; m_utf8Remaining = 2; }
        else if   ((b & 0xF8) == 0xF0) { m_utf8Codepoint = b & 0x07; m_utf8Remaining = 3; }
        else                            PutChar(L'?');
    }

    void HandleControl(unsigned char b) {
        switch (b) {
        case '\r': m_curCol = 0; break;
        case '\n':
        case 0x0B:
        case 0x0C:
            LineFeed(); break;
        case '\b':
            if (m_curCol > 0) m_curCol--;
            break;
        case '\t': {
            int nx = (m_curCol / 8 + 1) * 8;
            if (nx >= m_cols) nx = m_cols - 1;
            m_curCol = nx;
            break;
        }
        case 0x07: wxBell(); break;
        default: break;
        }
    }

    void PutChar(wxChar ch) {
        if (m_curCol >= m_cols) {
            m_curCol = 0;
            LineFeed(true);
        }
        Cell& cell = m_screen[(size_t)m_curRow * m_cols + m_curCol];
        cell.ch    = ch;
        cell.fg    = m_curFg;
        cell.bg    = m_curBg;
        cell.attrs = m_curAttrs;
        m_curCol++;
    }

    void LineFeed(bool wrapped = false) {
        if (m_curRow == m_scrollBot) {
            ScrollRegionUp();
        } else if (m_curRow < m_rows - 1) {
            m_curRow++;
        }
        if (m_curRow >= 0 && m_curRow < m_rows) m_screenWrappedFromPrevious[(size_t)m_curRow] = wrapped;
    }

    void ReverseLineFeed() {
        if (m_curRow == m_scrollTop)  ScrollRegionDown();
        else if (m_curRow > 0)        m_curRow--;
    }

    void ScrollRegionUp() {
        if (m_scrollTop == 0 && m_scrollBot == m_rows - 1) {
            PhysicalLine line;
            line.cells = CopyScreenLineCells(m_scrollTop);
            line.wrappedFromPrevious = m_screenWrappedFromPrevious[(size_t)m_scrollTop];
            AppendScrollbackLine(std::move(line));
        }
        for (int r = m_scrollTop; r < m_scrollBot; r++)
            for (int c = 0; c < m_cols; c++)
                m_screen[(size_t)r * m_cols + c] = m_screen[(size_t)(r + 1) * m_cols + c];
        for (int r = m_scrollTop; r < m_scrollBot; r++)
            m_screenWrappedFromPrevious[(size_t)r] = m_screenWrappedFromPrevious[(size_t)(r + 1)];
        for (int c = 0; c < m_cols; c++)
            m_screen[(size_t)m_scrollBot * m_cols + c] = Cell{};
        m_screenWrappedFromPrevious[(size_t)m_scrollBot] = false;
    }

    void ScrollRegionDown() {
        for (int r = m_scrollBot; r > m_scrollTop; r--)
            for (int c = 0; c < m_cols; c++)
                m_screen[(size_t)r * m_cols + c] = m_screen[(size_t)(r - 1) * m_cols + c];
        for (int r = m_scrollBot; r > m_scrollTop; r--)
            m_screenWrappedFromPrevious[(size_t)r] = m_screenWrappedFromPrevious[(size_t)(r - 1)];
        for (int c = 0; c < m_cols; c++)
            m_screen[(size_t)m_scrollTop * m_cols + c] = Cell{};
        m_screenWrappedFromPrevious[(size_t)m_scrollTop] = false;
    }

    std::vector<int> ParseParams(int defaultVal) {
        std::vector<int> out;
        if (m_paramBuf.empty()) { out.push_back(defaultVal); return out; }
        int cur = 0; bool any = false;
        for (char c : m_paramBuf) {
            if (c == ';') { out.push_back(any ? cur : defaultVal); cur = 0; any = false; }
            else if (c >= '0' && c <= '9') { cur = cur * 10 + (c - '0'); any = true; }
        }
        out.push_back(any ? cur : defaultVal);
        return out;
    }

    void HandleCsi(char final) {
        if (m_csiQMark) {
            // Private modes; we ignore most. This deliberately swallows DECSET/RST.
            m_paramBuf.clear();
            return;
        }
        bool hadParam = !m_paramBuf.empty();
        auto p = ParseParams(1);
        switch (final) {
        case 'A': m_curRow = std::max(0, m_curRow - p[0]); break;
        case 'B': m_curRow = std::min(m_rows - 1, m_curRow + p[0]); break;
        case 'C': m_curCol = std::min(m_cols - 1, m_curCol + p[0]); break;
        case 'D': m_curCol = std::max(0, m_curCol - p[0]); break;
        case 'E': m_curCol = 0; m_curRow = std::min(m_rows - 1, m_curRow + p[0]); break;
        case 'F': m_curCol = 0; m_curRow = std::max(0, m_curRow - p[0]); break;
        case 'G': m_curCol = std::min(m_cols - 1, std::max(0, p[0] - 1)); break;
        case 'H': case 'f': {
            int r = (p.size() > 0) ? std::max(0, p[0] - 1) : 0;
            int c = (p.size() > 1) ? std::max(0, p[1] - 1) : 0;
            m_curRow = std::min(r, m_rows - 1);
            m_curCol = std::min(c, m_cols - 1);
            break;
        }
        case 'J': EraseDisplay(hadParam ? p[0] : 0); break;
        case 'K': EraseLine(hadParam ? p[0] : 0); break;
        case 'L': for (int i = 0; i < p[0]; i++) {
            int saveTop = m_scrollTop; m_scrollTop = m_curRow;
            ScrollRegionDown(); m_scrollTop = saveTop;
        } break;
        case 'M': for (int i = 0; i < p[0]; i++) {
            int saveTop = m_scrollTop; m_scrollTop = m_curRow;
            ScrollRegionUp();   m_scrollTop = saveTop;
        } break;
        case 'P': {
            int n = std::min(p[0], m_cols - m_curCol);
            for (int c = m_curCol; c < m_cols - n; c++)
                m_screen[(size_t)m_curRow * m_cols + c] = m_screen[(size_t)m_curRow * m_cols + c + n];
            for (int c = m_cols - n; c < m_cols; c++)
                m_screen[(size_t)m_curRow * m_cols + c] = Cell{};
            break;
        }
        case '@': {
            int n = std::min(p[0], m_cols - m_curCol);
            for (int c = m_cols - 1; c >= m_curCol + n; c--)
                m_screen[(size_t)m_curRow * m_cols + c] = m_screen[(size_t)m_curRow * m_cols + c - n];
            for (int c = m_curCol; c < m_curCol + n; c++)
                m_screen[(size_t)m_curRow * m_cols + c] = Cell{};
            break;
        }
        case 'X': {
            int n = std::min(p[0], m_cols - m_curCol);
            for (int c = m_curCol; c < m_curCol + n; c++)
                m_screen[(size_t)m_curRow * m_cols + c] = Cell{};
            break;
        }
        case 'r': {
            int top = (p.size() > 0) ? std::max(0, p[0] - 1) : 0;
            int bot = (p.size() > 1) ? std::min(m_rows - 1, p[1] - 1) : m_rows - 1;
            if (top < bot) { m_scrollTop = top; m_scrollBot = bot; }
            m_curRow = 0; m_curCol = 0;
            break;
        }
        case 's': m_savedRow = m_curRow; m_savedCol = m_curCol; break;
        case 'u': m_curRow = m_savedRow; m_curCol = m_savedCol; break;
        case 'm': HandleSGR(p); break;
        case 'n':
            if (p[0] == 6) {
                char rep[32];
                int len = std::snprintf(rep, sizeof(rep), "\x1b[%d;%dR", m_curRow + 1, m_curCol + 1);
                if (len > 0) m_pty.Write(rep, (size_t)len);
            }
            break;
        default: break;
        }
        m_paramBuf.clear();
    }

    void HandleSGR(const std::vector<int>& p) {
        static const uint32_t pal[8] = {
            0x000000, 0xCC0000, 0x00CC00, 0xCCCC00,
            0x0000CC, 0xCC00CC, 0x00CCCC, 0xCCCCCC,
        };
        static const uint32_t bri[8] = {
            0x808080, 0xFF5555, 0x55FF55, 0xFFFF55,
            0x5555FF, 0xFF55FF, 0x55FFFF, 0xFFFFFF,
        };
        for (size_t i = 0; i < p.size(); i++) {
            int n = p[i];
            if      (n == 0)               { m_curFg = theme::kTerminalText; m_curBg = theme::kTerminalBg; m_curAttrs = 0; }
            else if (n == 1)               m_curAttrs |= 1;
            else if (n == 4)               m_curAttrs |= 2;
            else if (n == 7)               m_curAttrs |= 4;
            else if (n == 22)              m_curAttrs &= ~1;
            else if (n == 24)              m_curAttrs &= ~2;
            else if (n == 27)              m_curAttrs &= ~4;
            else if (n >= 30  && n <= 37)  m_curFg = pal[n - 30];
            else if (n == 39)              m_curFg = theme::kTerminalText;
            else if (n >= 40  && n <= 47)  m_curBg = pal[n - 40];
            else if (n == 49)              m_curBg = theme::kTerminalBg;
            else if (n >= 90  && n <= 97)  m_curFg = bri[n - 90];
            else if (n >= 100 && n <= 107) m_curBg = bri[n - 100];
        }
    }

    void EraseLine(int mode) {
        int s, e;
        if      (mode == 0) { s = m_curCol; e = m_cols; }
        else if (mode == 1) { s = 0;        e = std::min(m_curCol + 1, m_cols); }
        else                { s = 0;        e = m_cols; }
        for (int c = s; c < e; c++) m_screen[(size_t)m_curRow * m_cols + c] = Cell{};
    }

    void EraseDisplay(int mode) {
        if (mode == 0) {
            for (int c = m_curCol; c < m_cols; c++) m_screen[(size_t)m_curRow * m_cols + c] = Cell{};
            for (int r = m_curRow + 1; r < m_rows; r++)
                for (int c = 0; c < m_cols; c++) m_screen[(size_t)r * m_cols + c] = Cell{};
        } else if (mode == 1) {
            for (int r = 0; r < m_curRow; r++)
                for (int c = 0; c < m_cols; c++) m_screen[(size_t)r * m_cols + c] = Cell{};
            for (int c = 0; c <= m_curCol && c < m_cols; c++)
                m_screen[(size_t)m_curRow * m_cols + c] = Cell{};
        } else {
            for (auto& cell : m_screen) cell = Cell{};
        }
    }

    void ResetTerminal() {
        ClearSelection(false);
        m_history.clear();
        m_viewOffset = 0;
        for (auto& cell : m_screen) cell = Cell{};
        std::fill(m_screenWrappedFromPrevious.begin(), m_screenWrappedFromPrevious.end(), false);
        m_curRow = m_curCol = 0;
        m_curFg = theme::kTerminalText; m_curBg = theme::kTerminalBg; m_curAttrs = 0;
        m_scrollTop = 0; m_scrollBot = m_rows - 1;
    }

    // ---------- input ----------

    void OnLeftDown(wxMouseEvent& evt) {
        SetFocus();
        m_selAnchor = ScreenPointToBufferPos(evt.GetPosition());
        m_selFocus = m_selAnchor;
        m_selecting = true;
        m_hasSelection = false;
        if (!HasCapture()) CaptureMouse();
        Refresh(false);
    }

    void OnLeftUp(wxMouseEvent& evt) {
        if (!m_selecting) {
            evt.Skip();
            return;
        }
        m_selFocus = ScreenPointToBufferPos(evt.GetPosition());
        m_hasSelection = m_selAnchor.line != m_selFocus.line || m_selAnchor.col != m_selFocus.col;
        m_selecting = false;
        if (HasCapture()) ReleaseMouse();
        Refresh(false);
    }

    void OnMouseMove(wxMouseEvent& evt) {
        if (!m_selecting || !evt.LeftIsDown()) {
            evt.Skip();
            return;
        }
        BufferPos next = ScreenPointToBufferPos(evt.GetPosition());
        if (next.line == m_selFocus.line && next.col == m_selFocus.col) return;
        m_selFocus = next;
        m_hasSelection = m_selAnchor.line != m_selFocus.line || m_selAnchor.col != m_selFocus.col;
        Refresh(false);
    }

    void OnMouseWheel(wxMouseEvent& evt) {
        if (evt.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
            evt.Skip();
            return;
        }
        int delta = std::max(1, evt.GetWheelDelta());
        int linesPerAction = evt.GetLinesPerAction() > 0 ? evt.GetLinesPerAction() : 3;
        int steps = std::max(1, std::abs(evt.GetWheelRotation()) / delta) * linesPerAction;
        if (evt.GetWheelRotation() > 0) ScrollViewport(steps);
        else if (evt.GetWheelRotation() < 0) ScrollViewport(-steps);
    }

    void OnKeyDown(wxKeyEvent& evt) {
        int kc = evt.GetKeyCode();
        bool ctrl = evt.ControlDown();
        bool shift = evt.ShiftDown();
        bool alt  = evt.AltDown();

        if (ctrl && !alt) {
            if (shift && (kc == 'C' || kc == 'c' || kc == WXK_INSERT)) { CopySelection(); return; }
            if (shift && (kc == 'V' || kc == 'v')) { PasteClipboard(); return; }
            // Ctrl+-/+/= zoom is handled by the frame-level accelerator so it
            // applies the same zoom to every terminal tab. Don't intercept here.
            if (kc == 'D' || kc == 'd') {
                if (m_closeCallback) {
                    CallAfter([closeCallback = m_closeCallback]() { closeCallback(); });
                    return;
                }
            }
        }

        if (shift && kc == WXK_INSERT) { PasteClipboard(); return; }
        if (shift && kc == WXK_PAGEUP)   { ScrollViewport(std::max(1, m_rows / 2)); return; }
        if (shift && kc == WXK_PAGEDOWN) { ScrollViewport(-std::max(1, m_rows / 2)); return; }

        JumpToLiveView();

        switch (kc) {
        case WXK_UP:        m_pty.Write("\x1b[A", 3); return;
        case WXK_DOWN:      m_pty.Write("\x1b[B", 3); return;
        case WXK_RIGHT:     m_pty.Write("\x1b[C", 3); return;
        case WXK_LEFT:      m_pty.Write("\x1b[D", 3); return;
        case WXK_HOME:      m_pty.Write("\x1b[H", 3); return;
        case WXK_END:       m_pty.Write("\x1b[F", 3); return;
        case WXK_PAGEUP:    m_pty.Write("\x1b[5~", 4); return;
        case WXK_PAGEDOWN:  m_pty.Write("\x1b[6~", 4); return;
        case WXK_DELETE:    m_pty.Write("\x1b[3~", 4); return;
        case WXK_INSERT:    m_pty.Write("\x1b[2~", 4); return;
        case WXK_F1: case WXK_F2: case WXK_F3: case WXK_F4: {
            char buf[3] = { 0x1b, 'O', (char)('P' + (kc - WXK_F1)) };
            m_pty.Write(buf, 3);
            return;
        }
        }

        if (ctrl && !alt) {
            // Ctrl + letter → 0x01..0x1A. Captures Ctrl+C/D/L/R/A/E/U/W/K, etc.
            if (kc >= 'A' && kc <= 'Z') { char c = (char)(kc - 'A' + 1); m_pty.Write(&c, 1); return; }
            if (kc >= 'a' && kc <= 'z') { char c = (char)(kc - 'a' + 1); m_pty.Write(&c, 1); return; }
            if (kc == ' ' || kc == '@') { char c = 0;     m_pty.Write(&c, 1); return; }
            if (kc == '[')              { char c = 0x1b; m_pty.Write(&c, 1); return; }
            if (kc == '\\')             { char c = 0x1c; m_pty.Write(&c, 1); return; }
            if (kc == ']')              { char c = 0x1d; m_pty.Write(&c, 1); return; }
            if (kc == '_' || kc == '?') { char c = 0x1f; m_pty.Write(&c, 1); return; }
        }

        evt.Skip();
    }

    void OnChar(wxKeyEvent& evt) {
        int kc = evt.GetKeyCode();
        wxChar uni = evt.GetUnicodeKey();

        JumpToLiveView();

        if (kc == WXK_RETURN || kc == WXK_NUMPAD_ENTER) { m_pty.Write("\r", 1); return; }
        if (kc == WXK_BACK)  { m_pty.Write("\x7f", 1); return; }
        if (kc == WXK_TAB)   { m_pty.Write("\t", 1);   return; }
        if (kc == WXK_ESCAPE){ m_pty.Write("\x1b", 1); return; }

        if (uni != WXK_NONE && uni >= 32 && uni != 127) {
            wxString s(uni);
            wxScopedCharBuffer u8 = s.ToUTF8();
            m_pty.Write(u8.data(), u8.length());
            return;
        }
        evt.Skip();
    }

    PtySession m_pty;
    wxTimer    m_timer;
    wxFont     m_font;
    int        m_zoomDelta = 0;
    bool       m_running = false;

    int m_charW = 0, m_charH = 0;
    int m_textOffsetY = 0;
    int m_cols  = 80, m_rows = 24;
    int m_curRow = 0, m_curCol = 0;
    int m_savedRow = 0, m_savedCol = 0;
    int m_scrollTop = 0, m_scrollBot = 23;
    int m_viewOffset = 0;
    int m_pendingCursorPhysicalIndex = 0;
    int m_pendingCursorCol = 0;

    uint32_t m_curFg = theme::kTerminalText;
    uint32_t m_curBg = theme::kTerminalBg;
    uint8_t  m_curAttrs = 0;

    State        m_state = State::Ground;
    std::string  m_paramBuf;
    bool         m_csiQMark = false;
    int          m_utf8Remaining = 0;
    uint32_t     m_utf8Codepoint = 0;

    bool m_selecting = false;
    bool m_hasSelection = false;
    BufferPos m_selAnchor;
    BufferPos m_selFocus;
    std::function<void()> m_closeCallback;
    std::function<void(const GraphRequest&)> m_graphRequestCallback;
    std::function<void(const KumaPlotRequest&)> m_kumaPlotRequestCallback;
    std::vector<wxString> m_commandHistory;
    bool m_suppressNextInputRecord = false;

    std::deque<PhysicalLine> m_history;
    std::vector<bool> m_screenWrappedFromPrevious = std::vector<bool>((size_t)m_rows, false);
    std::vector<Cell> m_screen;
};

// ============================================================
// GraphCanvas: draws sampled NEKO graphs inside a plot viewport.
// ============================================================
class GraphCanvas : public wxWindow {
public:
    GraphCanvas(wxWindow* parent)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxBORDER_NONE),
          m_resampleTimer(this) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ColourFromRgb(theme::kPanelBg));

        Bind(wxEVT_PAINT, &GraphCanvas::OnPaint, this);
        Bind(wxEVT_SIZE, &GraphCanvas::OnSize, this);
        Bind(wxEVT_MOUSEWHEEL, &GraphCanvas::OnMouseWheel, this);
        Bind(wxEVT_LEFT_DOWN, &GraphCanvas::OnLeftDown, this);
        Bind(wxEVT_LEFT_UP, &GraphCanvas::OnLeftUp, this);
        Bind(wxEVT_MOTION, &GraphCanvas::OnMouseMove, this);
        Bind(wxEVT_TIMER, &GraphCanvas::OnResampleTimer, this);
        Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&){});
        ResetView();
    }

    ~GraphCanvas() override {
        for (auto& graph : m_graphs) FreeSamples(graph);
    }

    bool AddGraph(const GraphRequest& request) {
        wxScopedCharBuffer serial = request.serialized.ToUTF8();
        NekoExpr* expr = nekoDeserializeExpr(serial.data());
        if (!expr) return false;

        Graph graph;
        graph.kind = request.kind;
        graph.expr = expr;
        graph.label = request.label.empty() ? "graph" : request.label;
        graph.request = request;
        graph.request.label = graph.label;
        graph.colour = NextColour(m_graphs.size());
        m_graphs.push_back(graph);
        ResampleAll();
        if (m_graphsChanged) m_graphsChanged();
        Refresh(false);
        return true;
    }

    void RemoveGraph(size_t index) {
        if (index >= m_graphs.size()) return;
        FreeSamples(m_graphs[index]);
        m_graphs.erase(m_graphs.begin() + (ptrdiff_t)index);
        if (m_graphsChanged) m_graphsChanged();
        Refresh(false);
    }

    void ResetView() {
        m_freeAxisAspect = false;
        m_viewInitialized = true;
        SetSquareView(0.0L, 0.0L, 10.0L, PlotRect());
        ResampleAll();
        Refresh(false);
    }

    size_t GraphCount() const { return m_graphs.size(); }

    wxString GraphLabel(size_t index) const {
        return index < m_graphs.size() ? m_graphs[index].label : wxString();
    }

    wxColour GraphColour(size_t index) const {
        return index < m_graphs.size() ? m_graphs[index].colour : ColourFromRgb(theme::kAccent);
    }

    std::vector<GraphRequest> GraphRequests() const {
        std::vector<GraphRequest> requests;
        requests.reserve(m_graphs.size());
        for (const auto& graph : m_graphs) requests.push_back(graph.request);
        return requests;
    }

    void SetGraphsChangedCallback(std::function<void()> callback) {
        m_graphsChanged = std::move(callback);
    }

private:
    struct Graph {
        GraphRequestKind kind = GraphRequestKind::Explicit;
        NekoExpr* expr = nullptr;
        wxString label;
        GraphRequest request;
        wxColour colour;
        NekoExplicitGraphSample explicitSample = {};
        NekoImplicitGraphSample implicitSample = {};
        bool samplesReady = false;
    };

    enum class DragMode {
        None,
        Pan,
        ScaleX,
        ScaleY
    };

    static wxColour NextColour(size_t index) {
        static const uint32_t colours[] = {
            0xE06C75u, 0x61AFEFu, 0x98C379u, 0xE5C07Bu,
            0xC678DDu, 0x56B6C2u, 0xD19A66u, 0xABB2BFu
        };
        return ColourFromRgb(colours[index % (sizeof(colours) / sizeof(colours[0]))]);
    }

    wxRect PlotRect() const {
        wxSize size = GetClientSize();
        int left = 56;
        int top = 20;
        int right = 24;
        int bottom = 44;
        return wxRect(left, top,
                      std::max(1, size.x - left - right),
                      std::max(1, size.y - top - bottom));
    }

    long double PlotAspect(const wxRect& plot) const {
        if (plot.width <= 0 || plot.height <= 0) return 1.0L;
        long double aspect = (long double)plot.width / (long double)plot.height;
        return isfinite(aspect) && aspect > 0.0L ? aspect : 1.0L;
    }

    void SetSquareView(long double centerX, long double centerY, long double yHalfSpan, const wxRect& plot) {
        if (!isfinite(yHalfSpan) || yHalfSpan <= 0.0L) yHalfSpan = 10.0L;
        long double xHalfSpan = yHalfSpan * PlotAspect(plot);
        m_xMin = centerX - xHalfSpan;
        m_xMax = centerX + xHalfSpan;
        m_yMin = centerY - yHalfSpan;
        m_yMax = centerY + yHalfSpan;
    }

    void PreserveSquareScale(const wxRect& plot) {
        long double centerX = (m_xMin + m_xMax) / 2.0L;
        long double centerY = (m_yMin + m_yMax) / 2.0L;
        long double yHalfSpan = (m_yMax - m_yMin) / 2.0L;
        SetSquareView(centerX, centerY, yHalfSpan, plot);
    }

    double WorldToScreenX(long double x, const wxRect& plot) const {
        long double t = (x - m_xMin) / (m_xMax - m_xMin);
        return plot.x + (double)t * plot.width;
    }

    double WorldToScreenY(long double y, const wxRect& plot) const {
        long double t = (y - m_yMin) / (m_yMax - m_yMin);
        return plot.y + plot.height - (double)t * plot.height;
    }

    long double ScreenToWorldX(int sx, const wxRect& plot) const {
        long double t = (long double)(sx - plot.x) / (long double)std::max(1, plot.width);
        return m_xMin + t * (m_xMax - m_xMin);
    }

    long double ScreenToWorldY(int sy, const wxRect& plot) const {
        long double t = (long double)(plot.y + plot.height - sy) / (long double)std::max(1, plot.height);
        return m_yMin + t * (m_yMax - m_yMin);
    }

    long double NiceTick(long double span) const {
        if (!isfinite(span) || span <= 0.0L) return 1.0L;
        long double raw = span / 10.0L;
        long double mag = powl(10.0L, floorl(log10l(raw)));
        long double scaled = raw / mag;
        if (scaled < 1.5L) return mag;
        if (scaled < 3.5L) return 2.0L * mag;
        if (scaled < 7.5L) return 5.0L * mag;
        return 10.0L * mag;
    }

    bool HitXAxis(const wxPoint& pt, const wxRect& plot) const {
        if (!(m_yMin <= 0.0L && m_yMax >= 0.0L)) return false;
        double sy = WorldToScreenY(0.0L, plot);
        return pt.x >= plot.x && pt.x <= plot.x + plot.width
            && std::abs(pt.y - sy) <= 7;
    }

    bool HitYAxis(const wxPoint& pt, const wxRect& plot) const {
        if (!(m_xMin <= 0.0L && m_xMax >= 0.0L)) return false;
        double sx = WorldToScreenX(0.0L, plot);
        return pt.y >= plot.y && pt.y <= plot.y + plot.height
            && std::abs(pt.x - sx) <= 7;
    }

    DragMode AxisDragMode(const wxPoint& pt, const wxRect& plot) const {
        bool hitX = HitXAxis(pt, plot);
        bool hitY = HitYAxis(pt, plot);
        if (hitX && hitY) {
            double sx = WorldToScreenX(0.0L, plot);
            double sy = WorldToScreenY(0.0L, plot);
            return std::abs(pt.y - sy) <= std::abs(pt.x - sx)
                ? DragMode::ScaleX
                : DragMode::ScaleY;
        }
        if (hitX) return DragMode::ScaleX;
        if (hitY) return DragMode::ScaleY;
        return DragMode::Pan;
    }

    void UpdateAxisCursor(const wxPoint& pt) {
        wxRect plot = PlotRect();
        DragMode mode = AxisDragMode(pt, plot);
        if (mode == DragMode::ScaleX) SetCursor(wxCursor(wxCURSOR_SIZEWE));
        else if (mode == DragMode::ScaleY) SetCursor(wxCursor(wxCURSOR_SIZENS));
        else SetCursor(wxCursor(wxCURSOR_ARROW));
    }

    void FreeSamples(Graph& graph) {
        nekoFreeExplicitGraphSample(graph.explicitSample);
        nekoFreeImplicitGraphSample(graph.implicitSample);
        graph.explicitSample = {};
        graph.implicitSample = {};
        graph.samplesReady = false;
    }

    void ResampleAll() {
        wxRect plot = PlotRect();
        size_t explicitSamples = (size_t)std::max(160, plot.width * 2);
        size_t xsteps = (size_t)std::clamp(plot.width / 5, 40, 180);
        size_t ysteps = (size_t)std::clamp(plot.height / 5, 40, 180);

        for (auto& graph : m_graphs) {
            FreeSamples(graph);
            if (!graph.expr) continue;
            if (graph.kind == GraphRequestKind::Explicit) {
                graph.explicitSample = nekoSampleExplicitGraph(graph.expr, m_xMin, m_xMax, explicitSamples);
            } else if (graph.kind == GraphRequestKind::Implicit) {
                graph.implicitSample = nekoSampleImplicitGraph(graph.expr, m_xMin, m_xMax, m_yMin, m_yMax, xsteps, ysteps);
            }
            graph.samplesReady = true;
        }
        m_resamplePending = false;
    }

    void ScheduleResample() {
        m_resamplePending = true;
        m_resampleTimer.StartOnce(160);
    }

    void DrawGrid(wxGraphicsContext* gc, const wxRect& plot) {
        long double xTick = NiceTick(m_xMax - m_xMin);
        long double yTick = NiceTick(m_yMax - m_yMin);
        wxFont font(wxFontInfo(9).Family(wxFONTFAMILY_DEFAULT));
        std::vector<std::pair<long double, double>> xTicks;
        std::vector<std::pair<long double, double>> yTicks;

        gc->SetPen(wxPen(ColourFromRgb(0x243040u), 1));
        gc->SetFont(font, ColourFromRgb(theme::kMutedText));
        for (long double x = ceill(m_xMin / xTick) * xTick; x <= m_xMax; x += xTick) {
            double sx = WorldToScreenX(x, plot);
            xTicks.push_back({x, sx});
            gc->StrokeLine(sx, plot.y, sx, plot.y + plot.height);
            wxString label = wxString::Format("%Lg", x);
            wxDouble width = 0.0;
            wxDouble height = 0.0;
            gc->GetTextExtent(label, &width, &height);
            gc->DrawText(label, sx - width / 2.0, plot.y + plot.height + 6);
        }
        for (long double y = ceill(m_yMin / yTick) * yTick; y <= m_yMax; y += yTick) {
            double sy = WorldToScreenY(y, plot);
            yTicks.push_back({y, sy});
            gc->StrokeLine(plot.x, sy, plot.x + plot.width, sy);
            wxString label = wxString::Format("%Lg", y);
            wxDouble width = 0.0;
            wxDouble height = 0.0;
            gc->GetTextExtent(label, &width, &height);
            gc->DrawText(label, plot.x - width - 8, sy - height / 2.0);
        }

        gc->SetPen(wxPen(ColourFromRgb(theme::kMutedText), 2));
        if (m_yMin <= 0.0L && m_yMax >= 0.0L) {
            double sy = WorldToScreenY(0.0L, plot);
            gc->StrokeLine(plot.x, sy, plot.x + plot.width, sy);
            gc->StrokeLine(plot.x + plot.width, sy, plot.x + plot.width - 9, sy - 5);
            gc->StrokeLine(plot.x + plot.width, sy, plot.x + plot.width - 9, sy + 5);
            gc->DrawText("x", plot.x + plot.width - 12, sy + 8);
            gc->SetPen(wxPen(ColourFromRgb(theme::kText), 1));
            for (const auto& tick : xTicks) {
                gc->StrokeLine(tick.second, sy - 4, tick.second, sy + 4);
            }
            gc->SetPen(wxPen(ColourFromRgb(theme::kMutedText), 2));
        }
        if (m_xMin <= 0.0L && m_xMax >= 0.0L) {
            double sx = WorldToScreenX(0.0L, plot);
            gc->StrokeLine(sx, plot.y + plot.height, sx, plot.y);
            gc->StrokeLine(sx, plot.y, sx - 5, plot.y + 9);
            gc->StrokeLine(sx, plot.y, sx + 5, plot.y + 9);
            gc->DrawText("y", sx + 8, plot.y + 4);
            gc->SetPen(wxPen(ColourFromRgb(theme::kText), 1));
            for (const auto& tick : yTicks) {
                gc->StrokeLine(sx - 4, tick.second, sx + 4, tick.second);
            }
            gc->SetPen(wxPen(ColourFromRgb(theme::kMutedText), 2));
        }
    }

    void DrawExplicit(wxGraphicsContext* gc, const Graph& graph, const wxRect& plot) {
        if (!graph.explicitSample.points || graph.explicitSample.count < 2) return;
        gc->SetPen(wxPen(graph.colour, 2));
        bool havePrev = false;
        double px = 0.0;
        double py = 0.0;
        for (size_t i = 0; i < graph.explicitSample.count; i++) {
            const NekoGraphPoint& point = graph.explicitSample.points[i];
            double sx = WorldToScreenX(point.x, plot);
            double sy = WorldToScreenY(point.y, plot);
            bool valid = point.valid && sy > plot.y - plot.height * 4 && sy < plot.y + plot.height * 5;
            if (valid && havePrev && std::abs(sy - py) < plot.height * 2)
                gc->StrokeLine(px, py, sx, sy);
            havePrev = valid;
            px = sx;
            py = sy;
        }
    }

    void DrawImplicit(wxGraphicsContext* gc, const Graph& graph, const wxRect& plot) {
        if (!graph.implicitSample.segments) return;
        gc->SetPen(wxPen(graph.colour, 2));
        for (size_t i = 0; i < graph.implicitSample.count; i++) {
            const NekoGraphSegment& s = graph.implicitSample.segments[i];
            gc->StrokeLine(WorldToScreenX(s.x1, plot), WorldToScreenY(s.y1, plot),
                           WorldToScreenX(s.x2, plot), WorldToScreenY(s.y2, plot));
        }
    }

    void DrawVertical(wxGraphicsContext* gc, const Graph& graph, const wxRect& plot) {
        if (!graph.expr || graph.expr->kind != NEKO_EXPR_CONST) return;
        long double x = graph.expr->as.constant;
        if (x < m_xMin || x > m_xMax) return;
        double sx = WorldToScreenX(x, plot);
        gc->SetPen(wxPen(graph.colour, 2));
        gc->StrokeLine(sx, plot.y, sx, plot.y + plot.height);
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(ColourFromRgb(theme::kPanelBg)));
        dc.Clear();

        wxRect plot = PlotRect();
        dc.SetBrush(wxBrush(ColourFromRgb(theme::kTerminalBg)));
        dc.SetPen(wxPen(ColourFromRgb(theme::kBorder), 1));
        dc.DrawRectangle(plot);

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        DrawGrid(gc.get(), plot);

        gc->Clip(plot.x, plot.y, plot.width, plot.height);
        for (const auto& graph : m_graphs) {
            if (graph.kind == GraphRequestKind::Explicit) DrawExplicit(gc.get(), graph, plot);
            else if (graph.kind == GraphRequestKind::Implicit) DrawImplicit(gc.get(), graph, plot);
            else DrawVertical(gc.get(), graph, plot);
        }
        gc->ResetClip();

        if (m_resamplePending) {
            wxFont font(wxFontInfo(9).Family(wxFONTFAMILY_DEFAULT));
            gc->SetFont(font, ColourFromRgb(theme::kMutedText));
            gc->DrawText("resampling...", plot.x + 12, plot.y + 10);
        }
    }

    void OnMouseWheel(wxMouseEvent& evt) {
        if (evt.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
            evt.Skip();
            return;
        }
        wxRect plot = PlotRect();
        wxPoint pt = evt.GetPosition();
        long double anchorX = ScreenToWorldX(pt.x, plot);
        long double anchorY = ScreenToWorldY(pt.y, plot);
        long double factor = evt.GetWheelRotation() > 0 ? 0.85L : 1.0L / 0.85L;

        m_xMin = anchorX + (m_xMin - anchorX) * factor;
        m_xMax = anchorX + (m_xMax - anchorX) * factor;
        m_yMin = anchorY + (m_yMin - anchorY) * factor;
        m_yMax = anchorY + (m_yMax - anchorY) * factor;
        ScheduleResample();
        Refresh(false);
    }

    void OnLeftDown(wxMouseEvent& evt) {
        SetFocus();
        wxRect plot = PlotRect();
        m_dragMode = AxisDragMode(evt.GetPosition(), plot);
        m_dragging = true;
        m_dragStart = evt.GetPosition();
        m_dragXMin = m_xMin;
        m_dragXMax = m_xMax;
        m_dragYMin = m_yMin;
        m_dragYMax = m_yMax;
        if (m_dragMode == DragMode::ScaleX || m_dragMode == DragMode::ScaleY)
            m_freeAxisAspect = true;
        if (!HasCapture()) CaptureMouse();
    }

    void OnLeftUp(wxMouseEvent&) {
        if (!m_dragging) return;
        m_dragging = false;
        m_dragMode = DragMode::None;
        if (HasCapture()) ReleaseMouse();
        ScheduleResample();
    }

    void OnMouseMove(wxMouseEvent& evt) {
        if (!m_dragging || !evt.LeftIsDown()) {
            UpdateAxisCursor(evt.GetPosition());
            evt.Skip();
            return;
        }
        wxRect plot = PlotRect();
        wxPoint pt = evt.GetPosition();
        if (m_dragMode == DragMode::ScaleX) {
            long double center = (m_dragXMin + m_dragXMax) / 2.0L;
            long double span = m_dragXMax - m_dragXMin;
            long double factor = expl(-(long double)(pt.x - m_dragStart.x) / 160.0L);
            factor = std::clamp(factor, 0.05L, 20.0L);
            m_xMin = center - span * factor / 2.0L;
            m_xMax = center + span * factor / 2.0L;
            SetCursor(wxCursor(wxCURSOR_SIZEWE));
        } else if (m_dragMode == DragMode::ScaleY) {
            long double center = (m_dragYMin + m_dragYMax) / 2.0L;
            long double span = m_dragYMax - m_dragYMin;
            long double factor = expl((long double)(pt.y - m_dragStart.y) / 160.0L);
            factor = std::clamp(factor, 0.05L, 20.0L);
            m_yMin = center - span * factor / 2.0L;
            m_yMax = center + span * factor / 2.0L;
            SetCursor(wxCursor(wxCURSOR_SIZENS));
        } else {
            long double dx = (long double)(pt.x - m_dragStart.x) / (long double)std::max(1, plot.width) * (m_dragXMax - m_dragXMin);
            long double dy = (long double)(pt.y - m_dragStart.y) / (long double)std::max(1, plot.height) * (m_dragYMax - m_dragYMin);
            m_xMin = m_dragXMin - dx;
            m_xMax = m_dragXMax - dx;
            m_yMin = m_dragYMin + dy;
            m_yMax = m_dragYMax + dy;
        }
        ScheduleResample();
        Refresh(false);
    }

    void OnSize(wxSizeEvent& evt) {
        if (!m_freeAxisAspect) {
            wxRect plot = PlotRect();
            if (!m_viewInitialized) {
                SetSquareView(0.0L, 0.0L, 10.0L, plot);
                m_viewInitialized = true;
            } else {
                PreserveSquareScale(plot);
            }
            ResampleAll();
        }
        Refresh(false);
        evt.Skip();
    }

    void OnResampleTimer(wxTimerEvent&) {
        ResampleAll();
        Refresh(false);
    }

    std::vector<Graph> m_graphs;
    wxTimer m_resampleTimer;
    bool m_resamplePending = false;
    bool m_dragging = false;
    DragMode m_dragMode = DragMode::None;
    wxPoint m_dragStart;
    bool m_freeAxisAspect = false;
    bool m_viewInitialized = false;
    long double m_dragXMin = -10.0L;
    long double m_dragXMax = 10.0L;
    long double m_dragYMin = -10.0L;
    long double m_dragYMax = 10.0L;
    long double m_xMin = -10.0L;
    long double m_xMax = 10.0L;
    long double m_yMin = -10.0L;
    long double m_yMax = 10.0L;
    std::function<void()> m_graphsChanged;
};

class GraphPage : public wxPanel {
public:
    GraphPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY) {
        StyleDarkWindow(this, theme::kPanelBg);
        auto* root = new wxBoxSizer(wxHORIZONTAL);
        m_canvas = new GraphCanvas(this);

        auto* sidePanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(240, -1));
        StyleDarkWindow(sidePanel, theme::kTabBg);
        auto* sideSizer = new wxBoxSizer(wxVERTICAL);
        sidePanel->SetSizer(sideSizer);

        m_side = new wxScrolledWindow(sidePanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxVSCROLL | wxBORDER_NONE);
        StyleDarkWindow(m_side, theme::kTabBg);
        m_side->SetScrollRate(8, 8);
        m_sideSizer = new wxBoxSizer(wxVERTICAL);
        m_side->SetSizer(m_sideSizer);
        sideSizer->Add(m_side, 1, wxEXPAND);

        auto* resetRow = new wxBoxSizer(wxHORIZONTAL);
        resetRow->AddStretchSpacer(1);
        auto* reset = new wxButton(sidePanel, wxID_ANY, "Reset", wxDefaultPosition, wxSize(86, 30));
        StyleDarkButton(reset, true);
        reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_canvas->ResetView(); });
        resetRow->Add(reset, 0, wxALL, 10);
        sideSizer->Add(resetRow, 0, wxEXPAND);

        root->Add(m_canvas, 1, wxEXPAND);
        root->Add(sidePanel, 0, wxEXPAND);
        SetSizer(root);
        m_canvas->SetGraphsChangedCallback([this]() { RebuildList(); });
        RebuildList();
    }

    bool AddGraph(const GraphRequest& request) {
        return m_canvas->AddGraph(request);
    }

    std::vector<GraphRequest> GraphRequests() const {
        return m_canvas->GraphRequests();
    }

private:
    void RebuildList() {
        m_sideSizer->Clear(true);
        auto* title = new wxStaticText(m_side, wxID_ANY, "Graphs");
        StyleDarkLabel(title);
        wxFont titleFont = title->GetFont();
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        title->SetFont(titleFont);
        m_sideSizer->Add(title, 0, wxALL, 12);

        if (m_canvas->GraphCount() == 0) {
            auto* empty = new wxStaticText(m_side, wxID_ANY, "No graphs");
            StyleDarkLabel(empty, true);
            m_sideSizer->Add(empty, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
        }

        for (size_t i = 0; i < m_canvas->GraphCount(); i++) {
            auto* row = new wxPanel(m_side);
            StyleDarkWindow(row, theme::kPanelRaisedBg);
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);
            auto* swatch = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(10, 22));
            swatch->SetBackgroundColour(m_canvas->GraphColour(i));
            auto* label = new wxStaticText(row, wxID_ANY, m_canvas->GraphLabel(i));
            StyleDarkLabel(label);
            auto* close = new wxButton(row, wxID_ANY, "x", wxDefaultPosition, wxSize(24, 24), wxBU_EXACTFIT);
            StyleDarkButton(close);
            close->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) { m_canvas->RemoveGraph(i); });
            rowSizer->Add(swatch, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
            rowSizer->Add(label, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);
            rowSizer->Add(close, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
            row->SetSizer(rowSizer);
            m_sideSizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
        }

        m_side->FitInside();
        m_side->Layout();
    }

    GraphCanvas* m_canvas = nullptr;
    wxScrolledWindow* m_side = nullptr;
    wxBoxSizer* m_sideSizer = nullptr;
};

// ============================================================
// KumaPlotCanvas: draws KUMA plot data in a primitive viewport.
// ============================================================
class KumaPlotCanvas : public wxWindow {
public:
    KumaPlotCanvas(wxWindow* parent)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                   wxBORDER_NONE) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ColourFromRgb(theme::kPanelBg));

        Bind(wxEVT_PAINT, &KumaPlotCanvas::OnPaint, this);
        Bind(wxEVT_MOUSEWHEEL, &KumaPlotCanvas::OnMouseWheel, this);
        Bind(wxEVT_LEFT_DOWN, &KumaPlotCanvas::OnLeftDown, this);
        Bind(wxEVT_LEFT_UP, &KumaPlotCanvas::OnLeftUp, this);
        Bind(wxEVT_MOTION, &KumaPlotCanvas::OnMouseMove, this);
        Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&){});
    }

    void SetPlot(const KumaPlotRequest& request, const wxString& tabTitle) {
        m_request = request;
        m_kind = request.kind;
        m_title = request.title.empty() ? tabTitle : request.title;
        m_xLabel = request.xLabel.empty() ? "x" : request.xLabel;
        m_yLabel = request.yLabel.empty() ? "y" : request.yLabel;
        ParsePayload(request.payload);
        ResetView();
    }

    KumaPlotRequest PlotRequest() const {
        return m_request;
    }

    void ResetView() {
        ComputeBounds();
        Refresh(false);
    }

private:
    struct Point {
        long double x = 0.0L;
        long double y = 0.0L;
    };

    struct HistogramBin {
        long double lower = 0.0L;
        long double upper = 0.0L;
        long double midpoint = 0.0L;
        long double count = 0.0L;
        long double proportion = 0.0L;
        long double density = 0.0L;
    };

    struct Bar {
        long double label = 0.0L;
        long double height = 0.0L;
    };

    struct Box {
        long double minimum = 0.0L;
        long double q1 = 0.0L;
        long double median = 0.0L;
        long double q3 = 0.0L;
        long double maximum = 0.0L;
        long double lowerFence = 0.0L;
        long double upperFence = 0.0L;
        long double lowerWhisker = 0.0L;
        long double upperWhisker = 0.0L;
        std::vector<long double> outliers;
        bool valid = false;
    };

    enum class DragMode {
        None,
        Pan,
        ScaleX,
        ScaleY
    };

    static std::vector<std::string> SplitString(const std::string& text, char sep) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (start <= text.size()) {
            size_t at = text.find(sep, start);
            if (at == std::string::npos) {
                parts.push_back(text.substr(start));
                break;
            }
            parts.push_back(text.substr(start, at - start));
            start = at + 1;
        }
        return parts;
    }

    static bool ParseLongDouble(const std::string& text, long double& out) {
        char* end = nullptr;
        out = strtold(text.c_str(), &end);
        return end && *end == '\0' && isfinite(out);
    }

    wxRect PlotRect() const {
        wxSize size = GetClientSize();
        int left = 72;
        int top = 58;
        int right = 28;
        int bottom = 76;
        return wxRect(left, top,
                      std::max(1, size.x - left - right),
                      std::max(1, size.y - top - bottom));
    }

    double WorldToScreenX(long double x, const wxRect& plot) const {
        long double t = (x - m_xMin) / (m_xMax - m_xMin);
        return plot.x + (double)t * plot.width;
    }

    double WorldToScreenY(long double y, const wxRect& plot) const {
        long double t = (y - m_yMin) / (m_yMax - m_yMin);
        return plot.y + plot.height - (double)t * plot.height;
    }

    long double ScreenToWorldX(int sx, const wxRect& plot) const {
        long double t = (long double)(sx - plot.x) / (long double)std::max(1, plot.width);
        return m_xMin + t * (m_xMax - m_xMin);
    }

    long double ScreenToWorldY(int sy, const wxRect& plot) const {
        long double t = (long double)(plot.y + plot.height - sy) / (long double)std::max(1, plot.height);
        return m_yMin + t * (m_yMax - m_yMin);
    }

    long double NiceTick(long double span) const {
        if (!isfinite(span) || span <= 0.0L) return 1.0L;
        long double raw = span / 8.0L;
        long double mag = powl(10.0L, floorl(log10l(raw)));
        long double scaled = raw / mag;
        if (scaled < 1.5L) return mag;
        if (scaled < 3.5L) return 2.0L * mag;
        if (scaled < 7.5L) return 5.0L * mag;
        return 10.0L * mag;
    }

    long double XAxisWorldY() const {
        return (m_yMin <= 0.0L && m_yMax >= 0.0L) ? 0.0L : m_yMin;
    }

    long double YAxisWorldX() const {
        return (m_xMin <= 0.0L && m_xMax >= 0.0L) ? 0.0L : m_xMin;
    }

    bool HitXAxis(const wxPoint& pt, const wxRect& plot) const {
        double sy = WorldToScreenY(XAxisWorldY(), plot);
        return pt.x >= plot.x && pt.x <= plot.x + plot.width
            && std::abs(pt.y - sy) <= 7;
    }

    bool HitYAxis(const wxPoint& pt, const wxRect& plot) const {
        double sx = WorldToScreenX(YAxisWorldX(), plot);
        return pt.y >= plot.y && pt.y <= plot.y + plot.height
            && std::abs(pt.x - sx) <= 7;
    }

    DragMode AxisDragMode(const wxPoint& pt, const wxRect& plot) const {
        bool hitX = HitXAxis(pt, plot);
        bool hitY = HitYAxis(pt, plot);
        if (hitX && hitY) {
            double sx = WorldToScreenX(YAxisWorldX(), plot);
            double sy = WorldToScreenY(XAxisWorldY(), plot);
            return std::abs(pt.y - sy) <= std::abs(pt.x - sx)
                ? DragMode::ScaleX
                : DragMode::ScaleY;
        }
        if (hitX) return DragMode::ScaleX;
        if (hitY) return DragMode::ScaleY;
        return DragMode::Pan;
    }

    void UpdateAxisCursor(const wxPoint& pt) {
        wxRect plot = PlotRect();
        DragMode mode = AxisDragMode(pt, plot);
        if (mode == DragMode::ScaleX) SetCursor(wxCursor(wxCURSOR_SIZEWE));
        else if (mode == DragMode::ScaleY) SetCursor(wxCursor(wxCURSOR_SIZENS));
        else SetCursor(wxCursor(wxCURSOR_ARROW));
    }

    void IncludeBounds(long double x, long double y, bool& have,
                       long double& xMin, long double& xMax,
                       long double& yMin, long double& yMax) {
        if (!isfinite(x) || !isfinite(y)) return;
        if (!have) {
            xMin = xMax = x;
            yMin = yMax = y;
            have = true;
            return;
        }
        xMin = std::min(xMin, x);
        xMax = std::max(xMax, x);
        yMin = std::min(yMin, y);
        yMax = std::max(yMax, y);
    }

    bool ForceNonnegativeY() const {
        return m_kind == KumaPlotRequestKind::Histogram
            || m_kind == KumaPlotRequestKind::Frequency
            || m_kind == KumaPlotRequestKind::Bar
            || m_kind == KumaPlotRequestKind::Density
            || m_kind == KumaPlotRequestKind::Dot
            || m_kind == KumaPlotRequestKind::ECDF;
    }

    void ComputeBounds() {
        bool have = false;
        long double xMin = 0.0L;
        long double xMax = 1.0L;
        long double yMin = 0.0L;
        long double yMax = 1.0L;

        if (m_kind == KumaPlotRequestKind::Box && m_box.valid) {
            IncludeBounds(1.0L, m_box.minimum, have, xMin, xMax, yMin, yMax);
            IncludeBounds(1.0L, m_box.maximum, have, xMin, xMax, yMin, yMax);
            for (long double outlier : m_box.outliers)
                IncludeBounds(1.0L, outlier, have, xMin, xMax, yMin, yMax);
            xMin = 0.0L;
            xMax = 2.0L;
        }

        for (const auto& bin : m_bins) {
            IncludeBounds(bin.lower, 0.0L, have, xMin, xMax, yMin, yMax);
            IncludeBounds(bin.upper, bin.count, have, xMin, xMax, yMin, yMax);
        }
        for (size_t i = 0; i < m_bars.size(); i++) {
            IncludeBounds((long double)i, 0.0L, have, xMin, xMax, yMin, yMax);
            IncludeBounds((long double)i + 1.0L, m_bars[i].height, have, xMin, xMax, yMin, yMax);
        }
        for (const auto& point : m_points)
            IncludeBounds(point.x, point.y, have, xMin, xMax, yMin, yMax);
        if (m_lineValid) {
            IncludeBounds(m_lineStart.x, m_lineStart.y, have, xMin, xMax, yMin, yMax);
            IncludeBounds(m_lineEnd.x, m_lineEnd.y, have, xMin, xMax, yMin, yMax);
        }

        if (!have) {
            xMin = 0.0L;
            xMax = 1.0L;
            yMin = ForceNonnegativeY() ? 0.0L : -1.0L;
            yMax = 1.0L;
        }

        long double xPad = (xMax - xMin) * 0.08L;
        long double yPad = (yMax - yMin) * 0.12L;
        if (!isfinite(xPad) || xPad <= 0.0L) xPad = 1.0L;
        if (!isfinite(yPad) || yPad <= 0.0L) yPad = 1.0L;
        m_xMin = xMin - xPad;
        m_xMax = xMax + xPad;
        if (ForceNonnegativeY()) {
            m_yMin = 0.0L;
            m_yMax = yMax + yPad;
            if (m_yMax <= 0.0L) m_yMax = 1.0L;
        } else {
            m_yMin = yMin - yPad;
            m_yMax = yMax + yPad;
            if (m_yMin == m_yMax) {
                m_yMin -= 1.0L;
                m_yMax += 1.0L;
            }
        }
    }

    void ParsePayload(const wxString& payload) {
        m_points.clear();
        m_bins.clear();
        m_bars.clear();
        m_box = Box{};
        m_lineValid = false;

        wxScopedCharBuffer raw = payload.ToUTF8();
        std::string text = raw.data() ? raw.data() : "";
        if (m_kind == KumaPlotRequestKind::Box) {
            std::vector<std::string> parts = SplitString(text, '|');
            std::vector<std::string> fields = parts.empty() ? std::vector<std::string>() : SplitString(parts[0], ',');
            if (fields.size() >= 9
                    && ParseLongDouble(fields[0], m_box.minimum)
                    && ParseLongDouble(fields[1], m_box.q1)
                    && ParseLongDouble(fields[2], m_box.median)
                    && ParseLongDouble(fields[3], m_box.q3)
                    && ParseLongDouble(fields[4], m_box.maximum)
                    && ParseLongDouble(fields[5], m_box.lowerFence)
                    && ParseLongDouble(fields[6], m_box.upperFence)
                    && ParseLongDouble(fields[7], m_box.lowerWhisker)
                    && ParseLongDouble(fields[8], m_box.upperWhisker)) {
                m_box.valid = true;
            }
            if (parts.size() > 1 && !parts[1].empty()) {
                for (const std::string& item : SplitString(parts[1], ';')) {
                    long double value = 0.0L;
                    if (ParseLongDouble(item, value)) m_box.outliers.push_back(value);
                }
            }
            return;
        }

        if (m_kind == KumaPlotRequestKind::Regression) {
            std::vector<std::string> parts = SplitString(text, '|');
            if (!parts.empty()) {
                std::vector<std::string> line = SplitString(parts[0], ',');
                if (line.size() >= 4
                        && ParseLongDouble(line[0], m_lineStart.x)
                        && ParseLongDouble(line[1], m_lineStart.y)
                        && ParseLongDouble(line[2], m_lineEnd.x)
                        && ParseLongDouble(line[3], m_lineEnd.y)) {
                    m_lineValid = true;
                }
            }
            text = parts.size() > 1 ? parts[1] : "";
        }

        for (const std::string& item : SplitString(text, ';')) {
            if (item.empty()) continue;
            std::vector<std::string> fields = SplitString(item, ',');
            if (m_kind == KumaPlotRequestKind::Histogram && fields.size() >= 6) {
                HistogramBin bin;
                if (ParseLongDouble(fields[0], bin.lower)
                        && ParseLongDouble(fields[1], bin.upper)
                        && ParseLongDouble(fields[2], bin.midpoint)
                        && ParseLongDouble(fields[3], bin.count)
                        && ParseLongDouble(fields[4], bin.proportion)
                        && ParseLongDouble(fields[5], bin.density)) {
                    m_bins.push_back(bin);
                }
            } else if (m_kind == KumaPlotRequestKind::Bar && fields.size() >= 2) {
                Bar bar;
                if (ParseLongDouble(fields[0], bar.label)
                        && ParseLongDouble(fields[1], bar.height)) {
                    m_bars.push_back(bar);
                }
            } else if ((m_kind == KumaPlotRequestKind::Frequency || m_kind == KumaPlotRequestKind::Dot)
                       && fields.size() >= 2) {
                Point point;
                if (ParseLongDouble(fields[0], point.x) && ParseLongDouble(fields[1], point.y))
                    m_points.push_back(point);
            } else if (fields.size() >= 2) {
                Point point;
                if (ParseLongDouble(fields[0], point.x) && ParseLongDouble(fields[1], point.y))
                    m_points.push_back(point);
            }
        }
    }

    void DrawGrid(wxGraphicsContext* gc, const wxRect& plot) {
        long double xTick = NiceTick(m_xMax - m_xMin);
        long double yTick = NiceTick(m_yMax - m_yMin);
        wxFont font(wxFontInfo(9).Family(wxFONTFAMILY_DEFAULT));
        gc->SetPen(wxPen(ColourFromRgb(0x243040u), 1));
        gc->SetFont(font, ColourFromRgb(theme::kMutedText));

        for (long double x = ceill(m_xMin / xTick) * xTick; x <= m_xMax; x += xTick) {
            double sx = WorldToScreenX(x, plot);
            gc->StrokeLine(sx, plot.y, sx, plot.y + plot.height);
            wxString label = wxString::Format("%Lg", x);
            wxDouble width = 0.0;
            wxDouble height = 0.0;
            gc->GetTextExtent(label, &width, &height);
            gc->DrawText(label, sx - width / 2.0, plot.y + plot.height + 6);
        }
        for (long double y = ceill(m_yMin / yTick) * yTick; y <= m_yMax; y += yTick) {
            double sy = WorldToScreenY(y, plot);
            gc->StrokeLine(plot.x, sy, plot.x + plot.width, sy);
            wxString label = wxString::Format("%Lg", y);
            wxDouble width = 0.0;
            wxDouble height = 0.0;
            gc->GetTextExtent(label, &width, &height);
            gc->DrawText(label, plot.x - width - 8, sy - height / 2.0);
        }

        gc->SetPen(wxPen(ColourFromRgb(theme::kMutedText), 2));
        double xAxis = WorldToScreenY(XAxisWorldY(), plot);
        double yAxis = WorldToScreenX(YAxisWorldX(), plot);
        gc->StrokeLine(plot.x, xAxis, plot.x + plot.width, xAxis);
        gc->StrokeLine(yAxis, plot.y + plot.height, yAxis, plot.y);
        gc->StrokeLine(plot.x + plot.width, xAxis, plot.x + plot.width - 9, xAxis - 5);
        gc->StrokeLine(plot.x + plot.width, xAxis, plot.x + plot.width - 9, xAxis + 5);
        gc->StrokeLine(yAxis, plot.y, yAxis - 5, plot.y + 9);
        gc->StrokeLine(yAxis, plot.y, yAxis + 5, plot.y + 9);
    }

    void DrawLabels(wxGraphicsContext* gc, const wxRect& plot) {
        wxFont titleFont(wxFontInfo(13).Family(wxFONTFAMILY_DEFAULT).Bold());
        gc->SetFont(titleFont, ColourFromRgb(theme::kText));
        wxDouble titleWidth = 0.0;
        wxDouble titleHeight = 0.0;
        gc->GetTextExtent(m_title, &titleWidth, &titleHeight);
        gc->DrawText(m_title, plot.x + (plot.width - titleWidth) / 2.0, 18);

        wxFont labelFont(wxFontInfo(10).Family(wxFONTFAMILY_DEFAULT));
        gc->SetFont(labelFont, ColourFromRgb(theme::kMutedText));
        wxDouble xWidth = 0.0;
        wxDouble xHeight = 0.0;
        gc->GetTextExtent(m_xLabel, &xWidth, &xHeight);
        gc->DrawText(m_xLabel, plot.x + (plot.width - xWidth) / 2.0, plot.y + plot.height + 40);
        gc->DrawText(m_yLabel, 12, plot.y - 28);
    }

    void DrawBox(wxGraphicsContext* gc, const wxRect& plot) {
        if (!m_box.valid) return;
        double x = WorldToScreenX(1.0L, plot);
        double boxHalf = std::min(44.0, plot.width * 0.08);
        double q1 = WorldToScreenY(m_box.q1, plot);
        double q3 = WorldToScreenY(m_box.q3, plot);
        double med = WorldToScreenY(m_box.median, plot);
        double low = WorldToScreenY(m_box.lowerWhisker, plot);
        double high = WorldToScreenY(m_box.upperWhisker, plot);

        gc->SetPen(wxPen(ColourFromRgb(theme::kAccentStrong), 2));
        gc->SetBrush(wxBrush(ColourFromRgb(0x1F5662u)));
        gc->DrawRectangle(x - boxHalf, std::min(q1, q3), boxHalf * 2.0, std::abs(q3 - q1));
        gc->StrokeLine(x - boxHalf, med, x + boxHalf, med);
        gc->StrokeLine(x, high, x, std::min(q1, q3));
        gc->StrokeLine(x, std::max(q1, q3), x, low);
        gc->StrokeLine(x - boxHalf * 0.65, high, x + boxHalf * 0.65, high);
        gc->StrokeLine(x - boxHalf * 0.65, low, x + boxHalf * 0.65, low);
        for (long double outlier : m_box.outliers) {
            double y = WorldToScreenY(outlier, plot);
            gc->DrawEllipse(x - 3, y - 3, 6, 6);
        }
    }

    void DrawBars(wxGraphicsContext* gc, const wxRect& plot) {
        gc->SetPen(wxPen(ColourFromRgb(theme::kAccentStrong), 1));
        gc->SetBrush(wxBrush(ColourFromRgb(0x2A6F7Cu)));
        for (size_t i = 0; i < m_bins.size(); i++) {
            double x0 = WorldToScreenX(m_bins[i].lower, plot);
            double x1 = WorldToScreenX(m_bins[i].upper, plot);
            double y = WorldToScreenY(m_bins[i].count, plot);
            double base = WorldToScreenY(0.0L, plot);
            gc->DrawRectangle(std::min(x0, x1), y, std::max(1.0, std::abs(x1 - x0)), base - y);
        }
        for (size_t i = 0; i < m_bars.size(); i++) {
            double center = WorldToScreenX((long double)i + 0.5L, plot);
            double next = WorldToScreenX((long double)i + 1.0L, plot);
            double width = std::max(4.0, std::abs(next - center) * 1.2);
            double y = WorldToScreenY(m_bars[i].height, plot);
            double base = WorldToScreenY(0.0L, plot);
            gc->DrawRectangle(center - width / 2.0, y, width, base - y);
        }
    }

    void DrawPoints(wxGraphicsContext* gc, const wxRect& plot) {
        gc->SetPen(wxPen(ColourFromRgb(theme::kAccentStrong), 2));
        gc->SetBrush(wxBrush(ColourFromRgb(theme::kAccent)));
        for (const auto& point : m_points) {
            if (m_kind == KumaPlotRequestKind::Dot) {
                for (int i = 1; i <= (int)llround(point.y); i++) {
                    double sx = WorldToScreenX(point.x, plot);
                    double sy = WorldToScreenY((long double)i, plot);
                    gc->DrawEllipse(sx - 3, sy - 3, 6, 6);
                }
            } else if (m_kind == KumaPlotRequestKind::Frequency) {
                double sx = WorldToScreenX(point.x, plot);
                double sy = WorldToScreenY(point.y, plot);
                double base = WorldToScreenY(0.0L, plot);
                gc->StrokeLine(sx, base, sx, sy);
                gc->DrawEllipse(sx - 4, sy - 4, 8, 8);
            } else {
                double sx = WorldToScreenX(point.x, plot);
                double sy = WorldToScreenY(point.y, plot);
                gc->DrawEllipse(sx - 3, sy - 3, 6, 6);
            }
        }
    }

    void DrawLinePlot(wxGraphicsContext* gc, const wxRect& plot) {
        if (m_points.size() < 2) return;
        gc->SetPen(wxPen(ColourFromRgb(theme::kAccentStrong), 2));
        if (m_kind == KumaPlotRequestKind::ECDF) {
            long double prevX = m_xMin;
            long double prevY = 0.0L;
            for (const auto& point : m_points) {
                gc->StrokeLine(WorldToScreenX(prevX, plot), WorldToScreenY(prevY, plot),
                               WorldToScreenX(point.x, plot), WorldToScreenY(prevY, plot));
                gc->StrokeLine(WorldToScreenX(point.x, plot), WorldToScreenY(prevY, plot),
                               WorldToScreenX(point.x, plot), WorldToScreenY(point.y, plot));
                prevX = point.x;
                prevY = point.y;
            }
            gc->StrokeLine(WorldToScreenX(prevX, plot), WorldToScreenY(prevY, plot),
                           WorldToScreenX(m_xMax, plot), WorldToScreenY(prevY, plot));
            return;
        }

        for (size_t i = 1; i < m_points.size(); i++) {
            gc->StrokeLine(WorldToScreenX(m_points[i - 1].x, plot), WorldToScreenY(m_points[i - 1].y, plot),
                           WorldToScreenX(m_points[i].x, plot), WorldToScreenY(m_points[i].y, plot));
        }
    }

    void DrawRegressionLine(wxGraphicsContext* gc, const wxRect& plot) {
        if (!m_lineValid) return;
        gc->SetPen(wxPen(ColourFromRgb(0xE5C07Bu), 2));
        gc->StrokeLine(WorldToScreenX(m_lineStart.x, plot), WorldToScreenY(m_lineStart.y, plot),
                       WorldToScreenX(m_lineEnd.x, plot), WorldToScreenY(m_lineEnd.y, plot));
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(ColourFromRgb(theme::kPanelBg)));
        dc.Clear();

        wxRect plot = PlotRect();
        dc.SetBrush(wxBrush(ColourFromRgb(theme::kTerminalBg)));
        dc.SetPen(wxPen(ColourFromRgb(theme::kBorder), 1));
        dc.DrawRectangle(plot);

        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        DrawGrid(gc.get(), plot);
        DrawLabels(gc.get(), plot);

        gc->Clip(plot.x, plot.y, plot.width, plot.height);
        if (m_kind == KumaPlotRequestKind::Box) DrawBox(gc.get(), plot);
        else if (m_kind == KumaPlotRequestKind::Histogram || m_kind == KumaPlotRequestKind::Bar) DrawBars(gc.get(), plot);
        else if (m_kind == KumaPlotRequestKind::Density || m_kind == KumaPlotRequestKind::ECDF) DrawLinePlot(gc.get(), plot);
        else {
            DrawRegressionLine(gc.get(), plot);
            DrawPoints(gc.get(), plot);
        }
        gc->ResetClip();
    }

    void ClampForcedY() {
        if (!ForceNonnegativeY()) return;
        m_yMin = 0.0L;
        if (!isfinite(m_yMax) || m_yMax <= 0.0L) m_yMax = 1.0L;
    }

    void OnMouseWheel(wxMouseEvent& evt) {
        if (evt.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
            evt.Skip();
            return;
        }
        wxRect plot = PlotRect();
        wxPoint pt = evt.GetPosition();
        long double anchorX = ScreenToWorldX(pt.x, plot);
        long double anchorY = ScreenToWorldY(pt.y, plot);
        long double factor = evt.GetWheelRotation() > 0 ? 0.85L : 1.0L / 0.85L;
        m_xMin = anchorX + (m_xMin - anchorX) * factor;
        m_xMax = anchorX + (m_xMax - anchorX) * factor;
        if (ForceNonnegativeY()) {
            anchorY = std::clamp(anchorY, 0.0L, m_yMax);
            m_yMax = anchorY + (m_yMax - anchorY) * factor;
            ClampForcedY();
        } else {
            m_yMin = anchorY + (m_yMin - anchorY) * factor;
            m_yMax = anchorY + (m_yMax - anchorY) * factor;
        }
        Refresh(false);
    }

    void OnLeftDown(wxMouseEvent& evt) {
        SetFocus();
        wxRect plot = PlotRect();
        m_dragMode = AxisDragMode(evt.GetPosition(), plot);
        m_dragging = true;
        m_dragStart = evt.GetPosition();
        m_dragXMin = m_xMin;
        m_dragXMax = m_xMax;
        m_dragYMin = m_yMin;
        m_dragYMax = m_yMax;
        if (!HasCapture()) CaptureMouse();
    }

    void OnLeftUp(wxMouseEvent&) {
        if (!m_dragging) return;
        m_dragging = false;
        m_dragMode = DragMode::None;
        if (HasCapture()) ReleaseMouse();
    }

    void OnMouseMove(wxMouseEvent& evt) {
        if (!m_dragging || !evt.LeftIsDown()) {
            UpdateAxisCursor(evt.GetPosition());
            evt.Skip();
            return;
        }
        wxRect plot = PlotRect();
        wxPoint pt = evt.GetPosition();
        if (m_dragMode == DragMode::ScaleX) {
            long double center = (m_dragXMin + m_dragXMax) / 2.0L;
            long double span = m_dragXMax - m_dragXMin;
            long double factor = expl(-(long double)(pt.x - m_dragStart.x) / 160.0L);
            factor = std::clamp(factor, 0.05L, 20.0L);
            m_xMin = center - span * factor / 2.0L;
            m_xMax = center + span * factor / 2.0L;
            SetCursor(wxCursor(wxCURSOR_SIZEWE));
        } else if (m_dragMode == DragMode::ScaleY) {
            long double factor = expl((long double)(pt.y - m_dragStart.y) / 160.0L);
            factor = std::clamp(factor, 0.05L, 20.0L);
            if (ForceNonnegativeY()) {
                long double span = m_dragYMax - m_dragYMin;
                m_yMin = 0.0L;
                m_yMax = span * factor;
                ClampForcedY();
            } else {
                long double center = (m_dragYMin + m_dragYMax) / 2.0L;
                long double span = m_dragYMax - m_dragYMin;
                m_yMin = center - span * factor / 2.0L;
                m_yMax = center + span * factor / 2.0L;
            }
            SetCursor(wxCursor(wxCURSOR_SIZENS));
        } else {
            long double dx = (long double)(pt.x - m_dragStart.x) / (long double)std::max(1, plot.width) * (m_dragXMax - m_dragXMin);
            long double dy = (long double)(pt.y - m_dragStart.y) / (long double)std::max(1, plot.height) * (m_dragYMax - m_dragYMin);
            m_xMin = m_dragXMin - dx;
            m_xMax = m_dragXMax - dx;
            if (!ForceNonnegativeY()) {
                m_yMin = m_dragYMin + dy;
                m_yMax = m_dragYMax + dy;
            }
        }
        Refresh(false);
    }

    KumaPlotRequestKind m_kind = KumaPlotRequestKind::Scatter;
    KumaPlotRequest m_request;
    wxString m_title = "KUMA Plot";
    wxString m_xLabel = "x";
    wxString m_yLabel = "y";
    Box m_box;
    std::vector<HistogramBin> m_bins;
    std::vector<Bar> m_bars;
    std::vector<Point> m_points;
    Point m_lineStart;
    Point m_lineEnd;
    bool m_lineValid = false;
    bool m_dragging = false;
    DragMode m_dragMode = DragMode::None;
    wxPoint m_dragStart;
    long double m_dragXMin = 0.0L;
    long double m_dragXMax = 1.0L;
    long double m_dragYMin = 0.0L;
    long double m_dragYMax = 1.0L;
    long double m_xMin = 0.0L;
    long double m_xMax = 1.0L;
    long double m_yMin = 0.0L;
    long double m_yMax = 1.0L;
};

class KumaPlotPage : public wxPanel {
public:
    KumaPlotPage(wxWindow* parent, const KumaPlotRequest& request, const wxString& tabTitle)
        : wxPanel(parent, wxID_ANY) {
        StyleDarkWindow(this, theme::kPanelBg);
        auto* root = new wxBoxSizer(wxVERTICAL);
        m_canvas = new KumaPlotCanvas(this);
        root->Add(m_canvas, 1, wxEXPAND);

        auto* resetRow = new wxBoxSizer(wxHORIZONTAL);
        resetRow->AddStretchSpacer(1);
        auto* reset = new wxButton(this, wxID_ANY, "Reset", wxDefaultPosition, wxSize(86, 30));
        StyleDarkButton(reset, true);
        reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_canvas->ResetView(); });
        resetRow->Add(reset, 0, wxRIGHT | wxBOTTOM, 12);
        root->Add(resetRow, 0, wxEXPAND);

        SetSizer(root);
        m_canvas->SetPlot(request, tabTitle);
    }

    KumaPlotRequest PlotRequest() const {
        return m_canvas->PlotRequest();
    }

private:
    KumaPlotCanvas* m_canvas = nullptr;
};

// ============================================================
// MainFrame: tab strip + simplebook of pages.
// ============================================================
struct HelpSearchTarget {
    wxWindow* control;
    wxString text;
    long textPosition = -1;
    int priority = 50;
};

struct WrappedBlock {
    wxTextCtrl* control;
    wxString source;
    int horizontalPadding;
    int lastWrapWidth;
};

struct HelpPageState {
    wxScrolledWindow* page = nullptr;
    std::vector<HelpSearchTarget> targets;
    wxString lastQuery;
    int lastMatchIndex = -1;
    std::function<void()> applyWrapAndLayout;
    // Zoom support: base fonts per control + invalidation hook into wrappedBlocks.
    // Current zoom level is owned by MainFrame (m_helpZoomDelta) so it's
    // shared across all help tabs and persists when tabs are closed.
    std::vector<std::pair<wxWindow*, wxFont>> controlFonts;
    std::shared_ptr<std::vector<WrappedBlock>> wrappedBlocks;
    wxString highlightedQuery;
};

enum HelpSearchPriority {
    kHelpSearchPriorityCommandName = 0,
    kHelpSearchPriorityCommandUsage = 10,
    kHelpSearchPriorityCommandPurpose = 20,
    kHelpSearchPrioritySectionTitle = 30,
    kHelpSearchPrioritySectionLabel = 40,
    kHelpSearchPriorityBodyText = 50,
    kHelpSearchPriorityCommandValues = 60,
    kHelpSearchPriorityCommandReturns = 70,
    kHelpSearchPriorityCommandArity = 80,
};

struct Tab {
    wxString      title;
    wxWindow*     page;
    wxPanel*      tab;
    bool          closable;
    int           sessionNumber; // >= 1 for numbered Bestiary sessions, -1 otherwise
    int           graphNumber = -1; // >= 1 for numbered Graph tabs, -1 otherwise
    int           kumaPlotNumber = -1; // >= 1 for numbered KUMA Plot tabs, -1 otherwise
    TerminalView* terminal;      // nullptr for non-terminal pages
    GraphPage*    graph;         // nullptr for non-graph pages
    KumaPlotPage* kumaPlot;      // nullptr for non-KUMA plot pages
    int           scrollX = 0;
    int           scrollY = 0;
    std::shared_ptr<HelpPageState> helpPage;
};

class MainFrame : public wxFrame {
public:
    static constexpr int kZoomOutId = wxID_HIGHEST + 100;
    static constexpr int kZoomInId  = wxID_HIGHEST + 101;
    static constexpr int kFindId    = wxID_HIGHEST + 102;
    static constexpr int kNewTabId  = wxID_HIGHEST + 103;
    static constexpr int kCloseTabId = wxID_HIGHEST + 104;
    static constexpr int kSelectTabBaseId = wxID_HIGHEST + 120;
    static constexpr int kDefaultFrameWidth = 1100;
    static constexpr int kDefaultFrameHeight = 760;
    static constexpr int kMinFrameWidth = 960;
    static constexpr int kMinFrameHeight = 640;
    static constexpr double kMinFrameAspect = 4.0 / 3.0;
    static constexpr double kMaxFrameAspect = 16.0 / 9.0;

    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "Bestiary", wxDefaultPosition, wxSize(kDefaultFrameWidth, kDefaultFrameHeight)),
          m_selected(wxNOT_FOUND) {
                StyleDarkWindow(this, theme::kFrameBg);
        SetSizeHints(kMinFrameWidth, kMinFrameHeight);

        LoadPersistedZoom();

        wxIconBundle icons = BuildEmbeddedIconBundle();
        if (icons.GetIconCount() > 0) SetIcons(icons);

        // Use an accelerator table so Ctrl+-/+/= preempt the native Rich Edit
        // bindings (subscript/soft-hyphen) on Windows, where wxEVT_CHAR_HOOK
        // and wxEVT_KEY_DOWN don't reliably swallow them inside a wxTextCtrl.
        wxAcceleratorEntry accels[] = {
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'-',           kZoomOutId),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_SUBTRACT,       kZoomOutId),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD_SUBTRACT,kZoomOutId),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'=',           kZoomInId),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'+',           kZoomInId),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_ADD,            kZoomInId),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD_ADD,     kZoomInId),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'F',           kFindId),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'T',           kNewTabId),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'D',           kCloseTabId),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'1',           kSelectTabBaseId + 0),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'2',           kSelectTabBaseId + 1),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'3',           kSelectTabBaseId + 2),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'4',           kSelectTabBaseId + 3),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'5',           kSelectTabBaseId + 4),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'6',           kSelectTabBaseId + 5),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'7',           kSelectTabBaseId + 6),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'8',           kSelectTabBaseId + 7),
            wxAcceleratorEntry(wxACCEL_CTRL, (int)'9',           kSelectTabBaseId + 8),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD1,        kSelectTabBaseId + 0),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD2,        kSelectTabBaseId + 1),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD3,        kSelectTabBaseId + 2),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD4,        kSelectTabBaseId + 3),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD5,        kSelectTabBaseId + 4),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD6,        kSelectTabBaseId + 5),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD7,        kSelectTabBaseId + 6),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD8,        kSelectTabBaseId + 7),
            wxAcceleratorEntry(wxACCEL_CTRL, WXK_NUMPAD9,        kSelectTabBaseId + 8),
        };
        SetAcceleratorTable(wxAcceleratorTable(WXSIZEOF(accels), accels));
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ZoomActiveTab(-1); }, kZoomOutId);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ZoomActiveTab(+1); }, kZoomInId);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){
            if (m_selected >= 0 && m_selected < (int)m_tabs.size() && m_tabs[m_selected].helpPage)
                SearchCurrentHelpPage();
        }, kFindId);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ AddBestiaryTab(); }, kNewTabId);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ CloseCurrentTab(); }, kCloseTabId);
        for (int i = 0; i < 9; i++) {
            Bind(wxEVT_MENU, [this, i](wxCommandEvent&){ SelectTabByShortcut(i); }, kSelectTabBaseId + i);
        }
        auto* root = new wxBoxSizer(wxVERTICAL);
        m_tabStrip = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxHSCROLL | wxBORDER_NONE);
                StyleDarkWindow(m_tabStrip, theme::kFrameBg);
        m_tabStrip->SetScrollRate(8, 0);
        m_tabSizer = new wxBoxSizer(wxHORIZONTAL);
        m_tabStrip->SetSizer(m_tabSizer);

        m_pages = new wxSimplebook(this);
                StyleDarkWindow(m_pages, theme::kPanelBg);
        Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);
        Bind(wxEVT_SIZE, &MainFrame::OnFrameSize, this);
        Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);

        root->Add(m_tabStrip, 0, wxEXPAND);
        root->Add(m_pages, 1, wxEXPAND);
        SetSizer(root);

        AddStartPage();
        LoadPersistedSession();
        Centre();
    }

private:
    void OnClose(wxCloseEvent& evt) {
        SavePersistedSession();
        SavePersistedZoom();
        evt.Skip();
    }

    wxSize GraphFriendlyFrameSize(const wxSize& size) const {
        int width = std::max(size.x, kMinFrameWidth);
        int height = std::max(size.y, kMinFrameHeight);
        double aspect = (double)width / (double)height;

        if (aspect < kMinFrameAspect)
            width = (int)ceil((double)height * kMinFrameAspect);
        else if (aspect > kMaxFrameAspect)
            height = (int)ceil((double)width / kMaxFrameAspect);

        return wxSize(width, height);
    }

    void OnFrameSize(wxSizeEvent& evt) {
        if (!m_adjustingFrameSize && !IsMaximized() && !IsFullScreen()) {
            wxSize current = evt.GetSize();
            wxSize adjusted = GraphFriendlyFrameSize(current);
            if (adjusted != current) {
                m_adjustingFrameSize = true;
                SetSize(adjusted);
                m_adjustingFrameSize = false;
                return;
            }
        }
        evt.Skip();
    }

    int NextSessionNumber() {
        for (int n = 1; ; n++)
            if (m_openNumbers.find(n) == m_openNumbers.end()) return n;
    }

    int NextGraphNumber() {
        for (int n = 1; ; n++)
            if (m_openGraphNumbers.find(n) == m_openGraphNumbers.end()) return n;
    }

    int NextKumaPlotNumber() {
        for (int n = 1; ; n++)
            if (m_openKumaPlotNumbers.find(n) == m_openKumaPlotNumbers.end()) return n;
    }

    void UpdateStartPageScale() {
        if (!m_startPage) return;

        wxSize pageSize = m_startPage->GetClientSize();
        if (pageSize.x <= 0 || pageSize.y <= 0) return;

        if (m_startTitle) {
            wxFont titleFont = m_startTitle->GetFont();
            int pointSize = std::clamp((int)(pageSize.y * 0.054), m_startTitleBasePointSize, 32);
            titleFont.SetPointSize(pointSize);
            titleFont.SetWeight(wxFONTWEIGHT_BOLD);
            m_startTitle->SetFont(titleFont);
        }
        if (m_startVersion) {
            wxFont versionFont = m_startVersion->GetFont();
            int pointSize = std::clamp((int)(pageSize.y * 0.032), m_startVersionBasePointSize, 18);
            versionFont.SetPointSize(pointSize);
            versionFont.SetWeight(wxFONTWEIGHT_NORMAL);
            m_startVersion->SetFont(versionFont);
        }

        if (m_startBanner && m_startBannerImage.IsOk()) {
            int titleWidth = m_startTitle ? m_startTitle->GetBestSize().x : 0;
            int targetWidth = std::max((int)(pageSize.x * 0.70), titleWidth + (int)(pageSize.x * 0.10));
            int maxWidth  = std::clamp(targetWidth, 260, std::max(260, (int)(pageSize.x * 0.86)));
            int maxHeight = std::max(130, (int)(pageSize.y * 0.38));
            double scale = std::min((double)maxWidth / m_startBannerImage.GetWidth(),
                                    (double)maxHeight / m_startBannerImage.GetHeight());
            int bannerWidth  = std::max(1, (int)(m_startBannerImage.GetWidth() * scale));
            int bannerHeight = std::max(1, (int)(m_startBannerImage.GetHeight() * scale));
            wxBitmap bitmap(m_startBannerImage.Scale(bannerWidth, bannerHeight, wxIMAGE_QUALITY_HIGH));
            m_startBanner->SetBitmap(bitmap);
            m_startBanner->SetMinSize(wxSize(bannerWidth, bannerHeight));
        }

        int buttonWidth  = std::clamp((int)(pageSize.x * 0.38), 220, 400);
        int buttonHeight = std::clamp((int)(pageSize.y * 0.09), 34, 56);
        int buttonPointSize = std::clamp((int)(pageSize.y * 0.03), m_startButtonBasePointSize, 18);

        for (wxButton* button : {m_startButton, m_scriptButton, m_helpButton}) {
            if (!button) continue;
            wxFont buttonFont = button->GetFont();
            buttonFont.SetPointSize(buttonPointSize);
            button->SetFont(buttonFont);
            button->SetMinSize(wxSize(buttonWidth, buttonHeight));
        }

        m_startPage->Layout();
    }

    void AddStartPage() {
        auto* page = new wxPanel(m_pages);
        StyleDarkWindow(page, theme::kPanelBg);
        auto* outer = new wxBoxSizer(wxVERTICAL);
        auto* center = new wxBoxSizer(wxVERTICAL);

        m_startPage = page;

        wxImage image = LoadEmbeddedBannerImage();
        if (image.IsOk()) {
            m_startBannerImage = image;
            m_startBanner = new wxStaticBitmap(page, wxID_ANY, wxBitmap(image));
            center->Add(m_startBanner, 0, wxALIGN_CENTER | wxBOTTOM, 12);
        }

        auto* title = new wxStaticText(page, wxID_ANY, "Bestiary: Release the BEASTs!");
        StyleDarkLabel(title);
        wxFont titleFont = title->GetFont();
        titleFont.SetPointSize(titleFont.GetPointSize() + 4);
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        title->SetFont(titleFont);
        m_startTitle = title;
        m_startTitleBasePointSize = titleFont.GetPointSize();
        center->Add(title, 0, wxALIGN_CENTER | wxBOTTOM, 8);

        auto* version = new wxStaticText(page, wxID_ANY, "v2.0.0");
        StyleDarkLabel(version, true);
        wxFont versionFont = version->GetFont();
        versionFont.SetPointSize(versionFont.GetPointSize() + 1);
        versionFont.SetWeight(wxFONTWEIGHT_NORMAL);
        version->SetFont(versionFont);
        m_startVersion = version;
        m_startVersionBasePointSize = versionFont.GetPointSize();
        center->Add(version, 0, wxALIGN_CENTER | wxBOTTOM, 24);

        auto* startButton  = new wxButton(page, wxID_ANY, "Start Bestiary", wxDefaultPosition, wxSize(220, 34));
        auto* scriptButton = new wxButton(page, wxID_ANY, "Run script",     wxDefaultPosition, wxSize(220, 34));
        auto* helpButton   = new wxButton(page, wxID_ANY, "Help and docs",  wxDefaultPosition, wxSize(220, 34));
        StyleDarkButton(startButton, true);
        StyleDarkButton(scriptButton);
        StyleDarkButton(helpButton);
        m_startButton = startButton;
        m_scriptButton = scriptButton;
        m_helpButton = helpButton;
        m_startButtonBasePointSize = startButton->GetFont().GetPointSize();
        center->Add(startButton,  0, wxALIGN_CENTER | wxBOTTOM, 10);
        center->Add(scriptButton, 0, wxALIGN_CENTER | wxBOTTOM, 10);
        center->Add(helpButton,   0, wxALIGN_CENTER);

        outer->AddStretchSpacer(1);
        outer->Add(center, 0, wxALIGN_CENTER);
        outer->AddStretchSpacer(2);
        page->SetSizer(outer);

        page->Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
            UpdateStartPageScale();
            evt.Skip();
        });

        startButton ->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddBestiaryTab(); });
        scriptButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenScriptTab(); });
        helpButton  ->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddHelpTab(); });

        AddPage("Start page", page, false, -1, nullptr);
        CallAfter([this]() { UpdateStartPageScale(); });
    }

    wxPanel* MakeTabControl(const wxString& title, bool closable, size_t index) {
        auto* tab = new wxPanel(m_tabStrip);
        StyleDarkWindow(tab, index == 0 ? theme::kTabActiveBg : theme::kTabBg);
        auto* sizer = new wxBoxSizer(wxHORIZONTAL);
        auto* label = new wxStaticText(tab, wxID_ANY, title);
        StyleDarkLabel(label, index != 0);
        sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 8);

        if (closable) {
            auto* close = new wxButton(tab, wxID_ANY, "x", wxDefaultPosition, wxSize(24, 24), wxBU_EXACTFIT);
            StyleDarkButton(close);
            sizer->Add(close, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
            // Defer the actual close so we don't destroy this button mid-event.
            close->Bind(wxEVT_BUTTON, [this, tab](wxCommandEvent&) {
                CallAfter([this, tab]() { CloseTabByControl(tab); });
            });
        }

        tab->SetSizer(sizer);
    tab->Bind(wxEVT_LEFT_DOWN,   [this, tab](wxMouseEvent& evt) { BeginTabDrag(tab, evt); });
    tab->Bind(wxEVT_MOTION,      [this](wxMouseEvent& evt) { UpdateTabDrag(evt); });
    tab->Bind(wxEVT_LEFT_UP,     [this](wxMouseEvent& evt) { EndTabDrag(evt); });
    label->Bind(wxEVT_LEFT_DOWN, [this, tab](wxMouseEvent& evt) { BeginTabDrag(tab, evt); });
    label->Bind(wxEVT_MOTION,    [this](wxMouseEvent& evt) { UpdateTabDrag(evt); });
    label->Bind(wxEVT_LEFT_UP,   [this](wxMouseEvent& evt) { EndTabDrag(evt); });
        tab->SetToolTip(title);
        wxSize minSize = sizer->CalcMin();
        minSize.x += 4;
        minSize.y = std::max(minSize.y, 30);
        if (index == 0) minSize.x = std::max(minSize.x, 112);
        tab->SetMinSize(minSize);
        return tab;
    }

    void RebuildTabStrip() {
        m_tabSizer->Clear(true);
        for (size_t i = 0; i < m_tabs.size(); i++) {
            m_tabs[i].tab = MakeTabControl(m_tabs[i].title, m_tabs[i].closable, i);
            m_tabSizer->Add(m_tabs[i].tab, 0, wxEXPAND | wxRIGHT, 2);
        }
        auto* plus = new wxButton(m_tabStrip, wxID_ANY, "+", wxDefaultPosition, wxSize(30, 30), wxBU_EXACTFIT);
        StyleDarkButton(plus, true);
        plus->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AddBestiaryTab(); });
        m_tabSizer->Add(plus, 0, wxALIGN_CENTER_VERTICAL);

        m_tabStrip->FitInside();
        m_tabStrip->Layout();
        RefreshTabStyles();
    }

    void RefreshTabStyles() {
        for (size_t i = 0; i < m_tabs.size(); i++) {
            bool selected = (int)i == m_selected;
            wxColour bg = ColourFromRgb(selected ? theme::kTabActiveBg : theme::kTabBg);
            m_tabs[i].tab->SetBackgroundColour(bg);
            for (wxWindowList::compatibility_iterator child = m_tabs[i].tab->GetChildren().GetFirst(); child; child = child->GetNext()) {
                if (auto* label = wxDynamicCast(child->GetData(), wxStaticText)) {
                    StyleDarkLabel(label, !selected);
                } else if (auto* button = wxDynamicCast(child->GetData(), wxButton)) {
                    StyleDarkButton(button, selected);
                }
            }
            m_tabs[i].tab->Refresh();
        }
    }

    void AddPage(const wxString& title, wxWindow* page, bool closable,
                 int sessionNumber, TerminalView* terminal,
                 int graphNumber = -1, GraphPage* graph = nullptr,
                 int kumaPlotNumber = -1, KumaPlotPage* kumaPlot = nullptr,
                 std::shared_ptr<HelpPageState> helpPageState = nullptr) {
        m_pages->AddPage(page, title);
        Tab tab{};
        tab.title = title;
        tab.page = page;
        tab.tab = nullptr;
        tab.closable = closable;
        tab.sessionNumber = sessionNumber;
        tab.graphNumber = graphNumber;
        tab.kumaPlotNumber = kumaPlotNumber;
        tab.terminal = terminal;
        tab.graph = graph;
        tab.kumaPlot = kumaPlot;
        tab.helpPage = std::move(helpPageState);
        m_tabs.push_back(tab);
        // Apply current shared zoom level so a freshly opened help tab matches
        // any zoom the user has already set on other help tabs.
        if (m_tabs.back().helpPage && m_helpZoomDelta != 0)
            ApplyHelpZoomToPage(m_tabs.back().helpPage);
        RebuildTabStrip();
        SelectTab((int)m_tabs.size() - 1);
    }

    void AddBestiaryTab(const wxString& scriptPath = wxString()) {
        int number = -1;
        wxString title;
        if (scriptPath.empty()) {
            number = NextSessionNumber();
            m_openNumbers.insert(number);
            title = wxString::Format("Bestiary %d", number);
        } else {
            title = wxFileName(scriptPath).GetFullName();
        }

        auto* term = new TerminalView(m_pages);
        term->SetCloseCallback([this, term]() { CloseTabByPage(term); });
        term->SetGraphRequestCallback([this](const GraphRequest& request) { HandleGraphRequest(request); });
        term->SetKumaPlotRequestCallback([this](const KumaPlotRequest& request) { HandleKumaPlotRequest(request); });
        term->SetZoomDelta(m_terminalZoomDelta);
        AddPage(title, term, true, number, term);

        wxString exe = BestiaryExecutablePath();
        if (term->Spawn(exe) && !scriptPath.empty()) {
            wxString sp = scriptPath;
            CallAfter([term, sp]() { term->RunScript(sp); });
        }
    }

    wxString WriteRestoredScript(int number, const std::vector<wxString>& commands) {
        if (commands.empty()) return wxString();
        wxString dir = BestiarySessionDirectory();
        if (dir.empty()) return wxString();
        wxString path = wxFileName(dir, wxString::Format("restore-%d.bsy", number)).GetFullPath();
        wxScopedCharBuffer filePath = path.ToUTF8();
        FILE* fp = fopen(filePath.data(), "w");
        if (!fp) return wxString();

        bool wroteCommand = false;
        for (const wxString& command : commands) {
            wxScopedCharBuffer u8 = command.ToUTF8();
            if (u8.data() && u8.length() > 0) fwrite(u8.data(), 1, u8.length(), fp);
            if (command.empty() || command.Last() != '\n') fputc('\n', fp);
            wroteCommand = true;
        }
        fclose(fp);
        if (!wroteCommand) {
            remove(filePath.data());
            return wxString();
        }
        return path;
    }

    void AddRestoredBestiaryTab(const wxString& title, const std::vector<wxString>& commands,
                                const std::vector<wxString>& transcript) {
        int number = NextSessionNumber();
        m_openNumbers.insert(number);

        auto* term = new TerminalView(m_pages);
        term->SetCloseCallback([this, term]() { CloseTabByPage(term); });
        term->SetGraphRequestCallback([this](const GraphRequest& request) { HandleGraphRequest(request); });
        term->SetKumaPlotRequestCallback([this](const KumaPlotRequest& request) { HandleKumaPlotRequest(request); });
        term->SetZoomDelta(m_terminalZoomDelta);
        term->SetCommandHistory(commands);
        term->SetTranscriptLines(transcript);
        AddPage(title.empty() ? wxString::Format("Bestiary %d", number) : title, term, true, number, term);

        wxString exe = BestiaryExecutablePath();
        wxString script = WriteRestoredScript(number, commands);
        wxArrayString args;
        if (!script.empty()) {
            args.Add("--restore");
            args.Add(script);
        }
        args.Add("--no-startup-message");
        term->Spawn(exe, args);
    }

    GraphPage* FindGraphPage(int number) const {
        for (const auto& tab : m_tabs) {
            if (tab.graphNumber == number) return tab.graph;
        }
        return nullptr;
    }

    void HandleGraphRequest(const GraphRequest& request) {
        if (request.target > 0) {
            if (request.target > INT_MAX) return;
            GraphPage* existing = FindGraphPage((int)request.target);
            if (existing) {
                existing->AddGraph(request);
                for (size_t i = 0; i < m_tabs.size(); i++) {
                    if (m_tabs[i].graph == existing) {
                        SelectTab((int)i);
                        break;
                    }
                }
                return;
            }
        }
        AddGraphTab(request);
    }

    void AddGraphTab(const GraphRequest& request) {
        int number = request.target > 0 && request.target <= INT_MAX
            ? (int)request.target
            : NextGraphNumber();
        if (m_openGraphNumbers.find(number) != m_openGraphNumbers.end())
            number = NextGraphNumber();
        m_openGraphNumbers.insert(number);

        auto* graph = new GraphPage(m_pages);
        GraphRequest adjusted = request;
        adjusted.target = number;
        graph->AddGraph(adjusted);
        AddPage(wxString::Format("Graph %d", number), graph, true, -1, nullptr, number, graph);
    }

    void AddRestoredGraphTab(int number, const std::vector<GraphRequest>& requests) {
        if (number < 1 || m_openGraphNumbers.find(number) != m_openGraphNumbers.end())
            number = NextGraphNumber();
        m_openGraphNumbers.insert(number);

        auto* graph = new GraphPage(m_pages);
        for (GraphRequest request : requests) {
            request.target = number;
            graph->AddGraph(request);
        }
        AddPage(wxString::Format("Graph %d", number), graph, true, -1, nullptr, number, graph);
    }

    void HandleKumaPlotRequest(const KumaPlotRequest& request) {
        AddKumaPlotTab(request);
    }

    void AddKumaPlotTab(const KumaPlotRequest& request) {
        int number = NextKumaPlotNumber();
        m_openKumaPlotNumbers.insert(number);
        wxString title = wxString::Format("KUMA Plot %d", number);
        auto* plot = new KumaPlotPage(m_pages, request, title);
        AddPage(title, plot, true, -1, nullptr, -1, nullptr, number, plot);
    }

    void AddRestoredKumaPlotTab(int number, const KumaPlotRequest& request) {
        if (number < 1 || m_openKumaPlotNumbers.find(number) != m_openKumaPlotNumbers.end())
            number = NextKumaPlotNumber();
        m_openKumaPlotNumbers.insert(number);
        wxString title = wxString::Format("KUMA Plot %d", number);
        auto* plot = new KumaPlotPage(m_pages, request, title);
        AddPage(title, plot, true, -1, nullptr, -1, nullptr, number, plot);
    }

    void OpenScriptTab() {
        wxFileDialog dialog(this, "Run Bestiary script", wxEmptyString, wxEmptyString,
                            "Bestiary scripts (*.bsy)|*.bsy|Text files (*.txt)|*.txt|All files (*.*)|*.*",
                            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dialog.ShowModal() == wxID_OK) AddBestiaryTab(dialog.GetPath());
    }

    wxString FormatHelpUsage(const BestiaryHelpPage::Command& command) const {
        wxString usage = "\\";
        usage += command.name;
        if (command.arity < 0) {
            usage += "{value...}";
            return usage;
        }
        for (int i = 0; i < command.arity; i++)
            usage += wxString::Format("{value%d}", i + 1);
        return usage;
    }

    std::vector<wxString> BuildHelpCommandEntries(BestiaryHelpPage::Beast beast) const {
        std::vector<wxString> entries;
        for (size_t i = 0; i < BestiaryHelpPage::kCommandCount; i++) {
            const auto& command = BestiaryHelpPage::kCommands[i];
            if (command.beast != beast) continue;

            wxString text;
            text += "* \\help{";
            text += command.name;
            text += "}\n";
            text += "  Command: ";
            text += FormatHelpUsage(command);
            text += "\n  Purpose: ";
            text += command.purpose;
            text += "\n  Values: ";
            text += command.values;
            text += "\n  Returns: ";
            text += command.returns;
            text += "\n  Arity: ";
            text += command.arity < 0 ? "variadic" : wxString::Format("%d", command.arity);
            entries.push_back(text);
        }
        return entries;
    }

    wxString BuildHelpSectionText(BestiaryHelpPage::Beast beast) const {
        wxString text;
        for (size_t i = 0; i < BestiaryHelpPage::kCommandCount; i++) {
            const auto& command = BestiaryHelpPage::kCommands[i];
            if (command.beast != beast) continue;
            if (!text.empty()) text += "\n\n";
            text += "* \\help{";
            text += command.name;
            text += "}\n";
            text += "  Command: ";
            text += FormatHelpUsage(command);
            text += "\n  Purpose: ";
            text += command.purpose;
            text += "\n  Values: ";
            text += command.values;
            text += "\n  Returns: ";
            text += command.returns;
            text += "\n  Arity: ";
            text += command.arity < 0 ? "variadic" : wxString::Format("%d", command.arity);
        }
        return text;
    }

    const BestiaryHelpPage::Section* FindHelpSection(BestiaryHelpPage::Beast beast) const {
        for (size_t i = 0; i < BestiaryHelpPage::kSectionCount; i++)
            if (BestiaryHelpPage::kSections[i].beast == beast) return &BestiaryHelpPage::kSections[i];
        return nullptr;
    }

    wxString HelpBeastName(const BestiaryHelpPage::Section& section) const {
        wxString title = section.title;
        int sep = title.Find(" (");
        return sep == wxNOT_FOUND ? title : title.Left(sep);
    }

    void ForwardMouseWheelToPage(wxScrolledWindow* page, wxWindow* source) const {
        source->Bind(wxEVT_MOUSEWHEEL, [page](wxMouseEvent& evt) {
            int wheelDelta = std::max(1, evt.GetWheelDelta());
            int linesPerAction = evt.GetLinesPerAction() > 0 ? evt.GetLinesPerAction() : 3;
            int steps = std::max(1, std::abs(evt.GetWheelRotation()) / wheelDelta) * linesPerAction;

            int viewX = 0;
            int viewY = 0;
            page->GetViewStart(&viewX, &viewY);
            if (evt.GetWheelRotation() > 0) viewY = std::max(0, viewY - steps);
            else if (evt.GetWheelRotation() < 0) viewY += steps;
            page->Scroll(viewX, viewY);
        });
    }

    void BindHelpSearchShortcut(wxWindow* control) {
        control->Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& evt) {
            int kc = evt.GetKeyCode();
            if (evt.ControlDown() && !evt.AltDown()) {
                if (kc == 'F' || kc == 'f') {
                    SearchCurrentHelpPage();
                    return;
                }
                if (kc == '-' || kc == WXK_SUBTRACT || kc == WXK_NUMPAD_SUBTRACT) {
                    ZoomActiveTab(-1);
                    return;
                }
                if (kc == '+' || kc == '=' || kc == WXK_ADD || kc == WXK_NUMPAD_ADD) {
                    ZoomActiveTab(+1);
                    return;
                }
            }
            evt.Skip();
        });
    }

    static void BindHelpCopy(wxWindow* control, const wxString& text) {
        control->Bind(wxEVT_RIGHT_DOWN, [control, text](wxMouseEvent&) {
            wxMenu menu;
            menu.Append(wxID_COPY, "&Copy");
            menu.Bind(wxEVT_MENU, [text](wxCommandEvent&) {
                if (wxClipboard::Get()->Open()) {
                    wxClipboard::Get()->SetData(new wxTextDataObject(text));
                    wxClipboard::Get()->Close();
                }
            }, wxID_COPY);
            control->PopupMenu(&menu);
        });
    }

    wxTextCtrl* CreateHelpTextBlock(wxWindow* parent, const wxString& text, const wxFont& font) const {
        auto* block = new wxTextCtrl(parent, wxID_ANY, text, wxDefaultPosition, wxDefaultSize,
                                     wxTE_READONLY | wxTE_MULTILINE | wxTE_WORDWRAP |
                                     wxTE_NO_VSCROLL | wxTE_RICH2 | wxBORDER_NONE);
        StyleDarkTextCtrl(block);
        block->SetFont(font);
        block->SetEditable(false);
        return block;
    }

    void ApplyHelpPageSearchHighlights(const std::shared_ptr<HelpPageState>& helpPage,
                                       const wxString& query) {
        if (!helpPage || !helpPage->wrappedBlocks) return;

        wxTextAttr baseStyle(ColourFromRgb(theme::kText), ColourFromRgb(theme::kPanelBg));
        baseStyle.SetFlags(wxTEXT_ATTR_TEXT_COLOUR | wxTEXT_ATTR_BACKGROUND_COLOUR);
        wxTextAttr highlightStyle(ColourFromRgb(theme::kSearchHighlightText),
                                  ColourFromRgb(theme::kSearchHighlightBg));
        highlightStyle.SetFlags(wxTEXT_ATTR_TEXT_COLOUR | wxTEXT_ATTR_BACKGROUND_COLOUR);

        wxString needle = query.Lower();
        for (const auto& block : *helpPage->wrappedBlocks) {
            if (!block.control) continue;
            wxTextCtrl* control = block.control;
            long end = control->GetLastPosition();
            control->SetStyle(0, end, baseStyle);

            if (needle.empty()) continue;

            wxString haystack = block.source.Lower();
            size_t offset = 0;
            while (offset < haystack.length()) {
                int pos = haystack.Mid(offset).Find(needle);
                if (pos == wxNOT_FOUND) break;
                long start = (long)(offset + (size_t)pos);
                long matchEnd = start + (long)needle.length();
                control->SetStyle(start, matchEnd, highlightStyle);
                offset = (size_t)matchEnd;
            }
        }

        helpPage->highlightedQuery = query;
    }

    void AddHelpTextBlock(wxScrolledWindow* page,
                          wxBoxSizer* sizer,
                          const wxString& text,
                          const wxFont& font,
                          int bottomPadding,
                          const std::shared_ptr<std::vector<WrappedBlock>>& wrappedBlocks,
                          const std::shared_ptr<HelpPageState>& helpPageState,
                          std::vector<wxWindow*>& scrollTargets) {
        auto* block = CreateHelpTextBlock(page, text, font);
        sizer->Add(block, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, bottomPadding);
        wrappedBlocks->push_back({block, text, 48, -1});
        helpPageState->targets.push_back({block, text});
        helpPageState->controlFonts.push_back({block, font});
        scrollTargets.push_back(block);
        BindHelpCopy(block, text);
    }

    wxTextCtrl* AddSelectableHelpTextArea(wxScrolledWindow* page,
                                          wxBoxSizer* sizer,
                                          const wxString& text,
                                          const wxFont& font,
                                          int bottomPadding,
                                          const std::shared_ptr<std::vector<WrappedBlock>>& wrappedBlocks,
                                          const std::shared_ptr<HelpPageState>& helpPageState,
                                          std::vector<wxWindow*>& scrollTargets) {
        auto* block = CreateHelpTextBlock(page, text, font);
        sizer->Add(block, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, bottomPadding);
        wrappedBlocks->push_back({block, text, 48, -1});
        helpPageState->controlFonts.push_back({block, font});
        scrollTargets.push_back(block);
        BindHelpCopy(block, text);
        return block;
    }

    void AddHelpTextPositionTarget(const std::shared_ptr<HelpPageState>& helpPageState,
                                   wxTextCtrl* control,
                                   const wxString& text,
                                   long position,
                                   int priority = kHelpSearchPriorityBodyText) const {
        helpPageState->targets.push_back({control, text, position, priority});
    }

    int HelpCommandFieldPriority(size_t fieldIndex) const {
        switch (fieldIndex) {
            case 0: return kHelpSearchPriorityCommandName;
            case 1: return kHelpSearchPriorityCommandUsage;
            case 2: return kHelpSearchPriorityCommandPurpose;
            case 3: return kHelpSearchPriorityCommandValues;
            case 4: return kHelpSearchPriorityCommandReturns;
            case 5: return kHelpSearchPriorityCommandArity;
            default: return kHelpSearchPriorityBodyText;
        }
    }

    int ScoreHelpSearchTarget(const HelpSearchTarget& target, const wxString& needleLower) const {
        wxString haystack = target.text.Lower();
        int pos = haystack.Find(needleLower);
        if (pos == wxNOT_FOUND) return INT_MAX;

        int score = target.priority * 1000;
        if (haystack == needleLower) score += 0;
        else if (haystack.StartsWith(needleLower)) score += 50;
        else if (pos == 0) score += 100;
        else score += 200 + std::min(pos, 200);
        score += std::min((int)haystack.length(), 400);
        return score;
    }

    void AddHelpCommandBlock(wxScrolledWindow* page,
                             wxBoxSizer* sizer,
                             const BestiaryHelpPage::Command& command,
                             const wxFont& font,
                             const std::shared_ptr<std::vector<WrappedBlock>>& wrappedBlocks,
                             const std::shared_ptr<HelpPageState>& helpPageState,
                             std::vector<wxWindow*>& scrollTargets) {
        auto* panel = new wxPanel(page, wxID_ANY);
        StyleDarkWindow(panel, theme::kPanelRaisedBg);
        auto* panelSizer = new wxBoxSizer(wxVERTICAL);
        panel->SetSizer(panelSizer);

        std::vector<wxString> fields = BuildHelpCommandFields(command);
        for (size_t i = 0; i < fields.size(); i++) {
            auto* line = CreateHelpTextBlock(panel, fields[i], font);
            panelSizer->Add(line, 0, wxEXPAND | wxBOTTOM, i + 1 == fields.size() ? 0 : 2);
            wrappedBlocks->push_back({line, fields[i], 32, -1});
            helpPageState->targets.push_back({line, fields[i]});
            helpPageState->controlFonts.push_back({line, font});
            scrollTargets.push_back(line);
            BindHelpSearchShortcut(line);
            BindHelpCopy(line, fields[i]);
            ForwardMouseWheelToPage(page, line);
        }

        sizer->Add(panel, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 16);
        BindHelpSearchShortcut(panel);
    }

    std::vector<wxString> SplitHelpParagraphs(const wxString& text) const {
        std::vector<wxString> parts;
        wxString remaining = text;
        while (!remaining.empty()) {
            int sep = remaining.Find("\n\n");
            wxString part = sep == wxNOT_FOUND ? remaining : remaining.Left(sep);
            part.Trim(true).Trim(false);
            if (!part.empty()) parts.push_back(part);
            if (sep == wxNOT_FOUND) break;
            remaining = remaining.Mid(sep + 2);
        }
        return parts;
    }

    std::vector<wxString> SplitHelpLines(const wxString& text) const {
        std::vector<wxString> parts;
        wxString remaining = text;
        while (!remaining.empty()) {
            int sep = remaining.Find('\n');
            wxString part = sep == wxNOT_FOUND ? remaining : remaining.Left(sep);
            part.Trim(true).Trim(false);
            if (!part.empty()) parts.push_back(part);
            if (sep == wxNOT_FOUND) break;
            remaining = remaining.Mid(sep + 1);
        }
        return parts;
    }

    std::vector<wxString> BuildHelpCommandFields(const BestiaryHelpPage::Command& command) const {
        std::vector<wxString> fields;
        wxString helpLine = "* \\help{";
        helpLine += command.name;
        helpLine += "}";
        fields.push_back(helpLine);

        wxString commandLine = "  Command: ";
        commandLine += FormatHelpUsage(command);
        fields.push_back(commandLine);

        wxString purposeLine = "  Purpose: ";
        purposeLine += command.purpose;
        fields.push_back(purposeLine);

        wxString valuesLine = "  Values: ";
        valuesLine += command.values;
        fields.push_back(valuesLine);

        wxString returnsLine = "  Returns: ";
        returnsLine += command.returns;
        fields.push_back(returnsLine);

        wxString arityLine = "  Arity: ";
        arityLine += command.arity < 0 ? "variadic" : wxString::Format("%d", command.arity);
        fields.push_back(arityLine);
        return fields;
    }

    int EstimateWrappedTextHeight(wxTextCtrl* control, const wxString& text, int wrapWidth) const {
        wxClientDC dc(control);
        dc.SetFont(control->GetFont());
        wxCoord charW = 0;
        wxCoord lineH = 0;
        dc.GetTextExtent("M", &charW, &lineH);
        if (charW <= 0) charW = 8;
        if (lineH <= 0) lineH = 16;

        int usableWidth = std::max(1, wrapWidth - 10);
        int totalLines = 0;
        wxString remaining = text;
        while (true) {
            int sep = remaining.Find('\n');
            wxString line = sep == wxNOT_FOUND ? remaining : remaining.Left(sep);
            wxCoord lineW = 0;
            wxCoord ignoredH = 0;
            dc.GetTextExtent(line.empty() ? " " : line, &lineW, &ignoredH);
            totalLines += std::max(1, (int)((lineW + usableWidth - 1) / usableWidth));
            if (sep == wxNOT_FOUND) break;
            remaining = remaining.Mid(sep + 1);
        }

        return std::max((int)lineH + 8, totalLines * ((int)lineH + 2) + 8);
    }

    std::function<void()> MakeHelpPageRelayout(
        wxScrolledWindow* page,
        const std::shared_ptr<std::vector<WrappedBlock>>& wrappedBlocks) const {
        return [this, page, wrappedBlocks]() {
            bool wrappedAny = false;
            page->Freeze();
            for (auto& block : *wrappedBlocks) {
                if (!block.control || !block.control->IsShown()) continue;
                int wrapWidth = std::max(240, block.control->GetParent()->GetClientSize().x - block.horizontalPadding);
                if (wrapWidth == block.lastWrapWidth) continue;
                block.control->SetMinSize(wxSize(-1, EstimateWrappedTextHeight(block.control, block.source, wrapWidth)));
                block.lastWrapWidth = wrapWidth;
                wrappedAny = true;
            }
            if (wrappedAny) page->Layout();
            page->FitInside();
            page->Thaw();
        };
    }

    std::function<void()> MakeHelpPageRelayoutScheduler(
        wxScrolledWindow* page,
        const std::shared_ptr<std::vector<WrappedBlock>>& wrappedBlocks) const {
        auto relayoutPending = std::make_shared<bool>(false);
        auto relayout = std::make_shared<std::function<void()>>(MakeHelpPageRelayout(page, wrappedBlocks));
        return [page, relayoutPending, relayout]() {
            if (*relayoutPending) return;
            *relayoutPending = true;
            page->CallAfter([relayoutPending, relayout]() {
                *relayoutPending = false;
                (*relayout)();
            });
        };
    }

    void SaveTabScrollState(int index) {
        if (index < 0 || index >= (int)m_tabs.size()) return;
        auto* scrolled = wxDynamicCast(m_tabs[index].page, wxScrolledWindow);
        if (!scrolled) return;
        scrolled->GetViewStart(&m_tabs[index].scrollX, &m_tabs[index].scrollY);
    }

    void RestoreTabScrollState(int index) {
        if (index < 0 || index >= (int)m_tabs.size()) return;
        auto* scrolled = wxDynamicCast(m_tabs[index].page, wxScrolledWindow);
        if (!scrolled) return;
        scrolled->Scroll(m_tabs[index].scrollX, m_tabs[index].scrollY);
    }

    int TabIndexFromControl(wxWindow* tab) const {
        for (size_t i = 0; i < m_tabs.size(); i++)
            if (m_tabs[i].tab == tab) return (int)i;
        return wxNOT_FOUND;
    }

    int DropTabIndex(const wxPoint& stripPoint, int draggedIndex) const {
        int target = draggedIndex;
        for (size_t i = 1; i < m_tabs.size(); i++) {
            if ((int)i == draggedIndex) continue;
            wxRect rect = m_tabs[i].tab->GetRect();
            int midpoint = rect.x + rect.width / 2;
            if (stripPoint.x < midpoint) return (int)i;
            target = (int)i;
        }
        return std::max(1, target);
    }

    void ReorderTab(int from, int to) {
        if (from <= 0 || to <= 0 || from == to) return;
        if (from < 0 || from >= (int)m_tabs.size() || to >= (int)m_tabs.size()) return;

        SaveTabScrollState(m_selected);
        wxWindow* selectedPage = m_selected >= 0 && m_selected < (int)m_tabs.size() ? m_tabs[m_selected].page : nullptr;

        Tab moved = m_tabs[from];
        m_tabs.erase(m_tabs.begin() + from);
        m_tabs.insert(m_tabs.begin() + to, moved);

        RebuildTabStrip();
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].page == selectedPage) {
                SelectTab((int)i);
                return;
            }
        }
    }

    void BeginTabDrag(wxWindow* tab, wxMouseEvent& evt) {
        int index = TabIndexFromControl(tab);
        if (index == wxNOT_FOUND) {
            evt.Skip();
            return;
        }
        SelectTab(index);
        if (index == 0) {
            evt.Skip();
            return;
        }
        m_dragTabIndex = index;
        m_dragTabOrigin = tab;
        m_dragStartScreen = wxGetMousePosition();
        m_draggingTabs = false;
        if (m_dragTabOrigin && !m_dragTabOrigin->HasCapture()) m_dragTabOrigin->CaptureMouse();
        evt.Skip();
    }

    void UpdateTabDrag(wxMouseEvent& evt) {
        if (m_dragTabIndex <= 0 || !evt.LeftIsDown()) {
            evt.Skip();
            return;
        }
        wxPoint delta = wxGetMousePosition() - m_dragStartScreen;
        if (!m_draggingTabs && std::abs(delta.x) + std::abs(delta.y) >= 6) m_draggingTabs = true;
        evt.Skip();
    }

    void EndTabDrag(wxMouseEvent& evt) {
        if (m_dragTabOrigin && m_dragTabOrigin->HasCapture()) m_dragTabOrigin->ReleaseMouse();
        if (m_draggingTabs && m_dragTabIndex > 0) {
            wxPoint stripPoint = m_tabStrip->ScreenToClient(wxGetMousePosition());
            int dropIndex = DropTabIndex(stripPoint, m_dragTabIndex);
            int from = m_dragTabIndex;
            CallAfter([this, from, dropIndex]() { ReorderTab(from, dropIndex); });
        }
        m_draggingTabs = false;
        m_dragTabIndex = wxNOT_FOUND;
        m_dragTabOrigin = nullptr;
        evt.Skip();
    }

    void ScrollHelpTargetIntoView(const std::shared_ptr<HelpPageState>& helpPage, const HelpSearchTarget& target) {
        wxWindow* control = target.control;
        if (!helpPage || !helpPage->page || !control) return;
        auto* page = helpPage->page;

        // Force any pending wrap/layout to run synchronously so the position
        // we measure reflects the current visible layout. Without this, the
        // first Ctrl+F right after opening a help tab can race the deferred
        // wrap scheduler and read positions from a half-laid-out page.
        if (helpPage->applyWrapAndLayout) helpPage->applyWrapAndLayout();
        page->Layout();
        page->FitInside();

        int targetPixelOffsetY = 0;
        if (target.textPosition >= 0) {
            if (auto* text = wxDynamicCast(control, wxTextCtrl)) {
                long row = 0;
                long col = 0;
                text->PositionToXY(target.textPosition, &col, &row);
                wxClientDC dc(text);
                dc.SetFont(text->GetFont());
                wxCoord ignoredW = 0;
                wxCoord lineH = 0;
                dc.GetTextExtent("M", &ignoredW, &lineH);
                if (lineH <= 0) lineH = 16;
                targetPixelOffsetY = (int)row * ((int)lineH + 2);
                text->SetInsertionPoint(target.textPosition);
                text->ShowPosition(target.textPosition);
            }
        }

        // Translate the control's screen position into the page's logical
        // (unscrolled) coordinate space. Going through screen coords is
        // platform-independent and correctly handles arbitrary nesting
        // (e.g. lines inside a command panel inside the page) without
        // assuming whether wxScrolledWindow children move with scroll.
        auto computeTargetUnits = [page, targetPixelOffsetY](wxWindow* c) -> std::pair<int, int> {
            wxPoint clientPos = c->GetScreenPosition() - page->GetScreenPosition();
            wxPoint virt = page->CalcUnscrolledPosition(clientPos);
            int unitX = 1, unitY = 1;
            page->GetScrollPixelsPerUnit(&unitX, &unitY);
            if (unitX <= 0) unitX = 1;
            if (unitY <= 0) unitY = 1;
            int curX = 0;
            page->GetViewStart(&curX, nullptr);
            int targetY = std::max(0, (virt.y + targetPixelOffsetY - 12) / unitY);
            return {curX, targetY};
        };

        auto [curX, targetY] = computeTargetUnits(control);
        page->Scroll(curX, targetY);

        // Re-issue once on the next idle: if anything queued a deferred
        // relayout (e.g. the SIZE event handler from us calling Layout/
        // FitInside), it would otherwise undo this scroll on the next tick.
        // Recompute the target in the lambda so we use the post-relayout
        // position rather than a stale one.
        page->CallAfter([page, control, computeTargetUnits]() {
            if (!page || !control) return;
            auto [x, y] = computeTargetUnits(control);
            page->Scroll(x, y);
        });
    }

    void ApplyHelpZoomToPage(const std::shared_ptr<HelpPageState>& helpPage) {
        if (!helpPage) return;
        const wxColour fg = ColourFromRgb(theme::kText);
        const wxColour bg = ColourFromRgb(theme::kPanelBg);
        for (auto& [control, baseFont] : helpPage->controlFonts) {
            if (!control) continue;
            wxFont f = baseFont;
            f.SetPointSize(std::max(6, f.GetPointSize() + m_helpZoomDelta));
            control->SetFont(f);
            // wxTextCtrl on Windows (Rich Edit) clears the foreground colour
            // when SetFont is called. Re-apply the dark-theme colours to the
            // existing range so text doesn't go grey on zoom.
            control->SetForegroundColour(fg);
            if (auto* tc = wxDynamicCast(control, wxTextCtrl)) {
                tc->SetBackgroundColour(bg);
                wxTextAttr style(fg, bg);
                style.SetFlags(wxTEXT_ATTR_TEXT_COLOUR | wxTEXT_ATTR_BACKGROUND_COLOUR);
                long end = tc->GetLastPosition();
                if (end > 0) tc->SetStyle(0, end, style);
            }
        }
        if (helpPage->wrappedBlocks) {
            for (auto& block : *helpPage->wrappedBlocks)
                block.lastWrapWidth = -1;
        }
        if (helpPage->applyWrapAndLayout) helpPage->applyWrapAndLayout();
        // Re-apply highlights so existing search matches survive the restyle.
        if (!helpPage->lastQuery.empty())
            ApplyHelpPageSearchHighlights(helpPage, helpPage->lastQuery);
    }

    void AdjustHelpZoom(int delta) {
        int next = std::clamp(m_helpZoomDelta + delta, kHelpZoomMin, kHelpZoomMax);
        if (next == m_helpZoomDelta) return;
        m_helpZoomDelta = next;
        for (auto& tab : m_tabs)
            if (tab.helpPage) ApplyHelpZoomToPage(tab.helpPage);
        SavePersistedZoom();
    }

    void AdjustTerminalZoom(int delta) {
        int next = std::clamp(m_terminalZoomDelta + delta,
                              TerminalView::kMinFontSize - TerminalView::kBaseFontPointSize,
                              TerminalView::kMaxFontSize - TerminalView::kBaseFontPointSize);
        if (next == m_terminalZoomDelta) return;
        m_terminalZoomDelta = next;
        for (auto& tab : m_tabs)
            if (tab.terminal) tab.terminal->SetZoomDelta(m_terminalZoomDelta);
        SavePersistedZoom();
    }

    static constexpr int kHelpZoomMin = -5;
    static constexpr int kHelpZoomMax = 8;
    static constexpr const char* kZoomConfigGroup = "/Zoom";
    static constexpr const char* kHelpZoomKey     = "HelpDelta";
    static constexpr const char* kTerminalZoomKey = "TerminalDelta";

    void LoadPersistedZoom() {
        wxConfigBase* config = wxConfigBase::Get(true);
        if (!config) return;
        long help = 0, term = 0;
        config->Read(wxString(kZoomConfigGroup) + "/" + kHelpZoomKey, &help, 0);
        config->Read(wxString(kZoomConfigGroup) + "/" + kTerminalZoomKey, &term, 0);
        m_helpZoomDelta = std::clamp((int)help, kHelpZoomMin, kHelpZoomMax);
        m_terminalZoomDelta = std::clamp(
            (int)term,
            TerminalView::kMinFontSize - TerminalView::kBaseFontPointSize,
            TerminalView::kMaxFontSize - TerminalView::kBaseFontPointSize);
    }

    void SavePersistedZoom() {
        wxConfigBase* config = wxConfigBase::Get(true);
        if (!config) return;
        config->Write(wxString(kZoomConfigGroup) + "/" + kHelpZoomKey,
                      (long)m_helpZoomDelta);
        config->Write(wxString(kZoomConfigGroup) + "/" + kTerminalZoomKey,
                      (long)m_terminalZoomDelta);
        config->Flush();
    }

    static constexpr const char* kSessionConfigGroup = "/Session";

    wxString SessionTabGroup(size_t index) const {
        return wxString::Format("%s/Tab%zu", kSessionConfigGroup, index);
    }

    int FindTabByTitle(const wxString& title) const {
        for (size_t i = 0; i < m_tabs.size(); i++)
            if (m_tabs[i].title == title) return (int)i;
        return wxNOT_FOUND;
    }

    static long GraphKindToLong(GraphRequestKind kind) {
        return kind == GraphRequestKind::Implicit ? 1
            : (kind == GraphRequestKind::Vertical ? 2 : 0);
    }

    static GraphRequestKind LongToGraphKind(long value) {
        if (value == 1) return GraphRequestKind::Implicit;
        if (value == 2) return GraphRequestKind::Vertical;
        return GraphRequestKind::Explicit;
    }

    static long KumaPlotKindToLong(KumaPlotRequestKind kind) {
        return (long)kind;
    }

    static KumaPlotRequestKind LongToKumaPlotKind(long value) {
        if (value == 0) return KumaPlotRequestKind::Box;
        if (value == 1) return KumaPlotRequestKind::Histogram;
        if (value == 2) return KumaPlotRequestKind::Frequency;
        if (value == 3) return KumaPlotRequestKind::Bar;
        if (value == 4) return KumaPlotRequestKind::Density;
        if (value == 5) return KumaPlotRequestKind::Dot;
        if (value == 6) return KumaPlotRequestKind::ECDF;
        if (value == 7) return KumaPlotRequestKind::QQ;
        if (value == 9) return KumaPlotRequestKind::Regression;
        return KumaPlotRequestKind::Scatter;
    }

    static bool TranscriptLineStartsNewPrompt(const wxString& line) {
        wxString text = line;
        text.Trim(false);
        return text.StartsWith(">")
            || text.StartsWith("...")
            || text.StartsWith("=>")
            || text.StartsWith("Bestiary ")
            || text.StartsWith("\\help ")
            || text.StartsWith("[Bestiary ");
    }

    static std::vector<wxString> UnwrapLegacyTranscriptRows(const std::vector<wxString>& rows,
                                                           long savedCols) {
        std::vector<wxString> out;
        size_t wrapWidth = savedCols > 0 ? (size_t)savedCols : 40;

        for (const wxString& row : rows) {
            if (!out.empty() && out.back().length() + 1 >= wrapWidth
                    && !TranscriptLineStartsNewPrompt(row)) {
                out.back() += row;
            } else {
                out.push_back(row);
            }
        }
        return out;
    }

    void SavePersistedSession() {
        SaveTabScrollState(m_selected);
        for (size_t i = 0; i < m_tabs.size(); i++) SaveTabScrollState((int)i);

        wxConfigBase* config = wxConfigBase::Get(true);
        if (!config) return;
        config->DeleteGroup(kSessionConfigGroup);
        config->Write(wxString(kSessionConfigGroup) + "/Selected", (long)m_selected);
        config->Write(wxString(kSessionConfigGroup) + "/TabCount",
                      (long)std::max(0, (int)m_tabs.size() - 1));

        size_t outIndex = 0;
        for (size_t i = 1; i < m_tabs.size(); i++, outIndex++) {
            const Tab& tab = m_tabs[i];
            wxString group = SessionTabGroup(outIndex);
            config->Write(group + "/Title", tab.title);
            config->Write(group + "/ScrollX", (long)tab.scrollX);
            config->Write(group + "/ScrollY", (long)tab.scrollY);

            if (tab.terminal) {
                config->Write(group + "/Type", "terminal");
                const auto& commands = tab.terminal->CommandHistory();
                config->Write(group + "/CommandCount", (long)commands.size());
                for (size_t j = 0; j < commands.size(); j++)
                    config->Write(group + wxString::Format("/Command%zu", j), commands[j]);
                std::vector<wxString> transcript = tab.terminal->TranscriptLines();
                config->Write(group + "/TranscriptLogical", 1L);
                config->Write(group + "/TranscriptCount", (long)transcript.size());
                for (size_t j = 0; j < transcript.size(); j++)
                    config->Write(group + wxString::Format("/Transcript%zu", j), transcript[j]);
            } else if (tab.graph) {
                config->Write(group + "/Type", "graph");
                config->Write(group + "/Number", (long)tab.graphNumber);
                std::vector<GraphRequest> requests = tab.graph->GraphRequests();
                config->Write(group + "/GraphCount", (long)requests.size());
                for (size_t j = 0; j < requests.size(); j++) {
                    wxString item = group + wxString::Format("/Graph%zu", j);
                    config->Write(item + "/Kind", GraphKindToLong(requests[j].kind));
                    config->Write(item + "/Target", (long)requests[j].target);
                    config->Write(item + "/Label", requests[j].label);
                    config->Write(item + "/Serialized", requests[j].serialized);
                }
            } else if (tab.kumaPlot) {
                config->Write(group + "/Type", "kuma");
                config->Write(group + "/Number", (long)tab.kumaPlotNumber);
                KumaPlotRequest request = tab.kumaPlot->PlotRequest();
                config->Write(group + "/Kind", KumaPlotKindToLong(request.kind));
                config->Write(group + "/PlotTitle", request.title);
                config->Write(group + "/XLabel", request.xLabel);
                config->Write(group + "/YLabel", request.yLabel);
                config->Write(group + "/Payload", request.payload);
            } else if (tab.helpPage) {
                config->Write(group + "/Type", "help");
            }
        }
        config->Flush();
    }

    void RestoreHelpScroll(int index, int scrollX, int scrollY) {
        if (index < 0 || index >= (int)m_tabs.size()) return;
        m_tabs[index].scrollX = scrollX;
        m_tabs[index].scrollY = scrollY;
        CallAfter([this, index]() { RestoreTabScrollState(index); });
    }

    void LoadPersistedSession() {
        wxConfigBase* config = wxConfigBase::Get(true);
        if (!config) return;

        long tabCount = 0;
        if (!config->Read(wxString(kSessionConfigGroup) + "/TabCount", &tabCount, 0) || tabCount <= 0)
            return;

        for (long i = 0; i < tabCount; i++) {
            wxString group = SessionTabGroup((size_t)i);
            wxString type;
            wxString title;
            long scrollX = 0;
            long scrollY = 0;
            config->Read(group + "/Type", &type, "");
            config->Read(group + "/Title", &title, "");
            config->Read(group + "/ScrollX", &scrollX, 0);
            config->Read(group + "/ScrollY", &scrollY, 0);

            if (type == "terminal") {
                long commandCount = 0;
                long transcriptCount = 0;
                long transcriptLogical = 0;
                std::vector<wxString> commands;
                std::vector<wxString> transcript;
                config->Read(group + "/CommandCount", &commandCount, 0);
                for (long j = 0; j < commandCount; j++) {
                    wxString command;
                    config->Read(group + wxString::Format("/Command%ld", j), &command, "");
                    if (!command.empty()) commands.push_back(command);
                }
                config->Read(group + "/TranscriptCount", &transcriptCount, 0);
                config->Read(group + "/TranscriptLogical", &transcriptLogical, 0);
                for (long j = 0; j < transcriptCount; j++) {
                    wxString line;
                    config->Read(group + wxString::Format("/Transcript%ld", j), &line, "");
                    transcript.push_back(line);
                }
                if (!transcriptLogical) transcript = UnwrapLegacyTranscriptRows(transcript, 0);
                AddRestoredBestiaryTab(title, commands, transcript);
            } else if (type == "graph") {
                long number = 0;
                long graphCount = 0;
                std::vector<GraphRequest> requests;
                config->Read(group + "/Number", &number, 0);
                config->Read(group + "/GraphCount", &graphCount, 0);
                for (long j = 0; j < graphCount; j++) {
                    wxString item = group + wxString::Format("/Graph%ld", j);
                    long kind = 0;
                    long target = 0;
                    GraphRequest request;
                    config->Read(item + "/Kind", &kind, 0);
                    config->Read(item + "/Target", &target, number);
                    config->Read(item + "/Label", &request.label, "");
                    config->Read(item + "/Serialized", &request.serialized, "");
                    request.kind = LongToGraphKind(kind);
                    request.target = target;
                    if (!request.serialized.empty()) requests.push_back(request);
                }
                AddRestoredGraphTab((int)number, requests);
            } else if (type == "kuma") {
                long number = 0;
                long kind = 8;
                KumaPlotRequest request;
                config->Read(group + "/Number", &number, 0);
                config->Read(group + "/Kind", &kind, 8);
                config->Read(group + "/PlotTitle", &request.title, "");
                config->Read(group + "/XLabel", &request.xLabel, "");
                config->Read(group + "/YLabel", &request.yLabel, "");
                config->Read(group + "/Payload", &request.payload, "");
                request.kind = LongToKumaPlotKind(kind);
                AddRestoredKumaPlotTab((int)number, request);
            } else if (type == "help") {
                if (title == "Help") {
                    AddHelpTab();
                    RestoreHelpScroll(FindTabByTitle("Help"), (int)scrollX, (int)scrollY);
                } else {
                    for (size_t sectionIndex = 0; sectionIndex < BestiaryHelpPage::kSectionCount; sectionIndex++) {
                        const auto& section = BestiaryHelpPage::kSections[sectionIndex];
                        wxString tabTitle = section.beast == BestiaryHelpPage::Beast::Basic
                            ? "Help - Basic"
                            : "Help - " + HelpBeastName(section);
                        if (tabTitle != title) continue;
                        AddBeastHelpTab(section.beast);
                        RestoreHelpScroll(FindTabByTitle(tabTitle), (int)scrollX, (int)scrollY);
                        break;
                    }
                }
            }
        }

        long selected = 0;
        config->Read(wxString(kSessionConfigGroup) + "/Selected", &selected, 0);
        CallAfter([this, selected]() {
            int index = std::clamp((int)selected, 0, std::max(0, (int)m_tabs.size() - 1));
            SelectTab(index);
        });
    }

    void SearchCurrentHelpPage() {
        if (m_selected < 0 || m_selected >= (int)m_tabs.size()) return;
        std::shared_ptr<HelpPageState> helpPage = m_tabs[m_selected].helpPage;
        if (!helpPage) return;

        wxString query = wxGetTextFromUser("Find in this help page:", "Find", helpPage->lastQuery, this);
        if (query.empty()) return;
        ApplyHelpPageSearchHighlights(helpPage, query);

        wxString needle = query.Lower();
        int count = (int)helpPage->targets.size();
        if (count == 0) return;

        std::vector<std::pair<int, int>> matches;
        matches.reserve((size_t)count);
        for (int index = 0; index < count; index++) {
            int score = ScoreHelpSearchTarget(helpPage->targets[index], needle);
            if (score == INT_MAX) continue;
            matches.push_back({score, index});
        }

        std::stable_sort(matches.begin(), matches.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.first != rhs.first) return lhs.first < rhs.first;
            return lhs.second < rhs.second;
        });

        if (!matches.empty()) {
            int matchPos = 0;
            if (helpPage->lastQuery == query && helpPage->lastMatchIndex >= 0) {
                for (size_t i = 0; i < matches.size(); i++) {
                    if (matches[i].second != helpPage->lastMatchIndex) continue;
                    matchPos = (int)((i + 1) % matches.size());
                    break;
                }
            }
            int index = matches[(size_t)matchPos].second;
            helpPage->lastQuery = query;
            helpPage->lastMatchIndex = index;
            ScrollHelpTargetIntoView(helpPage, helpPage->targets[index]);
            return;
        }

        helpPage->lastQuery = query;
        helpPage->lastMatchIndex = -1;
        wxMessageBox(wxString::Format("No match found for \"%s\".", query),
                     "Find", wxOK | wxICON_INFORMATION, this);
    }

    void ZoomActiveTab(int delta) {
        if (m_selected < 0 || m_selected >= (int)m_tabs.size()) return;
        Tab& tab = m_tabs[m_selected];
        if (tab.helpPage) { AdjustHelpZoom(delta); return; }
        if (tab.terminal) { AdjustTerminalZoom(delta); return; }
    }

    void SelectTabByShortcut(int index) {
        if (index < 0 || index >= (int)m_tabs.size()) return;
        SelectTab(index);
    }

    void CloseCurrentTab() {
        if (m_selected <= 0 || m_selected >= (int)m_tabs.size()) return;
        CloseTabByPage(m_tabs[m_selected].page);
    }

    void OnCharHook(wxKeyEvent& evt) {
        int kc = evt.GetKeyCode();
        if (evt.ControlDown() && !evt.AltDown()) {
            if (kc == 'T' || kc == 't') {
                AddBestiaryTab();
                return;
            }
            if (kc == 'D' || kc == 'd') {
                CloseCurrentTab();
                return;
            }
            if (kc >= '1' && kc <= '9') {
                SelectTabByShortcut(kc - '1');
                return;
            }
            if (kc >= WXK_NUMPAD1 && kc <= WXK_NUMPAD9) {
                SelectTabByShortcut(kc - WXK_NUMPAD1);
                return;
            }
            if ((kc == 'F' || kc == 'f') &&
                m_selected >= 0 && m_selected < (int)m_tabs.size() &&
                m_tabs[m_selected].helpPage) {
                SearchCurrentHelpPage();
                return;
            }
            if (kc == '-' || kc == WXK_SUBTRACT || kc == WXK_NUMPAD_SUBTRACT) {
                ZoomActiveTab(-1);
                return;
            }
            if (kc == '+' || kc == '=' || kc == WXK_ADD || kc == WXK_NUMPAD_ADD) {
                ZoomActiveTab(+1);
                return;
            }
        }
        evt.Skip();
    }

    void AddBeastHelpTab(BestiaryHelpPage::Beast beast) {
        const BestiaryHelpPage::Section* section = FindHelpSection(beast);
        if (!section) return;

        wxString tabTitle = section->beast == BestiaryHelpPage::Beast::Basic
            ? "Help - Basic"
            : "Help - " + HelpBeastName(*section);
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].title == tabTitle) { SelectTab((int)i); return; }
        }

        auto* page = new wxScrolledWindow(m_pages, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxVSCROLL | wxBORDER_NONE);
        StyleDarkWindow(page, theme::kPanelBg);
        page->SetScrollRate(12, 12);
        BindHelpSearchShortcut(page);

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        page->SetSizer(sizer);

        auto* title = new wxStaticText(page, wxID_ANY, tabTitle);
        StyleDarkLabel(title);
        wxFont titleFont = title->GetFont();
        titleFont.SetPointSize(titleFont.GetPointSize() + 5);
        titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        title->SetFont(titleFont);

        auto* fullTitle = new wxStaticText(page, wxID_ANY, section->fullTitle);
        StyleDarkLabel(fullTitle);
        wxFont sectionFont = fullTitle->GetFont();
        sectionFont.SetPointSize(std::max(11, sectionFont.GetPointSize() + 1));
        sectionFont.SetWeight(wxFONTWEIGHT_BOLD);
        fullTitle->SetFont(sectionFont);

        auto* link = new wxHyperlinkCtrl(page, wxID_ANY, "Bestiary homepage",
                                         "https://git.keimai.space/algebraity/bestiary");
        StyleDarkHyperlink(link);
        wxFont monoFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
        monoFont.SetWeight(wxFONTWEIGHT_NORMAL);
        wxString featureLabel = section->beast == BestiaryHelpPage::Beast::Basic
            ? "Basic features"
            : "Getting started";
        auto* gettingStartedLabel = new wxStaticText(page, wxID_ANY, featureLabel);
        StyleDarkLabel(gettingStartedLabel);
        gettingStartedLabel->SetFont(sectionFont);
        auto* commandsLabel = new wxStaticText(page, wxID_ANY, "Commands");
        StyleDarkLabel(commandsLabel);
        commandsLabel->SetFont(sectionFont);

        auto wrappedBlocks = std::make_shared<std::vector<WrappedBlock>>();
        auto helpPageState = std::make_shared<HelpPageState>();
        helpPageState->page = page;
        helpPageState->wrappedBlocks = wrappedBlocks;
        helpPageState->applyWrapAndLayout = MakeHelpPageRelayout(page, wrappedBlocks);
        auto scheduleRelayout = MakeHelpPageRelayoutScheduler(page, wrappedBlocks);

        sizer->Add(title, 0, wxALL, 16);
        helpPageState->targets.push_back({title, tabTitle, -1, kHelpSearchPrioritySectionTitle});
        helpPageState->controlFonts.push_back({title, titleFont});
        sizer->Add(fullTitle, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        helpPageState->targets.push_back({fullTitle, section->fullTitle, -1, kHelpSearchPrioritySectionTitle});
        helpPageState->controlFonts.push_back({fullTitle, sectionFont});
        sizer->Add(link, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        helpPageState->targets.push_back({link, link->GetLabel(), -1, kHelpSearchPrioritySectionLabel});
        sizer->Add(gettingStartedLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        helpPageState->targets.push_back({gettingStartedLabel, featureLabel, -1, kHelpSearchPrioritySectionLabel});
        helpPageState->controlFonts.push_back({gettingStartedLabel, sectionFont});

        std::vector<wxWindow*> scrollTargets = {
            title, fullTitle, link, gettingStartedLabel, commandsLabel
        };
        wxString gettingStartedText;
        for (const wxString& line : SplitHelpLines(section->gettingStarted)) {
            if (!gettingStartedText.empty()) gettingStartedText += "\n";
            gettingStartedText += line;
        }
        auto* gettingStartedTextBlock = AddSelectableHelpTextArea(
            page, sizer, gettingStartedText, monoFont, 16, wrappedBlocks, helpPageState, scrollTargets);
        long gettingPos = 0;
        for (const wxString& line : SplitHelpLines(section->gettingStarted)) {
            AddHelpTextPositionTarget(helpPageState, gettingStartedTextBlock, line, gettingPos, kHelpSearchPriorityBodyText);
            gettingPos += (long)line.length() + 1;
        }

        sizer->Add(commandsLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxTOP, 16);
        helpPageState->targets.push_back({commandsLabel, "Commands", -1, kHelpSearchPrioritySectionLabel});
        helpPageState->controlFonts.push_back({commandsLabel, sectionFont});
        wxString commandsText;
        struct CommandSearchTargetSpec {
            wxString text;
            long position;
            int priority;
        };
        std::vector<CommandSearchTargetSpec> commandTargets;
        for (size_t i = 0; i < BestiaryHelpPage::kCommandCount; i++) {
            const auto& command = BestiaryHelpPage::kCommands[i];
            if (command.beast != section->beast) continue;
            if (!commandsText.empty()) commandsText += "\n\n";
            long commandStart = (long)commandsText.length();
            std::vector<wxString> fields = BuildHelpCommandFields(command);
            wxString commandText;
            for (const wxString& field : fields) {
                if (!commandText.empty()) commandText += "\n";
                commandText += field;
            }
            commandsText += commandText;
            commandTargets.push_back({commandText, commandStart, kHelpSearchPriorityBodyText});
            for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++) {
                const wxString& field = fields[fieldIndex];
                long fieldStart = commandStart + commandText.Find(field);
                commandTargets.push_back({field, fieldStart, HelpCommandFieldPriority(fieldIndex)});
            }
        }
        auto* commandsTextBlock = AddSelectableHelpTextArea(
            page, sizer, commandsText, monoFont, 16, wrappedBlocks, helpPageState, scrollTargets);
        for (const auto& target : commandTargets)
            AddHelpTextPositionTarget(helpPageState, commandsTextBlock, target.text, target.position, target.priority);
        for (wxWindow* control : scrollTargets)
        {
            ForwardMouseWheelToPage(page, control);
            BindHelpSearchShortcut(control);
        }

        page->Bind(wxEVT_SIZE, [scheduleRelayout](wxSizeEvent& evt) {
            scheduleRelayout();
            evt.Skip();
        });
        scheduleRelayout();
        AddPage(tabTitle, page, true, -1, nullptr, -1, nullptr, -1, nullptr, helpPageState);
    }

    void AddHelpTab() {
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].title == "Help") { SelectTab((int)i); return; }
        }
        auto* page = new wxScrolledWindow(m_pages, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxVSCROLL | wxBORDER_NONE);
        StyleDarkWindow(page, theme::kPanelBg);
        page->SetScrollRate(12, 12);
        BindHelpSearchShortcut(page);

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        page->SetSizer(sizer);

        auto* title = new wxStaticText(page, wxID_ANY, "Help");
        StyleDarkLabel(title);
        wxFont tf = title->GetFont();
        tf.SetPointSize(tf.GetPointSize() + 5);
        tf.SetWeight(wxFONTWEIGHT_BOLD);
        title->SetFont(tf);
        auto* link = new wxHyperlinkCtrl(page, wxID_ANY, "Bestiary homepage",
                                         "https://git.keimai.space/algebraity/bestiary");
        StyleDarkHyperlink(link);
        wxFont bodyFont = page->GetFont();
        bodyFont.SetWeight(wxFONTWEIGHT_NORMAL);
        bodyFont.SetStyle(wxFONTSTYLE_NORMAL);
        auto wrappedBlocks = std::make_shared<std::vector<WrappedBlock>>();
        auto helpPageState = std::make_shared<HelpPageState>();
        helpPageState->page = page;
        helpPageState->wrappedBlocks = wrappedBlocks;
        helpPageState->applyWrapAndLayout = MakeHelpPageRelayout(page, wrappedBlocks);
        auto scheduleRelayout = MakeHelpPageRelayoutScheduler(page, wrappedBlocks);
        std::vector<wxWindow*> scrollTargets = {title, link};
        helpPageState->targets.push_back({title, "Help"});
        helpPageState->controlFonts.push_back({title, tf});

        sizer->Add(title, 0, wxALL, 16);
        wxString introText;
        std::vector<std::pair<wxString, long>> introTargets;
        for (const wxString& paragraph : SplitHelpParagraphs(BestiaryHelpPage::kIntroText)) {
            if (!introText.empty()) introText += "\n\n";
            long paragraphStart = (long)introText.length();
            introText += paragraph;
            introTargets.push_back({paragraph, paragraphStart});
        }
        auto* introTextBlock = AddSelectableHelpTextArea(
            page, sizer, introText, bodyFont, 12, wrappedBlocks, helpPageState, scrollTargets);
        for (const auto& target : introTargets)
            AddHelpTextPositionTarget(helpPageState, introTextBlock, target.first, target.second);
        sizer->Add(link, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);

        for (size_t i = 0; i < BestiaryHelpPage::kSectionCount; i++) {
            const auto& section = BestiaryHelpPage::kSections[i];
            auto* button = new wxButton(page, wxID_ANY, section.title);
            StyleDarkButton(button);
            button->Bind(wxEVT_BUTTON, [this, beast = section.beast](wxCommandEvent&) { AddBeastHelpTab(beast); });
            sizer->Add(button, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 16);
            scrollTargets.push_back(button);
            helpPageState->targets.push_back({button, section.title});
        }
        for (wxWindow* control : scrollTargets)
        {
            ForwardMouseWheelToPage(page, control);
            BindHelpSearchShortcut(control);
        }

        page->Bind(wxEVT_SIZE, [scheduleRelayout](wxSizeEvent& evt) {
            scheduleRelayout();
            evt.Skip();
        });
        scheduleRelayout();
        AddPage("Help", page, true, -1, nullptr, -1, nullptr, -1, nullptr, helpPageState);
    }

    void SelectTab(int index) {
        if (index < 0 || index >= (int)m_tabs.size()) return;
        SaveTabScrollState(m_selected);
        m_selected = index;
        int pageIndex = m_pages->FindPage(m_tabs[index].page);
        if (pageIndex != wxNOT_FOUND) m_pages->SetSelection((size_t)pageIndex);
        RestoreTabScrollState(index);
        RefreshTabStyles();
        TerminalView* t = m_tabs[index].terminal;
        if (t) CallAfter([t]() { t->FocusInput(); });
    }

    void SelectTabByControl(wxWindow* tab) {
        int index = TabIndexFromControl(tab);
        if (index != wxNOT_FOUND) SelectTab(index);
    }

    void CloseTabByPage(wxWindow* page) {
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].page != page) continue;
            if (!m_tabs[i].closable) return;

            if (m_tabs[i].sessionNumber >= 1)
                m_openNumbers.erase(m_tabs[i].sessionNumber);
            if (m_tabs[i].graphNumber >= 1)
                m_openGraphNumbers.erase(m_tabs[i].graphNumber);
            if (m_tabs[i].kumaPlotNumber >= 1)
                m_openKumaPlotNumbers.erase(m_tabs[i].kumaPlotNumber);

            wxWindow* ownedPage = m_tabs[i].page;
            int pageIndex = m_pages->FindPage(ownedPage);
            if (pageIndex != wxNOT_FOUND) m_pages->RemovePage((size_t)pageIndex);
            m_tabs.erase(m_tabs.begin() + i);
            ownedPage->Destroy();

            if (m_selected >= (int)m_tabs.size()) m_selected = (int)m_tabs.size() - 1;
            RebuildTabStrip();
            SelectTab(m_selected < 0 ? 0 : m_selected);
            return;
        }
    }

    void CloseTabByControl(wxWindow* tab) {
        for (size_t i = 0; i < m_tabs.size(); i++) {
            if (m_tabs[i].tab != tab) continue;
            if (!m_tabs[i].closable) return;

            if (m_tabs[i].sessionNumber >= 1)
                m_openNumbers.erase(m_tabs[i].sessionNumber);
            if (m_tabs[i].graphNumber >= 1)
                m_openGraphNumbers.erase(m_tabs[i].graphNumber);
            if (m_tabs[i].kumaPlotNumber >= 1)
                m_openKumaPlotNumbers.erase(m_tabs[i].kumaPlotNumber);

            wxWindow* page = m_tabs[i].page;
            int pageIndex = m_pages->FindPage(page);
            if (pageIndex != wxNOT_FOUND) m_pages->RemovePage((size_t)pageIndex);
            m_tabs.erase(m_tabs.begin() + i);
            page->Destroy();

            if (m_selected >= (int)m_tabs.size()) m_selected = (int)m_tabs.size() - 1;
            RebuildTabStrip();
            SelectTab(m_selected < 0 ? 0 : m_selected);
            return;
        }
    }

    wxScrolledWindow* m_tabStrip;
    wxBoxSizer*       m_tabSizer;
    wxSimplebook*     m_pages;
    wxVector<Tab>     m_tabs;
    int               m_selected;
    std::set<int>     m_openNumbers;
    std::set<int>     m_openGraphNumbers;
    std::set<int>     m_openKumaPlotNumbers;
    int               m_dragTabIndex = wxNOT_FOUND;
    bool              m_draggingTabs = false;
    wxWindow*         m_dragTabOrigin = nullptr;
    wxPoint           m_dragStartScreen;

    wxPanel*          m_startPage = nullptr;
    wxStaticBitmap*   m_startBanner = nullptr;
    wxImage           m_startBannerImage;
    wxStaticText*     m_startTitle = nullptr;
    wxStaticText*     m_startVersion = nullptr;
    wxButton*         m_startButton = nullptr;
    wxButton*         m_scriptButton = nullptr;
    wxButton*         m_helpButton = nullptr;
    int               m_startTitleBasePointSize = 0;
    int               m_startVersionBasePointSize = 0;
    int               m_startButtonBasePointSize = 0;

    // Shared zoom levels persisted for the lifetime of the frame.
    // Help pages and terminals each have their own level so the user can
    // tune them independently; closing a tab does not lose the setting.
    int               m_helpZoomDelta = 0;
    int               m_terminalZoomDelta = 0;
    bool              m_adjustingFrameSize = false;
};

#ifdef __WXMSW__
static void EnableWindowsDpiAwareness() {
    HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetProcessDpiAwarenessContextFn = BOOL (WINAPI*)(HANDLE);
        auto setCtx = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            ::GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setCtx) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
            if (setCtx((HANDLE)-4)) return;
            // Fall back to DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE == (HANDLE)-3
            if (setCtx((HANDLE)-3)) return;
            // Fall back to DPI_AWARENESS_CONTEXT_SYSTEM_AWARE == (HANDLE)-2
            if (setCtx((HANDLE)-2)) return;
        }
    }

    HMODULE shcore = ::LoadLibraryW(L"shcore.dll");
    if (shcore) {
        using SetProcessDpiAwarenessFn = HRESULT (WINAPI*)(int);
        auto setAwareness = reinterpret_cast<SetProcessDpiAwarenessFn>(
            ::GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (setAwareness) {
            // PROCESS_PER_MONITOR_DPI_AWARE == 2, PROCESS_SYSTEM_DPI_AWARE == 1
            if (SUCCEEDED(setAwareness(2))) { ::FreeLibrary(shcore); return; }
            if (SUCCEEDED(setAwareness(1))) { ::FreeLibrary(shcore); return; }
        }
        ::FreeLibrary(shcore);
    }

    if (user32) {
        using SetProcessDPIAwareFn = BOOL (WINAPI*)(void);
        auto setAware = reinterpret_cast<SetProcessDPIAwareFn>(
            ::GetProcAddress(user32, "SetProcessDPIAware"));
        if (setAware) setAware();
    }
}
#endif

class BestiaryGuiApp : public wxApp {
public:
    bool OnInit() override {
#ifdef __WXMSW__
        EnableWindowsDpiAwareness();
#endif
        // Stable identity for wxConfigBase::Get(): the registry path
        // HKCU\Software\Bestiary\Bestiary on Windows and ~/.config/Bestiary
        // (or similar) on Linux. Used to persist the help/terminal zoom.
        SetAppName("Bestiary");
        SetVendorName("Bestiary");
        wxInitAllImageHandlers();
        auto* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

} // namespace

wxIMPLEMENT_APP(BestiaryGuiApp);
