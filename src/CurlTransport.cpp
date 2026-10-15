#include "CurlTransport.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #define DES_POPEN  _popen
  #define DES_PCLOSE _pclose
#else
  #include <fcntl.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #define DES_POPEN  popen
  #define DES_PCLOSE pclose
#endif

namespace des {

namespace {

// curl's config syntax: a double-quoted value with \" and \\ escaped.
std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n' || c == '\r') continue;   // a header cannot span lines
        out += c;
    }
    return out + "\"";
}

// A new file only this user can read. On Windows the temporary folder is
// already private to the user profile; elsewhere it is often shared, so the
// file is created 0600 rather than chmod'ed after the key is already in it.
bool writePrivate(const std::string& path, const std::string& text) {
#ifdef _WIN32
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << text;
    return static_cast<bool>(f);
#else
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    std::size_t done = 0;
    while (done < text.size()) {
        const ssize_t n = ::write(fd, text.data() + done, text.size() - done);
        if (n <= 0) { ::close(fd); return false; }
        done += static_cast<std::size_t>(n);
    }
    return ::close(fd) == 0;
#endif
}

std::string tempName(const char* suffix) {
    std::random_device rd;
    std::mt19937_64 gen((static_cast<unsigned long long>(rd()) << 32) ^ rd());
    char buf[40];
    std::snprintf(buf, sizeof buf, "des-llm-%016llx",
                  static_cast<unsigned long long>(gen()));
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
    return ((ec ? std::filesystem::path(".") : dir) / (std::string(buf) + suffix)).string();
}

// curl --config <file>, stdout and stderr together, into `output`.
//
// On Windows NOT through _popen: _popen runs cmd.exe, and from a windowed
// program -- the desktop app has no console -- that opens a console window
// for every request, a black box flashing up and away. CreateProcess with
// CREATE_NO_WINDOW runs curl with no window at all, console app or not.
bool runCurl(const std::string& cfgPath, std::string& output, int& rc) {
#ifdef _WIN32
    // System32's curl by its full path, so a stray curl.exe earlier on PATH --
    // Git's, say, built against a different TLS stack -- is not the one used.
    std::string exe = "curl.exe";
    if (const char* root = std::getenv("SystemRoot")) {
        const std::string sys = std::string(root) + "\\System32\\curl.exe";
        if (std::filesystem::exists(sys)) exe = sys;
    }
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0)) return false;
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writeEnd;
    si.hStdError = writeEnd;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::string cmd = "\"" + exe + "\" --config \"" + cfgPath + "\"";
    const BOOL started = CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                        nullptr, nullptr, &si, &pi);
    CloseHandle(writeEnd);   // ours; the child has its own, so EOF comes when it exits
    if (!started) { CloseHandle(readEnd); return false; }
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(readEnd, buf, sizeof buf, &n, nullptr) && n > 0) output.append(buf, n);
    CloseHandle(readEnd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    rc = static_cast<int>(code);
    return true;
#else
    const std::string cmd = "curl --config '" + cfgPath + "' 2>&1";
    FILE* pipe = DES_POPEN(cmd.c_str(), "r");
    if (pipe == nullptr) return false;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, pipe)) > 0) output.append(buf, n);
    rc = DES_PCLOSE(pipe);
    return true;
#endif
}

}  // namespace

std::string curlConfig(const std::string& url, const std::vector<std::string>& headers,
                       const std::string& bodyPath, int timeoutSeconds) {
    std::string c;
    c += "url = " + quoted(url) + "\n";
    c += "request = \"POST\"\n";
    for (const std::string& h : headers) c += "header = " + quoted(h) + "\n";
    c += "data-binary = " + quoted("@" + bodyPath) + "\n";
    c += "max-time = " + std::to_string(timeoutSeconds) + "\n";
    c += "silent\nshow-error\n";
    // The status on a line of its own after the body: the one thing popen
    // cannot otherwise tell us.
    c += "write-out = \"\\n%{http_code}\"\n";
    return c;
}

HttpResponse CurlTransport::post(const std::string& url, const std::vector<std::string>& headers,
                                 const std::string& body) {
    const std::string bodyPath = tempName(".json");
    struct Cleanup { std::string a; ~Cleanup() { std::remove(a.c_str()); } } cleanup{bodyPath};
    if (!writePrivate(bodyPath, body)) {
        HttpResponse r;
        r.error = "could not write a temporary file for the request";
        return r;
    }
    return run(url, curlConfig(url, headers, bodyPath, m_timeout), tempName(".cfg"));
}

HttpResponse CurlTransport::postForm(const std::string& url, const std::vector<std::string>& headers,
                                     const std::vector<std::pair<std::string, std::string>>& fields,
                                     const std::string& fileField, const std::string& fileName,
                                     const std::string& fileBytes) {
    // The file keeps its extension -- Whisper reads the format from it -- and
    // its path uses forward slashes, which curl's -F syntax takes on Windows
    // without the backslash-escaping questions a "C:\\Users\\User A" path raises.
    const std::size_t dot = fileName.rfind('.');
    const std::string ext = dot == std::string::npos ? std::string(".bin") : fileName.substr(dot);
    const std::string filePath = std::filesystem::path(tempName(ext.c_str())).generic_string();
    struct Cleanup { std::string a; ~Cleanup() { std::remove(a.c_str()); } } cleanup{filePath};
    if (!writePrivate(filePath, fileBytes)) {
        HttpResponse r;
        r.error = "could not write a temporary file for the recording";
        return r;
    }
    const std::string mime = ext == ".wav" ? "audio/wav" : "application/octet-stream";
    return run(url, curlFormConfig(url, headers, fields, fileField, filePath, mime, m_timeout),
               tempName(".cfg"));
}

std::string curlFormConfig(const std::string& url, const std::vector<std::string>& headers,
                           const std::vector<std::pair<std::string, std::string>>& fields,
                           const std::string& fileField, const std::string& filePath,
                           const std::string& mimeType, int timeoutSeconds) {
    std::string c;
    c += "url = " + quoted(url) + "\n";
    for (const std::string& h : headers) c += "header = " + quoted(h) + "\n";
    for (const auto& f : fields) c += "form-string = " + quoted(f.first + "=" + f.second) + "\n";
    c += "form = " + quoted(fileField + "=@" + filePath + ";type=" + mimeType) + "\n";
    c += "max-time = " + std::to_string(timeoutSeconds) + "\n";
    c += "silent\nshow-error\n";
    c += "write-out = \"\\n%{http_code}\"\n";
    return c;
}

HttpResponse CurlTransport::run(const std::string& url, const std::string& config,
                                const std::string& cfgPath) {
    HttpResponse r;
    struct Cleanup { std::string a; ~Cleanup() { std::remove(a.c_str()); } } cleanup{cfgPath};
    if (!writePrivate(cfgPath, config)) {
        r.error = "could not write a temporary file for the request";
        return r;
    }

    std::string output;
    int rc = 0;
    if (!runCurl(cfgPath, output, rc)) { r.error = "could not start curl"; return r; }

    const std::size_t nl = output.find_last_of('\n');
    const std::string last = nl == std::string::npos ? output : output.substr(nl + 1);
    const int status = std::atoi(last.c_str());
    if (status <= 0) {
        // 000, or nothing: curl never got an answer. Its own message is the
        // useful part -- "Could not resolve host", "SSL certificate problem".
        std::string why = nl == std::string::npos ? output : output.substr(0, nl);
        while (!why.empty() && (why.back() == '\n' || why.back() == '\r')) why.pop_back();
        if (why.empty() && rc != 0) why = "curl is not installed, or failed to run";
        r.error = "no response from " + url + (why.empty() ? std::string() : ": " + why);
        return r;
    }
    r.ok = true;
    r.status = status;
    r.body = nl == std::string::npos ? std::string() : output.substr(0, nl);
    return r;
}

}  // namespace des
