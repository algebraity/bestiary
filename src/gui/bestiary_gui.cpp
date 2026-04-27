#include<wx/wx.h>
#include<wx/clipbrd.h>
#include<wx/base64.h>
#include<wx/dcbuffer.h>
#include<wx/filename.h>
#include<wx/filedlg.h>
#include<wx/hyperlink.h>
#include<wx/mstream.h>
#include<wx/scrolwin.h>
#include<wx/simplebook.h>
#include<wx/stdpaths.h>
#include<wx/textdlg.h>
#include<wx/timer.h>
#include<wx/vector.h>
#include<algorithm>
#include<cstdio>
#include<cstdint>
#include<deque>
#include<memory>
#include<set>
#include<string>
#include<vector>

#include"help_page_content.h"
#include"embedded_banner.h"
#include"embedded_icon.h"

#ifdef __WXMSW__
#  define WIN32_LEAN_AND_MEAN
#  include<windows.h>
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

    void RunScript(const wxString& path) {
        wxString cmd = wxString::Format("\\run{\"%s\"}\r", path);
        wxScopedCharBuffer u8 = cmd.ToUTF8();
        m_pty.Write(u8.data(), u8.length());
    }

    void ZoomFont(int delta) { ChangeTerminalResolution(delta); }

private:
    struct Cell {
        wxChar  ch    = L' ';
        uint32_t fg   = 0xC0C0C0u;
        uint32_t bg   = 0x000000u;
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
        return cell.ch == L' ' && cell.fg == 0xC0C0C0u && cell.bg == 0x000000u && cell.attrs == 0;
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

    void ChangeTerminalResolution(int delta) {
        int pointSize = m_font.GetPointSize();
        if (pointSize <= 0) pointSize = 11;
        int newSize = std::clamp(pointSize + delta, kMinFontSize, kMaxFontSize);
        if (newSize == pointSize) return;
        m_font.SetPointSize(newSize);
        UpdateGeometry(true);
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
            if (b == 0x07) { m_state = State::Ground; m_paramBuf.clear(); }
            else if (b == 0x1b) { m_state = State::Esc; m_paramBuf.clear(); }
            else m_paramBuf += (char)b;
            break;
        case State::Charset:
            m_state = State::Ground;
            break;
        }
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
            if      (n == 0)               { m_curFg = 0xC0C0C0u; m_curBg = 0x000000u; m_curAttrs = 0; }
            else if (n == 1)               m_curAttrs |= 1;
            else if (n == 4)               m_curAttrs |= 2;
            else if (n == 7)               m_curAttrs |= 4;
            else if (n == 22)              m_curAttrs &= ~1;
            else if (n == 24)              m_curAttrs &= ~2;
            else if (n == 27)              m_curAttrs &= ~4;
            else if (n >= 30  && n <= 37)  m_curFg = pal[n - 30];
            else if (n == 39)              m_curFg = 0xC0C0C0u;
            else if (n >= 40  && n <= 47)  m_curBg = pal[n - 40];
            else if (n == 49)              m_curBg = 0x000000u;
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
        m_curFg = 0xC0C0C0u; m_curBg = 0x000000u; m_curAttrs = 0;
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
            if (kc == '-' || kc == '_' || kc == WXK_SUBTRACT) { ChangeTerminalResolution(-1); return; }
            if (kc == '+' || kc == '=' || kc == WXK_ADD)      { ChangeTerminalResolution(1); return; }
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

    std::deque<PhysicalLine> m_history;
    std::vector<bool> m_screenWrappedFromPrevious = std::vector<bool>((size_t)m_rows, false);
    std::vector<Cell> m_screen;
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
    std::vector<std::pair<wxWindow*, wxFont>> controlFonts;
    std::shared_ptr<std::vector<WrappedBlock>> wrappedBlocks;
    int fontZoomDelta = 0;
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
    TerminalView* terminal;      // nullptr for non-terminal pages
    int           scrollX = 0;
    int           scrollY = 0;
    std::shared_ptr<HelpPageState> helpPage;
};

class MainFrame : public wxFrame {
public:
    static constexpr int kZoomOutId = wxID_HIGHEST + 100;
    static constexpr int kZoomInId  = wxID_HIGHEST + 101;
    static constexpr int kFindId    = wxID_HIGHEST + 102;

    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "Bestiary", wxDefaultPosition, wxSize(720, 520)),
          m_selected(wxNOT_FOUND) {
                StyleDarkWindow(this, theme::kFrameBg);

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
        };
        SetAcceleratorTable(wxAcceleratorTable(WXSIZEOF(accels), accels));
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ZoomActiveTab(-1); }, kZoomOutId);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){ ZoomActiveTab(+1); }, kZoomInId);
        Bind(wxEVT_MENU, [this](wxCommandEvent&){
            if (m_selected >= 0 && m_selected < (int)m_tabs.size() && m_tabs[m_selected].helpPage)
                SearchCurrentHelpPage();
        }, kFindId);
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

        root->Add(m_tabStrip, 0, wxEXPAND);
        root->Add(m_pages, 1, wxEXPAND);
        SetSizer(root);

        AddStartPage();
        Centre();
    }

private:
    int NextSessionNumber() {
        for (int n = 1; ; n++)
            if (m_openNumbers.find(n) == m_openNumbers.end()) return n;
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

        auto* version = new wxStaticText(page, wxID_ANY, "v1.0.0");
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
                 std::shared_ptr<HelpPageState> helpPageState = nullptr) {
        m_pages->AddPage(page, title);
        Tab tab{};
        tab.title = title;
        tab.page = page;
        tab.tab = nullptr;
        tab.closable = closable;
        tab.sessionNumber = sessionNumber;
        tab.terminal = terminal;
        tab.helpPage = std::move(helpPageState);
        m_tabs.push_back(tab);
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
        AddPage(title, term, true, number, term);

        wxString exe = BestiaryExecutablePath();
        if (term->Spawn(exe) && !scriptPath.empty()) {
            wxString sp = scriptPath;
            CallAfter([term, sp]() { term->RunScript(sp); });
        }
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
                if (m_selected >= 0 && m_selected < (int)m_tabs.size() && m_tabs[m_selected].helpPage) {
                    auto& hp = m_tabs[m_selected].helpPage;
                    if (kc == '-' || kc == WXK_SUBTRACT || kc == WXK_NUMPAD_SUBTRACT) {
                        ApplyHelpPageFontZoom(hp, -1);
                        return;
                    }
                    if (kc == '+' || kc == '=' || kc == WXK_ADD || kc == WXK_NUMPAD_ADD) {
                        ApplyHelpPageFontZoom(hp, +1);
                        return;
                    }
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

    void ApplyHelpPageFontZoom(const std::shared_ptr<HelpPageState>& helpPage, int delta) {
        if (!helpPage) return;
        helpPage->fontZoomDelta = std::clamp(helpPage->fontZoomDelta + delta, -5, 8);
        for (auto& [control, baseFont] : helpPage->controlFonts) {
            if (!control) continue;
            wxFont f = baseFont;
            f.SetPointSize(std::max(6, f.GetPointSize() + helpPage->fontZoomDelta));
            control->SetFont(f);
        }
        if (helpPage->wrappedBlocks) {
            for (auto& block : *helpPage->wrappedBlocks)
                block.lastWrapWidth = -1;
        }
        if (helpPage->applyWrapAndLayout) helpPage->applyWrapAndLayout();
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
        if (tab.helpPage) {
            ApplyHelpPageFontZoom(tab.helpPage, delta);
            return;
        }
        if (tab.terminal) {
            tab.terminal->ZoomFont(delta);
            return;
        }
    }

    void OnCharHook(wxKeyEvent& evt) {
        int kc = evt.GetKeyCode();
        if (evt.ControlDown() && !evt.AltDown()) {
            if (kc == 'F' || kc == 'f') {
                if (m_selected >= 0 && m_selected < (int)m_tabs.size() && m_tabs[m_selected].helpPage) {
                    SearchCurrentHelpPage();
                    return;
                }
            }
            if (m_selected >= 0 && m_selected < (int)m_tabs.size() && m_tabs[m_selected].helpPage) {
                auto& hp = m_tabs[m_selected].helpPage;
                if (kc == '-' || kc == WXK_SUBTRACT || kc == WXK_NUMPAD_SUBTRACT) {
                    ApplyHelpPageFontZoom(hp, -1);
                    return;
                }
                if (kc == '+' || kc == '=' || kc == WXK_ADD || kc == WXK_NUMPAD_ADD) {
                    ApplyHelpPageFontZoom(hp, +1);
                    return;
                }
            }
        }
        evt.Skip();
    }

    void AddBeastHelpTab(BestiaryHelpPage::Beast beast) {
        const BestiaryHelpPage::Section* section = FindHelpSection(beast);
        if (!section) return;

        wxString tabTitle = "Help - " + HelpBeastName(*section);
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
        auto* gettingStartedLabel = new wxStaticText(page, wxID_ANY, "Getting started");
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
        helpPageState->targets.push_back({gettingStartedLabel, "Getting started", -1, kHelpSearchPrioritySectionLabel});
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
        AddPage(tabTitle, page, true, -1, nullptr, helpPageState);
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
        AddPage("Help", page, true, -1, nullptr, helpPageState);
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
        wxInitAllImageHandlers();
        auto* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

} // namespace

wxIMPLEMENT_APP(BestiaryGuiApp);
