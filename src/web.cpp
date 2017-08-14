/***************************************************************************
 *  Huawei Tool                                                            *
 *  Copyright (c) 2017 unknown (unknown.lteforum@gmail.com)                *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 **************************************************************************/

#include "web.h"
#include "atomic.h"

#include <map>
#include <vector>

#include <curl/curl.h>
#include <rapidxml.hpp>

#ifdef _WIN32
#undef ERROR
#endif

#define USERAGENT "Huawei Tool/" VERSION " (\"" CODENAME "\")"

#define err_http(result) \
    errfun << result.errorstr << err.endl()

#define err_huawei(result, description) \
    errfun << description << ":" \
           << huaweiErrStr(httpResult.huaweiErrCode) \
           << err.endl()

#define err_huawei_code(errcode, description) \
    errfun << description << ": " \
           << huaweiErrStr(errcode) \
           << err.endl()

extern atomic<bool> shouldExit;
extern atomic<bool> waitingForInput;

namespace web {
char routerIP[128] = "";
char routerUser[128] = "";
char routerPass[128] = "";

namespace {

int loggedIn = 0;
std::map<std::string, std::string> cookies;
bool csrfMethod2 = false;

struct HttpResult
{
    int code;
    std::string contentType;
    std::string content;
    std::string xmlContent;
    rapidxml::xml_document<> xml;
    unsigned long responseCode;
    std::string errorstr;
    HuaweiErrorCode huaweiErrCode;
    std::string huaweiErrStr;

    void reset()
    {
        code = 0;
        contentType.clear();
        content.clear();
        xmlContent.clear();
        responseCode = -1LU;
        huaweiErrCode = HuaweiErrorCode::ERROR;
        huaweiErrStr = ::huaweiErrStr(HuaweiErrorCode::ERROR);
    }

    HttpResult() { reset(); }
};

struct HttpOpts
{
    std::string data;
    std::string contentType;
    std::string csrfToken;

    void reset()
    {
        data.clear();
        contentType.clear();
        csrfToken.clear();
    }

    HttpOpts() { reset(); }
};

void addOrUpdateCookie(const char *cookie)
{
    char name[1024];
    char value[4096];

    if (sscanf(cookie, "%*s   %*s   %*s       %*s   %*u       "
                       "%1023[^ |\t]       %4095s", name, value) == 2)
    {
        if (cookies.find(name) == cookies.end()) dbglog << "New Cookie: ";
        else dbglog << "Updating Cookie: ";
        dbglog << "Name: " << name << " Value: " << value << dbg.endl();
        cookies[name] = cookie;
    }
    else
    {
        dbglog << "Cookie: " << cookie << dbg.endl();
        errfun << "Parsing cookie failed" << err.endl();
    }
}

bool httpRequest(const char *request, HttpResult &result, const HttpOpts opts = {})
{
    if (request[0] != '/')
    {
        errfun << "Request must begin with '/'" << err.endl();
        return false;
    }

    CURL *curl = curl_easy_init();
    if (!curl) abort();

    std::string req = "http://" + std::string(routerIP) + request;
    std::string ref = "http://" + std::string(routerIP) + "/html/home.html";
    struct curl_slist *headers = nullptr;

    dbglog << "### HTTP Request ###" << dbg.endl();
    dbglog << "URL: " << req << dbg.endl();
    dbglog << "Referrer: " << ref << dbg.endl();

    auto callback = [](void *data, size_t size, size_t nmemb, std::string &content)
    {
        content.append((const char *)data, size * nmemb);
        return size * nmemb;
    };

#ifdef cxx14
    auto setopt = [&curl](auto opt1, auto opt2)
    {
        if (curl_easy_setopt(curl, opt1, opt2) != CURLE_OK)
            abort();
    };
#else
    #define setopt(opt1, opt2) \
    do \
    { \
        if (curl_easy_setopt(curl, opt1, opt2) != CURLE_OK) \
            abort(); \
    } while (false)
#endif

    setopt(CURLOPT_CONNECTTIMEOUT, 5L);
    setopt(CURLOPT_TIMEOUT, 120L);
    setopt(CURLOPT_TIMEOUT, 60L);
    setopt(CURLOPT_URL, req.c_str());
    setopt(CURLOPT_REFERER, ref.c_str());
    setopt(CURLOPT_COOKIEFILE, "");
    setopt(CURLOPT_USERAGENT, USERAGENT);

    for (const auto &cookie : cookies)
    {
        dbglog << "Cookie: " << cookie.second << dbg.endl();
        setopt(CURLOPT_COOKIELIST, cookie.second.c_str());
    }

    if (!opts.csrfToken.empty())
    {
        std::string csrfToken = "__RequestVerificationToken: " + opts.csrfToken;
        dbglog << csrfToken << dbg.endl();
        headers = curl_slist_append(headers, csrfToken.c_str());
    }

    headers = curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");

    if (headers)
    {
        setopt(CURLOPT_HTTPHEADER, headers);
    }

    if (!opts.data.empty())
    {
        if (writeDebugLog)
        {
            dbglog << "------- POST Data -------" << dbg.endl();
            dbglog << "\n" << opts.data << dbg.endl();
            dbglog << "------- POST Data End -------" << dbg.endl();
        }

        setopt(CURLOPT_POSTFIELDS, opts.data.c_str());

        if (!opts.contentType.empty())
        {
            std::string contentType = "Content-Type: " + opts.contentType;
            dbglog << contentType << dbg.endl();
            headers = curl_slist_append(headers, contentType.c_str());
        }
    }

    setopt(CURLOPT_WRITEFUNCTION, +callback);
    setopt(CURLOPT_WRITEDATA, &result.content);
    setopt(CURLOPT_NOSIGNAL, 1L);

    again:;
    bool ok = false;

    dbglog << "Performing Request ..." << dbg.endl();
    CURLcode code = curl_easy_perform(curl);
    dbglog << "... done" << dbg.endl();

    if (code == CURLE_OK)
    {
        struct curl_slist *cookies = nullptr;
        struct curl_slist *cookie = nullptr;
        const char *contentType = nullptr;

        if (curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies) != CURLE_OK)
            abort();

        cookie = cookies;

        if (cookie)
        {
            while (cookie)
            {
                addOrUpdateCookie(cookie->data);
                cookie = cookie->next;
            }
            curl_slist_free_all(cookies);
        }

        ok = true;

        if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType) != CURLE_OK)
            abort();

        if (contentType)
        {
            result.contentType = contentType;
            dbglog << "Content Type: " << contentType << dbg.endl();
        }

        if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.responseCode) != CURLE_OK)
            abort();

        dbglog << "Response code: " << result.responseCode << dbg.endl();

        if (result.responseCode != 200)
        {
            std::stringstream errorstr;
            errorstr << "Response Code "<< result.responseCode << "!=200";
            result.errorstr = errorstr.str();
            ok = false;
        }
    }
    else
    {
        result.errorstr = curl_easy_strerror(code);
        dbglog << "Error: " << result.errorstr << dbg.endl();

        if (!shouldExit)
        {
            switch (code)
            {
                case CURLE_AGAIN:
                case CURLE_OPERATION_TIMEDOUT:
                case CURLE_COULDNT_CONNECT:
                case CURLE_NO_CONNECTION_AVAILABLE:
                case CURLE_GOT_NOTHING:
                case CURLE_SEND_ERROR:
                case CURLE_RECV_ERROR:
                {
                    errfun << result.errorstr << err.endl();
                    delay(1000);
                    goto again;
                }
                default:;
            }
        }
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (writeDebugLog)
    {
        std::string dbgContent = result.content;
        if (dbgContent.length() > 512) dbgContent.resize(512);

        while (dbgContent.size() > 0 &&
               (dbgContent[dbgContent.length()-1] == '\r' ||
                dbgContent[dbgContent.length()-1] == '\n'))
            dbgContent.pop_back();

        dbglog << "------- Content -------" << dbg.endl();
        dbglog << "\n" << dbgContent << " [truncated to 512 characters]" << dbg.endl();
        dbglog << "------- Content End -------" << dbg.endl();

        dbglog << "### HTTP Request End ###" << dbg.endl();

        // Do not print an empty line if this is an XML request
        if (strcmp(opts.contentType.c_str(), "application/x-www-form-urlencoded"))
            dbglog << dbg.endl();
    }

    return ok;
}

static bool getCsrfToken(std::string &csrfToken);

bool xmlHttpRequest(const char *request, HttpResult &result, HttpOpts &opts)
{
    dbglog << "### XML Request ###" << dbg.endl();

    bool ok = false;
    opts.contentType = "application/x-www-form-urlencoded";

    if (!opts.data.empty()) // POST requires csrf tokens
    {
        if (opts.csrfToken.empty())
        {
            if (!getCsrfToken(opts.csrfToken))
            {
                ok = false;
                goto end;
            }
        }
    }

    if (!httpRequest(request, result, opts))
    {
        err_http(result);
        ok = false;
        goto end;
    }

#if 0
    if (result.contentType != "text/xml")
    {
        ok = false;
        goto end;
    }
#endif

    dbglog << "Parsing XML" << dbg.endl();

    try
    {
        result.xmlContent = result.content;
        result.xmlContent.push_back('\0');
        result.xml.parse<0>(&result.xmlContent[0]);

        auto *response = result.xml.first_node("response");

        if (response)
        {
            // The server should response with "OK" when we do an XML POST request

            if (!opts.data.empty() && strcmp(response->value(), "OK"))
            {
                err << request << err.endl();
                err << result.content << err.endl();
                err << "Expected \"OK\" response value" << err.endl();
                result.huaweiErrCode = HuaweiErrorCode::ERROR;

                ok = false;
                goto end;
            }

            result.huaweiErrCode = HuaweiErrorCode::OK;
            ok = true;

            goto end;
        }

        if (auto *error = result.xml.first_node("error"))
        {
            if (auto *code = error->first_node("code"))
            {
                result.huaweiErrCode = (HuaweiErrorCode)atoi(code->value());
                result.huaweiErrStr = huaweiErrStr(result.huaweiErrCode);

                dbglog << "Huawei error code: " << result.huaweiErrCode
                       << " (" << result.huaweiErrStr << ")" << dbg.endl();

                ok = true;
            }
        }
    }
    catch (...)
    {
        dbglog << "XML parsing failed" << dbglog.endl();
        ok = false;
    }

    end:;
    dbglog << "### XML Request End ###" << dbg.endl();
    dbglog << dbg.endl();

    return ok;
}

rapidxml::xml_node<> *xmlHttpRequest(
    const char *description, const char *request,
    HttpResult &result, HttpOpts &opts
)
{
    if (!xmlHttpRequest(request, result, opts))
    {
        // err_http(result);
        return nullptr;
    }

    if (result.huaweiErrCode != HuaweiErrorCode::OK)
    {
        err_huawei_code(result.huaweiErrCode, description);
        return nullptr;
    }

    return result.xml.first_node("response");
}

bool getCsrfToken(std::string &csrfToken)
{
    HttpResult httpResult;

    if (!csrfMethod2 && !httpRequest("/", httpResult))
    {
        if (httpResult.responseCode != 307 /* Redirect */)
        {
            err_http(httpResult);
            return false;
        }

        httpResult.reset();
        csrfMethod2 = true;
    }

    if (csrfMethod2 && !httpRequest("/html/home.html", httpResult))
    {
        err_http(httpResult);
        return false;
    }

    const char *str = httpResult.content.c_str();
    char line[4096];
    char token[4096];

    while (getLine(str, line))
    {
        if (sscanf(line, "%*s name=\"csrf_token\" content=\"%4095[^\"]\">", token) != 1)
            continue;

        csrfToken = token;
        return true;
    }

    return true;
}

} // anonymous namespace

HuaweiErrorCode login()
{
    HttpResult httpResult;
    HttpOpts httpOpts;
    std::string csrfToken;
    std::string hashedLogin;

    loggedIn = 0;
    cookies.clear();
    csrfMethod2 = false;

    // Get SessionID cookie + csrfToken

    if (!getCsrfToken(csrfToken))
        return HuaweiErrorCode::ERROR;

    auto response = xmlHttpRequest(
        "Login state",
        "/api/user/state-login",
        httpResult,
        httpOpts
    );

    if (!response) return HuaweiErrorCode::ERROR;
    auto *state = response->first_node("State");
    if (!state) return HuaweiErrorCode::ERROR;

    if (!strcmp(state->value(), "0"))
    {
        // No login required
        loggedIn = 2;
        return HuaweiErrorCode::OK;
    }

    // Can't believe this actually works.

    std::string in;
    std::string out;

    in = std::move(sha256(routerPass, out));
    in = std::move(base64(in, out));
    in = routerUser + in + csrfToken;
    in = std::move(sha256(in, out));
    in = std::move(base64(in, out));
    hashedLogin = std::move(in);

    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("Username", routerUser);
    xml.printElement("Password", hashedLogin);
    xml.printElement("password_type", 4);

    httpResult.reset();
    httpOpts.reset();

    httpOpts.csrfToken = csrfToken;
    httpOpts.data = xml.getStr();

    if (!xmlHttpRequest("Login", "/api/user/login", httpResult, httpOpts))
        return httpResult.huaweiErrCode;

    loggedIn = 1;
    return HuaweiErrorCode::OK;
}

HuaweiErrorCode logout()
{
    switch (loggedIn)
    {
        case 0: return HuaweiErrorCode::ERROR;
        case 1: break;
        case 2: return HuaweiErrorCode::OK;
        default: return HuaweiErrorCode::ERROR;
    }

    loggedIn = 0;

    HttpResult httpResult;
    HttpOpts httpOpts;

    if (!getCsrfToken(httpOpts.csrfToken))
        return HuaweiErrorCode::ERROR;

    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("Logout", 1);

    httpOpts.data = xml.getStr();

    if (!xmlHttpRequest("Logout", "/api/user/logout", httpResult, httpOpts))
        return httpResult.huaweiErrCode;

    return HuaweiErrorCode::OK;
}

bool getAntennaType(AntennaType &type)
{
    HttpResult httpResult;
    HttpOpts httpOpts;

    auto response = xmlHttpRequest(
        "Getting Antenna Type",
        "/api/device/antenna_set_type",
        httpResult,
        httpOpts
    );

    if (!response) return false;
    auto *val = response->first_node("antennasettype");
    if (!val) return false;

    int tmp = atoi(val->value());

    if (tmp >= 0 && tmp <= 2)
    {
        type = (AntennaType)tmp;
        return true;
    }

    return false;
}

bool setAntennaType(const AntennaType type)
{
    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("antennasettype", type);

    web::HttpOpts httpOpts;
    web::HttpResult httpResult;

    httpOpts.data = xml.getStr();

    bool ok;

    ok = xmlHttpRequest(
        "Setting Antenna type",
        "/api/device/antenna_set_type",
        httpResult,
        httpOpts
    ) != nullptr;

    return ok;
}

namespace cli {

bool showAntennaType()
{
    AntennaType type;

    if (!web::getAntennaType(type))
        return false;

    printf("Current Antenna Type: %s\n", getAntennaTypeStr(type));
    return true;
}

bool setAntennaType(const char *antennaType)
{
    AntennaType type = getAntennaTypeFromStr(antennaType);

    if (type == AntennaType::ATENNA_TYPE_ERROR)
        return false;

    if (!web::setAntennaType(type))
        return false;

    return true;
}

bool showPlmn()
{
    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    auto response = web::xmlHttpRequest(
        "Getting PLMN",
        "/api/net/current-plmn",
        httpResult,
        httpOpts
    );

    if (!response) return false;
    auto *state = response->first_node("State");
    auto *fullName = response->first_node("FullName");
    auto *shortName = response->first_node("ShortName");
    auto *numeric = response->first_node("Numeric");
    auto *rat = response->first_node("Rat");

    if (!state || !numeric || !rat || !fullName || !shortName)
        return false;

    printf("Current Network Operator: %s (%s) | PLMN: %s | Rat: %s (%s)\n",
           fullName->value(), shortName->value(),
           numeric->value(), rat->value(),
           getRatStr(atoi(rat->value())));

    return true;
}

bool setPlmn(const char *plmn, const char *plmnMode, const char *plmnRat)
{
    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("Mode", plmnMode);
    xml.printElement("Plmn", plmn);
    xml.printElement("Rat", plmnRat);

    httpOpts.data = xml.getStr();

    bool ok = web::xmlHttpRequest(
        "Setting PLMN",
        "/api/net/register",
        httpResult,
        httpOpts
    ) != nullptr;

    if (httpResult.huaweiErrCode == HuaweiErrorCode::ERROR_NET_REGISTER_NET_FAILED)
    {
        err << "Wrong operator?" << err.endl();
        return false;
    }

    return ok;
}

bool selectPlmn(bool select)
{
    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    auto *response = web::xmlHttpRequest(
        "Getting network operator list",
        "/api/net/plmn-list",
        httpResult,
        httpOpts
    );

    if (shouldExit) return false;

    if (!response) return false;
    auto *networks = response->first_node("Networks");
    if (!networks) return false;

    auto *network = networks->first_node("Network");
    if (!network)
    {
        err << "No Network Operators available" << err.endl();
        return false;
    }

    struct _network
    {
        const char *plmn;
        const char *plmnRat;
    };

    std::vector<_network> _networks;
    int count = 0;

    do
    {
        auto *state = network->first_node("State");
        auto *fullName = network->first_node("FullName");
        auto *shortName = network->first_node("ShortName");
        auto *numeric = network->first_node("Numeric");
        auto *rat = network->first_node("Rat");

        if (!state || !numeric || !rat || !fullName || !shortName)
            return false;

        printf("[%d] | Network: %s (%s) | PLMN: %s | Rat: %s (%s)",
               ++count,
               fullName->value(), shortName->value(),
               numeric->value(), rat->value(),
               getRatStr(atoi(rat->value())));

        if (!strcmp(state->value(), "2"))
            printf(" | [Connected]");

        printf("\n");

        _networks.push_back({numeric->value(), rat->value()});
    }
    while((network = network->next_sibling("Network")));

    if (select && _networks.size() > 0)
    {
        printf("[%u] | [auto]\n", (unsigned)_networks.size()+1u);

        unsigned num;

        do
        {
            printf("\nSelect Network Operator [Number: 1-%u]: ", (unsigned)_networks.size()+1u);
            int c;
            std::string numStr;
            waitingForInput = true;
            while ((c = getchar()) != EOF && c != '\n' && c != '\r') numStr.push_back((char)c);
            waitingForInput = false;
            num = strtoul(numStr.c_str(), nullptr, 10);
            printf("\n");
        } while (num <= 0 || num > _networks.size()+1u);

        if (num == _networks.size()+1)
            return setPlmn("", "0" /* Auto */, "");

        const auto &net = _networks[--num];
        return setPlmn(net.plmn, "1" /* Manually */, net.plmnRat);
    }

    return true;
}

bool showNetworkMode()
{
    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    auto response = web::xmlHttpRequest(
        "Getting band",
        "/api/net/net-mode",
        httpResult,
        httpOpts
    );

    if (!response) return false;
    auto *xmlNetworkMode = response->first_node("NetworkMode");
    if (!xmlNetworkMode) return false;
    auto *xmlNetworkBand = response->first_node("NetworkBand");
    if (!xmlNetworkBand) return false;
    auto *xmlLTEBand = response->first_node("LTEBand");
    if (!xmlLTEBand) return false;

    std::string lteBandStr;
    unsigned long long lteBand = strtoull(xmlLTEBand->value(), nullptr, 16);
    getLTEBandStr((LTEBand)lteBand, lteBandStr);

    unsigned long long networkMode = strtoull(xmlNetworkBand->value(), nullptr, 16);

    printf("Current Network Mode: %s\n", xmlNetworkMode->value());
    printf("Current Network Band: %llX\n", networkMode);
    printf("Current LTE Band: %llX (%s)\n", lteBand, lteBandStr.c_str());

    return true;
}

bool setNetworkMode(const char *networkMode, const char *networkBand, const char *lteBand)
{
    web::HttpResult httpResult;
    web::HttpOpts httpOpts;
    unsigned long long _lteBand = 0;

    if (lteBand[0] == '+') _lteBand = getLTEBandFromStr(lteBand);
    else _lteBand = strtoull(lteBand, nullptr, 16);

    if (_lteBand == LTEBand::LTE_BAND_ERROR)
    {
        err << "Invalid LTE band given" << err.endl();
        return false;
    }

    char lteBandStr[32];
    snprintf(lteBandStr, sizeof(lteBandStr), "%llX", _lteBand);

    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("NetworkMode", networkMode);
    xml.printElement("NetworkBand", networkBand);
    xml.printElement("LTEBand", lteBandStr);

    httpOpts.data = xml.getStr();

    bool ok = web::xmlHttpRequest(
        "Setting network mode",
        "/api/net/net-mode",
        httpResult,
        httpOpts
    ) != nullptr;

    if (httpResult.huaweiErrCode == HuaweiErrorCode::ERROR_SET_NET_MODE_AND_BAND_FAILED)
    {
        info << "See http://"
             << web::routerIP
             << "/api/net/net-mode-list for a list of supported bands. Log in first."
             << info.endl();

        return false;
    }

    if (!ok) return false;

    if (httpResult.huaweiErrCode != HuaweiErrorCode::OK)
        return false;

    return showNetworkMode();
}

bool relay(const char *request, bool loop, int loopDelay)
{
    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    do
    {
        httpResult.reset();
        httpOpts.reset();

        if (!web::httpRequest(request, httpResult, httpOpts))
        {
            err_http(httpResult);
            return false;
        }

        printf("%s\r\n\r\n", httpResult.content.c_str());
        fflush(stdout);
        if (loopDelay > 0) delay(loopDelay);

        disableDebugLog("Relay loop: ");
    } while (loop && !shouldExit);

    enableDebugLog();

    return true;
}

namespace experimental {

bool showSignalStrength(bool noClearScreen)
{
    TimeType last = getMilliSeconds();

    do
    {
        web::HttpResult httpResult;
        web::HttpOpts httpOpts;

        auto *response = web::xmlHttpRequest(
            "Getting Signal Strength",
            "/api/device/signal",
            httpResult,
            httpOpts
        );

        if (!response) return false;

        // TODO: This should be obviously moved.

        struct
        {
            Value<> RSCP;
            Value<> ECIO;
            Value<> RSRP;
            Value<> RSRQ;
            Value<> RSSI;
            Value<> SINR;
            int band;
            Value<unsigned long long> cell;
            int DLBW;
            int UPBW;
            int mode;
            int networkTypeEx;
        } signal;

        signal.RSCP.update(getXMLNum(response, "rscp"));
        signal.ECIO.update(getXMLStr(response, "ecio"));
        signal.RSRP.update(getXMLStr(response, "rsrp"));
        signal.RSRQ.update(getXMLStr(response, "rsrq"));
        signal.RSSI.update(getXMLStr(response, "rssi"));
        signal.SINR.update(getXMLStr(response, "sinr"));
        signal.band = getXMLNum(response, "band");
        signal.cell.update(getXMLStr(response, "cell_id"));
        signal.DLBW = getXMLNum(response, "dlbandwidth");
        signal.UPBW = getXMLNum(response, "upbandwidth");
        signal.mode = getXMLNum(response, "mode");

        // TODO: Handle 0 cell_id

        httpResult.reset();
        httpOpts.reset();

        response = web::xmlHttpRequest(
            "Getting Signal Strength",
            "/api/monitoring/status",
            httpResult,
            httpOpts
        );

        if (!response) return false;

        signal.networkTypeEx = getXMLNum(response, "CurrentNetworkTypeEx");

        if (!noClearScreen) clearScreen();
        printf("[%s] ", getNetworkTypeExStr(signal.networkTypeEx));

        switch (signal.mode)
        {
            case 0:
            {
                printf("[RSSI: %d, CELL: %llX]\n",
                       *signal.RSSI.current, *signal.cell.current);
                break;
            }
            case 2:
            {
                printf("[RSCP: %d, ECIO: %d, RSSI: %d, CELL: %llX]\n",
                       *signal.RSCP.current, *signal.ECIO.current,
                       *signal.RSSI.current, *signal.cell.current);
                break;
            }
            case 7:
            {
                printf("[RSRP: %d, RSRQ: %d, RSSI: %d, SINR: %d] ",
                        *signal.RSRP.current, *signal.RSRQ.current,
                        *signal.RSSI.current, *signal.SINR.current);

                if (signal.band != -1 || signal.DLBW != -1)
                {
                    printf("[%d MHz, %d MHz, %llX]\n",
                             getBandFreq(signal.band),
                             signal.DLBW,
                             *signal.cell.current);
                }
                else // e3372
                {
                    printf("[CELL: %llX]\n", *signal.cell.current);
                }
                break;
            }
            default: printf("\n");
        }

#ifdef _WIN32
        // Reduce console flickering
        if (!noClearScreen) delay(1000);
#endif

        disableDebugLog("Signal strength loop: ");

        TimeType now = getMilliSeconds();
        TimeType elapsed = now - last;

        if (elapsed < 100)
        {
            // Allow up to 10 requests per second
            delay(100 - elapsed);
        }
    } while (!shouldExit);

    enableDebugLog();

    return true;
}

} // namespace experimental
} // namespace cli

void init()
{
    curl_global_init(CURL_GLOBAL_ALL);
}

void deinit()
{
    curl_global_cleanup();
    cookies.clear();
}

} // namespace web
