// ============================================================================
// CurlTransport.hpp  --  v17: HTTPS through the system's own curl
// ============================================================================
// curl ships with Windows 10 (1803) and 11 in System32, and with every macOS,
// so spawning it keeps the package's promise -- nothing to install -- without
// linking WinHTTP on one side and Objective-C's NSURLSession on the other.
//
// THE KEY NEVER TOUCHES A COMMAND LINE, where any process on the machine can
// read it. Headers and URL go in a config file and the body in another, both
// created readable by this user alone and deleted before post() returns.

#pragma once
#include "ModelGenerator.hpp"

namespace des {

class CurlTransport : public ITransport {
public:
    explicit CurlTransport(int timeoutSeconds = 60) : m_timeout(timeoutSeconds) {}
    HttpResponse post(const std::string& url, const std::vector<std::string>& headers,
                      const std::string& body) override;

private:
    int m_timeout;
};

// The curl config text for one request -- separate so a test can check the
// quoting without running curl. `bodyPath` is the file holding the body.
std::string curlConfig(const std::string& url, const std::vector<std::string>& headers,
                       const std::string& bodyPath, int timeoutSeconds);

}  // namespace des
