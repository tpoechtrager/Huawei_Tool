/***************************************************************************
 *  Huawei Tool                                                            *
 *  Copyright (c) 2017-2020 unknown (unknown.lteforum@gmail.com)           *
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
#include "cli_tools.h"

#include <map>
#include <vector>

#include <curl/curl.h>
#include <rapidxml.hpp>

#ifdef _WIN32
#undef ERROR
#endif

#define USERAGENT "Huawei Tool/" VERSION " (\"" CODENAME "\")"

#define err_http(result) \
    errfunf("%s", result.errorStr.c_str())

#define err_huawei(result, description) \
    errfunf("%s: %s", description, \
            huaweiErrStr(httpResult.huaweiErrCode))

#define err_huawei_code(errcode, description) \
    errfunf("%s: %s", description, \
            huaweiErrStr(errcode).c_str())

namespace web {
char routerIP[128] = "";
char routerUser[128] = "";
char routerPass[128] = "";

namespace {

bool inited = false;
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
    std::string errorStr;
    HuaweiErrorCode huaweiErrCode;
    std::string huaweiErrStr;

    void reset()
    {
        code = 0;
        contentType.clear();
        content.clear();
        xmlContent.clear();
        responseCode = 0;
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
        if (cookies.find(name) == cookies.end())
        {
            dbg.linef("New Cookie: Name: %s Value: %s", name, value);
        }
        else
        {
            dbg.linef("Updating Cookie: Name: %s Value: %s", name, value);
        }

        cookies[name] = cookie;
    }
    else
    {
        dbg.linef("Cookie: %s", cookie);
        errfunf("Parsing cookie failed");
    }
}

bool httpRequest(const char *request, HttpResult &result, const HttpOpts opts = {})
{
    if (request[0] != '/')
    {
        errfunf("Request must begin with '/'");
        return false;
    }

    CURL *curl = curl_easy_init();
    if (!curl) abort();

    std::string req = "http://" + std::string(routerIP) + request;
    std::string ref = "http://" + std::string(routerIP) + "/html/home.html";
    struct curl_slist *headers = nullptr;

    dbg.linef("### HTTP Request ###");
    dbg.linef("URL: %s", req.c_str());
    dbg.linef("Referrer: %s", ref.c_str());

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
        dbg.linef("Cookie: %s", cookie.second.c_str());
        setopt(CURLOPT_COOKIELIST, cookie.second.c_str());
    }

    if (!opts.csrfToken.empty())
    {
        std::string csrfToken = "__RequestVerificationToken: " + opts.csrfToken;
        dbg.linef("%s", csrfToken.c_str());
        headers = curl_slist_append(headers, csrfToken.c_str());
    }

    headers = curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");

    if (headers)
    {
        setopt(CURLOPT_HTTPHEADER, headers);
    }

    if (!opts.data.empty())
    {
        dbg.linef("------- POST Data -------");
        dbg.linef("\n%s", opts.data.c_str());
        dbg.linef("------- POST Data End -------");

        setopt(CURLOPT_POSTFIELDS, opts.data.c_str());

        if (!opts.contentType.empty())
        {
            std::string contentType = "Content-Type: " + opts.contentType;
            dbg.linef("%s", contentType.c_str());
            headers = curl_slist_append(headers, contentType.c_str());
        }
    }

    setopt(CURLOPT_WRITEFUNCTION, +callback);
    setopt(CURLOPT_WRITEDATA, &result.content);
    setopt(CURLOPT_NOSIGNAL, 1L);

    again:;
    bool ok = false;

    dbg.linef("Performing Request ...");
    CURLcode code = curl_easy_perform(curl);
    dbg.linef("... done");

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
            dbg.linef("Content Type: %s", contentType);
        }

        if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.responseCode) != CURLE_OK)
            abort();

        dbg.linef("Response code: %lu", result.responseCode);

        if (result.responseCode != 200)
        {
            std::stringstream errorStr;
            errorStr << "Response Code " << result.responseCode << "!=200";
            result.errorStr = errorStr.str();
            ok = false;
        }
    }
    else
    {
        result.errorStr = curl_easy_strerror(code);
        dbg.linef("Error: %s", result.errorStr.c_str());

        if (!checkExit())
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
                    errfunf("%s", result.errorStr.c_str());
                    delay(3000);
                    if (!checkExit()) goto again;
                }
                default:;
            }
        }
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (dbg.isEnabled())
    {
        std::string dbgContent = result.content;
        if (dbgContent.length() > 512) dbgContent.resize(512);

        while (dbgContent.size() > 0 &&
               (dbgContent[dbgContent.length()-1] == '\r' ||
                dbgContent[dbgContent.length()-1] == '\n'))
            dbgContent.pop_back();

        dbg.linef("------- Content -------");
        dbg.linef("\n%s [truncated to 512 characters]", dbgContent.c_str());
        dbg.linef("------- Content End -------");

        dbg.linef("### HTTP Request End ###");

        // Do not print an empty line if this is an XML request
        if (strcmp(opts.contentType.c_str(), "application/x-www-form-urlencoded"))
            dbg.linef(nullptr);
    }

    return ok;
}

static bool getCsrfToken(std::string &csrfToken);

bool xmlHttpRequest(const char *request, HttpResult &result, HttpOpts &opts)
{
    dbg.linef("### XML Request ###");

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

    dbg.linef("Parsing XML");

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
                err.linef("%s", request);
                err.linef("%s", result.content.c_str());
                err.linef("Expected \"OK\" response value");

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
                dbg.linef("Huawei error code: (%d)", result.huaweiErrCode);
                ok = true;
            }
        }
    }
    catch (rapidxml::parse_error &e)
    {
        dbg.linef("XML parsing failed: %s", e.what());
        ok = false;
    }

    end:;
    dbg.linef("### XML Request End ###");
    dbg.linef(nullptr);

    return ok;
}

rapidxml::xml_node<> *
xmlHttpRequest(const char *description, const char *request, HttpResult &result, HttpOpts &opts)
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

    while (getLine(str, line, sizeof(line)))
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
    int tmp = getXMLNum(response, "antennasettype");

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

namespace wlan {

std::map<std::string, Clients> ssids;

bool updateClients()
{
    auto addOrUpdateSsid = [&](std::string ssid) -> ClientVec*
    {
        ClientVec *clients;
        auto it = ssids.find(ssid);

        if (it == ssids.end())
        {
            clients = new std::vector<Client>;
            auto p = std::make_pair<>(std::move(ssid), Clients(uClientVec(clients)));
            auto it = ssids.insert(std::move(p));
            if (!it.second) return nullptr;
        }
        else
        {
            Clients &c = it->second;
            c.referenced = true;
            clients = c.clients.get();
        }

        return clients;
    };

    auto addOrUpdateClient = [&](ClientVec *clients, std::string macAddress)
    {
        for (auto &client : *clients)
        {
            if (client.macAddress == macAddress)
            {
                client.referenced = true;
                return &client;
            }
        }

        clients->push_back(macAddress);
        return &clients->back();
    };

    auto markObjectsUnreferenced = [&]()
    {
        for (auto &it : ssids)
        {
            Clients &c = it.second;
            c.referenced = false;
            ClientVec *clients = c.clients.get();
            for (auto &client : *clients) client.referenced = false;
        }
    };

    auto removeUnreferencedObjects = [&]()
    {
        delete_next_ssid:;
        for (auto it = ssids.begin(); it != ssids.end(); it++)
        {
            Clients &c = it->second;
            if (!c.referenced)
            {
                ssids.erase(it);
                goto delete_next_ssid;
            }
            ClientVec *clients = c.clients.get();
            while (true)
            {
                delete_next_client:;
                for (auto it = clients->begin(); it != clients->end(); it++)
                {
                    Client &client = *it;
                    if (!client.referenced)
                    {
                        clients->erase(it);
                        goto delete_next_client;
                    }
                }
                break;
            }
        }
    };

    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    auto response = web::xmlHttpRequest(
        "Getting WLAN Host List",
        "/api/wlan/host-list",
        httpResult,
        httpOpts
    );

    if (!response) return false;
    auto *hosts = response->first_node("Hosts");
    if (!hosts) return false;
    auto *host = hosts->first_node("Host");

    markObjectsUnreferenced();

    if (host)
    {
        updateTime();

        do
        {
            std::vector<Client> *clients = addOrUpdateSsid(
                getXMLStr(host, "AssociatedSsid")
            );

            if (!clients) return false;

            Client *client = addOrUpdateClient(
                clients,
                getXMLStr(host, "MacAddress")
            );

            const char *ipAddress = getXMLStr(host, "IpAddress");
            if (ipAddress == __XML_ERROR__) ipAddress = "?.?.?.?";
            const char *hostName = getXMLStr(host, "HostName");
            if (ipAddress == __XML_ERROR__) hostName = "??";

            client->ipAddress = ipAddress;
            client->hostName = hostName;
            client->connectionDuration = getXMLNum(host, "AssociatedTime");
            client->lastUpdate = now;
        } while ((host = host->next_sibling("Host")));
    }
    else
    {
        // No clients connected.
    }

    removeUnreferencedObjects();

    return true;
}

} // namespace wlan

namespace cli {
using namespace ::cli;

int trafficColumnSpacing = 40;

bool showAntennaType()
{
    AntennaType type;

    if (!web::getAntennaType(type))
        return false;

    outf("Current Antenna Type: %s\n", getAntennaTypeStr(type));
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

    unsigned rat = getXMLNum(response, "Rat");

    outf("Current Network Operator: %s (%s) | PLMN: %llu | RAT: %u (%s)\n",
         getXMLStr(response, "FullName"),
         getXMLStr(response, "ShortName"),
         getXMLNum(response, "Numeric"),
         rat, getRatStr(rat));

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
        err.linef("Wrong operator?");
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

    if (checkExit()) return false;

    if (!response) return false;
    auto *networks = response->first_node("Networks");
    if (!networks) return false;

    auto *network = networks->first_node("Network");
    if (!network)
    {
        err.linef("No Network Operators available");
        return false;
    }

    struct _network
    {
        std::string plmn;
        std::string rat;
    };

    std::vector<_network> _networks;
    int count = 0;

    do
    {
        unsigned plmn = getXMLNum(network, "Numeric");
        unsigned rat = getXMLNum(network, "Rat");

        const char *plmnStr = getXMLStr(network, "Numeric");
        const char *ratStr = getXMLStr(network, "Rat");

        outf("[%d] | Network: %s (%s) | PLMN: %u | RAT: %u (%s)",
             ++count,
             getXMLStr(network, "FullName"),
             getXMLStr(network, "ShortName"),
             plmn, rat, getRatStr(rat));

        if (getXMLNum(network, "State") == 2)
            outf(" | [Connected]");

        outf("\n");

        _networks.push_back({plmnStr, ratStr});
    }
    while ((network = network->next_sibling("Network")));

    if (select && _networks.size() > 0)
    {
        outf("[%u] | [auto]\n", (unsigned)_networks.size()+1u);

        unsigned num;

        do
        {
            outf("\nSelect Network Operator [Number: 1-%u]: ", (unsigned)_networks.size()+1u);
            int c;
            std::string numStr;
            while ((c = readChar()) != EOF && c != '\n' && c != '\r') numStr.push_back((char)c);
            if (checkExit()) return false;
            num = strtoul(numStr.c_str(), nullptr, 10);
            outf("\n");
        } while (num <= 0 || num > _networks.size()+1u);

        if (num == _networks.size()+1)
            return setPlmn("", "0" /* Auto */, "");

        const auto &net = _networks[--num];
        return setPlmn(net.plmn.c_str(), "1" /* Manually */, net.rat.c_str());
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

    std::string lteBandStr;
    unsigned long long lteBand = getXMLHexNum(response, "LTEBand");
    getLTEBandStr((LTEBand)lteBand, lteBandStr);

    outf("Current Network Mode: %s\n", getXMLStr(response, "NetworkMode"));
    outf("Current Network Band: %llX\n", getXMLHexNum(response, "NetworkBand"));
    outf("Current LTE Band: %llX (%s)\n", lteBand, lteBandStr.c_str());

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
        err.linef("Invalid LTE band given");
        return false;
    }

    StrBuf lteBandStr;
    lteBandStr.format("%llx", _lteBand);

    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("NetworkMode", networkMode);
    xml.printElement("NetworkBand", networkBand);
    xml.printElement("LTEBand", lteBandStr.c_str());

    httpOpts.data = xml.getStr();

    bool ok = web::xmlHttpRequest(
        "Setting network mode",
        "/api/net/net-mode",
        httpResult,
        httpOpts
    ) != nullptr;

    if (httpResult.huaweiErrCode == HuaweiErrorCode::ERROR_SET_NET_MODE_AND_BAND_FAILED)
    {
        info.linef("See http://%s/api/net/net-mode-list for a list of supported bands. "
                   "Log in first.", web::routerIP);
        return false;
    }

    if (!ok) return false;

    if (httpResult.huaweiErrCode != HuaweiErrorCode::OK)
        return false;

    return showNetworkMode();
}

bool relay(const char *request, const char *data, bool loop, int loopDelay)
{
    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    bool isXMLRequest = false;

    if (data)
    {
        isXMLRequest = !strncasecmp(data, "<?xml", 5);

        if (isXMLRequest)
        {
            // Ensure valid XML data has been passed.

            std::string buf;
            rapidxml::xml_document<> xml;

            buf = data;
            buf.push_back('\0');

            try
            {
                xml.parse<0>(&buf[0]);
            }
            catch (rapidxml::parse_error &e)
            {
                errfunf("Invalid XML Request: %s", e.what());
                return false;
            }
        }
    }

    do
    {
        httpResult.reset();
        httpOpts.reset();

        if (data) httpOpts.data = data;
        bool ok;

        if (isXMLRequest) ok = web::xmlHttpRequest(request, httpResult, httpOpts);
        else ok = web::httpRequest(request, httpResult, httpOpts);

        if (!ok && !httpResult.responseCode)
        {
            err_http(httpResult);
            return false;
        }

        if (httpResult.content.empty())
        {
            errfunf("Empty Response");
            return false;
        }

        // Add line breaks to improve readability.
        strReplace(httpResult.content, "><", ">\n<");
        strReplace(httpResult.content, ">\n</", "></");

        outf("%s\r\n\r\n", httpResult.content.c_str());
        if (loopDelay > 0 && !checkExit()) delay(loopDelay);

        disableDebugLog("Relay Loop: ");
    } while (loop && !checkExit());

    enableDebugLog();

    return true;
}

namespace experimental {

bool showSignalStrength()
{
    RequestLimiter<10, 1000> requestLimiter;

    // Avoid name clash with ::signal
    using x::signal;

    auto formatSignalStats = [&](const SignalValue<>::GetType type)
    {
        StrBuf str;

        if (type == SignalValue<>::GET_CURRENT)
        {
            str.format("MODE: %s\n\n", getNetworkTypeExStr((int)signal.networkTypeEx));
            str.format("OPER: %s\n", signal.operatorNameShort.c_str());
            str.format("PLMN: %llu\n\n", signal.PLMN);
        }
        else
        {
            str += "MODE: -\n\n";
            str += "PLMN: -\n";
            str += "NAME: -\n\n";
        }

        switch (signal.mode)
        {
            case 0:
            {
                str.format("\nRSSI: %d\n\nCELL: %llX\n",
                           signal.RSSI.getVal(type), signal.cell);
                break;
            }
            case 2:
            {
                str.format("RSCP: %d\nECIO: %d\nRSSI: %d\n\n",
                           signal.RSCP.getVal(type), signal.ECIO.getVal(type),
                           signal.RSSI.getVal(type));

                if (type == SignalValue<>::GET_CURRENT) str.format("CELL: %llX", signal.cell);
                else str += "CELL: -";

                break;
            }
            case 7:
            {
                str.format("RSRP: %d\nRSRQ: %d\nRSSI: %d\nSINR: %s%d\n\n",
                           signal.RSRP.getVal(type), signal.RSRQ.getVal(type),
                           signal.RSSI.getVal(type), signal.SINR.getVal(type) >= 0.f ? "+" : "",
                           signal.SINR.getVal(type));

                if (signal.CQI[0].isSet() && signal.CQI[1].isSet())
                {
                     str.format("CQI 0: %d\nCQI 1: %d\n\n",
                                signal.CQI[0].getVal(type), signal.CQI[1].getVal(type));
                }

                if (signal.DLMCS[0].isSet() && signal.DLMCS[1].isSet() && signal.UPMCS.isSet())
                {
                     str.format("DL MCS 0: %d\nDL MCS 1: %d\nUP MCS: %d\n\n",
                                signal.DLMCS[0].getVal(type), signal.DLMCS[1].getVal(type),
                                signal.UPMCS.getVal(type));
                }

                if (signal.TXPWrPPUSCH.isSet() && signal.TXPWrPPUCCH.isSet() &&
                    signal.TXPWrPSRS.isSet() && signal.TXPWrPPRACH.isSet())
                {
                    str.format("TX PPusch: %d\nTX PPucch: %d\nTX PSrs: %d\nTX PPrach: %d\n\n",
                                signal.TXPWrPPUSCH.getVal(type), signal.TXPWrPPUCCH.getVal(type),
                                signal.TXPWrPSRS.getVal(type), signal.TXPWrPPRACH.getVal(type));
                }

                if (signal.band != __XML_NUM_ERROR__ &&
                    signal.DLBW != __XML_NUM_ERROR__ &&
                    signal.UPBW != __XML_NUM_ERROR__)
                {
                    if (type == SignalValue<>::GET_CURRENT)
                    {
                        str.format("FREQ: %d MHz\nDLBW: %llu MHz\nUPBW: %llu MHz\n\n",
                                   getBandFreq((int)signal.band), signal.DLBW, signal.UPBW);
                    }
                    else
                    {
                        str += "FREQ: -\nDLBW: -\nUPBW: -\n\n";
                    }
                }

                if (type == SignalValue<>::GET_CURRENT) str.format("CELL: %llX", signal.cell);
                else str += "CELL: -";

                break;
            }
        }

        if (!str.empty()) str.append("\n\n", 2);
        return str.getLines();
    };

    auto printSignalStats = [&]()
    {
    #warning conf (parameter too)
        std::vector<status::Column> columns;

        columns.push_back({"Current", formatSignalStats(SignalValue<>::GET_CURRENT)});
        columns.push_back({"Average", formatSignalStats(SignalValue<>::GET_AVERAGE)});
        columns.push_back({"Min", formatSignalStats(SignalValue<>::GET_MIN)});
        columns.push_back({"Max", formatSignalStats(SignalValue<>::GET_MAX)});

        status::addColumns(columns, 30);
        status::show();
    };

    do
    {
        web::HttpResult httpResult;
        web::HttpOpts httpOpts;

        // /api/device/signal

        auto *response = web::xmlHttpRequest(
            "Getting Signal Strength",
            "/api/device/signal",
            httpResult,
            httpOpts
        );

        if (!response) return false;

        signal.RSCP.update(getXMLNum(response, "rscp"));
        signal.ECIO.update(getXMLStr(response, "ecio"));
        signal.RSRP.update(getXMLStr(response, "rsrp"));
        signal.RSRQ.update(getXMLStr(response, "rsrq"));
        signal.RSSI.update(getXMLStr(response, "rssi"));
        signal.SINR.update(getXMLStr(response, "sinr"));

        signal.CQI[0].update(getXMLStr(response, "cqi0"));
        signal.CQI[1].update(getXMLStr(response, "cqi1"));

        signal.DLMCS[0].update(getXMLSubValStr(response, "dl_mcs", "mcsDownCarrier1Code0:"));
        signal.DLMCS[1].update(getXMLSubValStr(response, "dl_mcs", "mcsDownCarrier1Code1:"));
        signal.UPMCS.update(getXMLSubValStr(response, "ul_mcs", "mcsUpCarrier1:"));

        signal.TXPWrPPUSCH.update(getXMLSubValStr(response, "txpower", "PPusch:"));
        signal.TXPWrPPUCCH.update(getXMLSubValStr(response, "txpower", "PPucch:"));
        signal.TXPWrPSRS.update(getXMLSubValStr(response, "txpower", "PSrs:"));
        signal.TXPWrPPRACH.update(getXMLSubValStr(response, "txpower", "PPrach:"));

        signal.band = getXMLNum(response, "band");
        signal.cell = getXMLNum(response, "cell_id");
        signal.DLBW = getXMLNum(response, "dlbandwidth");
        signal.UPBW = getXMLNum(response, "ulbandwidth");
        signal.mode = getXMLNum(response, "mode");

        httpResult.reset();
        httpOpts.reset();

        // /api/monitoring/status

        response = web::xmlHttpRequest(
            "Getting Network Type",
            "/api/monitoring/status",
            httpResult,
            httpOpts
        );

        if (!response) return false;

        signal.networkTypeEx = getXMLNum(response, "CurrentNetworkTypeEx");

        httpResult.reset();
        httpOpts.reset();
#warning lower
        // /api/net/current-plmn

        response = web::xmlHttpRequest(
            "Getting PLMN",
            "/api/net/current-plmn",
            httpResult,
            httpOpts
        );

        if (!response) return false;

        signal.operatorName = getXMLStr(response, "FullName");
        signal.operatorNameShort = getXMLStr(response, "ShortName");
        signal.PLMN = getXMLNum(response, "Numeric");

        disableDebugLog("Signal Strength Loop: ");

        printSignalStats();
        while (requestLimiter.limit()) printSignalStats();
    } while (!checkExit());

    enableDebugLog();
    status::exit();

    return true;
}

} // namespace experimental

bool showWlanClients()
{
    // Clients are tracked instead of just printed.

    auto printWlanClients = [&]()
    {
        if (wlan::ssids.empty())
        {
            status::append("No clients connected!");
            status::show();
            return;
        }

        bool first = true;
        StrBuf connectionDuration;

        updateTime();

        for (auto &it : wlan::ssids)
        {
            const std::string &ssid = it.first;
            const wlan::ClientVec *clients = it.second.clients.get();
            unsigned clientNum = 1;

            if (first)
            {
                status::addChar('-', 80);
                status::addChar('\n');
                first = false;
            }

            status::format("%s (%zu):\n\n", ssid.c_str(), clients->size());

            for (auto &client : *clients)
            {
                const TimeType millis = client.getInterpolatedConnectionDuration();
                connectionDuration.fmtMillis(millis);

                status::format("[%d] | IP: %s | Mac: %s | Duration: %s\n",
                               clientNum, client.ipAddress.c_str(),
                               client.macAddress.c_str(), connectionDuration.c_str());

                connectionDuration.clear();

                clientNum++;
            }

            status::addChar('-', 80);
            status::show();
        }
    };

    // Perform only one request per ten seconds
    // to avoid slowing down the WebUI too much.

    RequestLimiter<1, oneSecond * 10> requestLimiter;

    do
    {
        if (!wlan::updateClients()) return false;
        disableDebugLog("WLAN Clients Loop: ");
        printWlanClients();
        while (requestLimiter.limit() && !checkExit()) printWlanClients();
    } while (!checkExit());

    enableDebugLog();
    status::exit();

    return true;
}

bool showTraffic()
{
    RequestLimiter<1, 2000> requestLimiter;

    TrafficStats currentTraffic("Current");
    TrafficStats totalTraffic("Total");
    TrafficStats monthlyTraffic("Monthly");

    web::HttpResult httpResult;
    web::HttpOpts httpOpts;

    auto printTrafficStats = [&]()
    {
        auto formaTrafficStats = [&](const TrafficStats &traffic)
        {
            updateTime();

            TimeType trafficTimeDuration = TimeType(-1);
            StrBuf connectionDuration;
            StrBuf dlTraffic;
            StrBuf upTraffic;
            StrBuf str;

            if (*traffic.CD.current > 0)
            {
                const TimeType millis = traffic.CD.getInterpolatedDuration();
                connectionDuration.fmtMillis(millis);
            }
            else
            {
                connectionDuration = "0s";
            }

            if (&traffic != &currentTraffic)
            {
                // If this isn't the "Current" column then
                // we want overall traffic statistics.

                trafficTimeDuration = traffic.CD.getDuration();
            }

            str.format("Duration:  %s\n", connectionDuration.c_str());
            str.addChar('\n');
            str.format("Download:  %s\n", traffic.DL.getTrafficStr(dlTraffic).c_str());
            str.format("Upload:    %s\n", traffic.UP.getTrafficStr(upTraffic).c_str());
            str.addChar('\n');
            str.format("Speed DL:  %.3f Mbit/s\n", traffic.DL.getAvgSpeedInMbits(trafficTimeDuration));
            str.format("Speed UP:  %.3f Mbit/s\n", traffic.UP.getAvgSpeedInMbits(trafficTimeDuration));
            str.addChar('\n');

            return str.getLines();
        };

        std::vector<status::Column> trafficStatsColumns;

        trafficStatsColumns.push_back({"Current", formaTrafficStats(currentTraffic)});
        trafficStatsColumns.push_back({"Monthly", formaTrafficStats(monthlyTraffic)});
        trafficStatsColumns.push_back({"Total", formaTrafficStats(totalTraffic)});

        status::addColumns(trafficStatsColumns, trafficColumnSpacing);
        status::show();
    };

    auto getTrafficStats = [&](TrafficStats &traffic, const char *desc, bool cached = false)
    {
        rapidxml::xml_node<> *response;

        if (!cached)
        {
            std::string request = "/api/monitoring/";

            if (!strcmp(desc, "CurrentMonth")) request += "month_statistics";
            else request += "traffic-statistics";

            httpResult.reset();
            httpOpts.reset();

            response = web::xmlHttpRequest(
                "Getting Traffic Stats",
                request.c_str(),
                httpResult,
                httpOpts
            );

            if (!response) return false;
        }
        else
        {
            response = httpResult.xml.first_node("response");
        }

        if (response->first_node("showtraffic"))
        {
            unsigned long long showTraffic = getXMLNum(response, "showtraffic");
            if (showTraffic == __XML_NUM_ERROR__) return false;

            if (showTraffic == 0)
            {
                errfunf("showtraffic is set to '0'");
                return false;
            }
        }

        std::string connectTime = std::string(desc) + "ConnectTime";
        std::string currentDownload = std::string(desc) + "Download";
        std::string currentUpload = std::string(desc) + "Upload";

        if (!strcmp(desc, "CurrentMonth")) connectTime = "MonthDuration";

        updateTime();

        traffic.CD.update(getXMLNum(response, connectTime.c_str()));
        traffic.DL.update(getXMLNum(response, currentDownload.c_str()));
        traffic.UP.update(getXMLNum(response, currentUpload.c_str()));

        return traffic.isSet();
    };

    do
    {
        if (!getTrafficStats(currentTraffic, "Current")) return false;
        if (!getTrafficStats(totalTraffic, "Total", true)) return false;
        if (!getTrafficStats(monthlyTraffic, "CurrentMonth")) return false;

        printTrafficStats();
        disableDebugLog("Traffic Loop: ");

        while (requestLimiter.limit() && !checkExit()) printTrafficStats();
    } while (!checkExit());

    enableDebugLog();
    status::exit();

    return true;
}

bool connect(const int action)
{
    HttpResult httpResult;
    HttpOpts httpOpts;

    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("dataswitch", action);

    httpOpts.data = xml.getStr();

    bool rc;

    rc = xmlHttpRequest(
        "Connect", "/api/dialup/mobile-dataswitch",
        httpResult, httpOpts
    ) != nullptr;

    return rc;
}

bool disconnect()
{
    return connect(0);
}

bool reboot()
{
    HttpResult httpResult;
    HttpOpts httpOpts;

    XMLElementPrinter xml;
    XMLNodePrinter xmlNodeRequest(xml, "request");
    xml.printElement("Control", 1);

    httpOpts.data = xml.getStr();

    bool rc;

    rc = xmlHttpRequest(
        "Reboot", "/api/device/control",
        httpResult, httpOpts
    ) != nullptr;

    return rc;
}

} // namespace cli

void init()
{
    curl_global_init(CURL_GLOBAL_ALL);
    inited = true;
}

void deinit()
{
    if (!inited) return;
    curl_global_cleanup();
    cookies.clear();
    wlan::ssids.clear();
    inited = false;
}

} // namespace web
