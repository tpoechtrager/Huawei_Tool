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

#include "tools.h"
#include "cli_tools.h"
#include "atomic.h"

#include <cstdarg>
#include <sys/time.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/base64.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// String

bool vformat(char *text, size_t size, const char *fmt, va_list args, size_t *length)
{
    int ret;
    ret = vsnprintf(text, size, fmt, args);
    if (length) *length = ret;
    return ret > 0 && (size_t)ret < size;
}

void StrBuf::format(const char *fmt, ...)
{
    char buf[16384];
    size_t length;

    va_list args;
    va_start(args, fmt);
    bool ok = vformat(buf, sizeof(buf), fmt, args, &length);
    va_end(args);

    if (!ok) return;

    append(buf, length);
}

void StrBuf::addChar(const char c, const size_t repeat)
{
    append(repeat, c);
}

void StrBuf::popChar()
{
    if (empty()) return;
    pop_back();
}

void StrBuf::fmtMillis(const TimeType millis, const FmtMillisFlags flags)
{
    ::fmtMillis(millis, *this, flags);
}

std::vector<std::string> StrBuf::getLines() const
{
    std::vector<std::string> lines;
    splitLines(lines, c_str(), true);
    return lines;
}

StrBuf &StrBuf::operator=(StrBuf&& strBuf)
{
    std::string &s = *this;
    s = std::move(strBuf);
    return *this;
}

void strReplace(std::string &str, const char *needle, const char *replace)
{
    size_t needleLength = strlen(needle);
    size_t replaceLength = strlen(replace);
    size_t pos = 0;

    while ((pos = str.find(needle, pos)) != std::string::npos)
    {
        str.replace(pos, needleLength, replace);
        pos += replaceLength;
    }
}

void copystr(char *dst, const char *src, const size_t size)
{
    size_t slen = std::min<size_t>(strlen(src), size);
    memcpy(dst, src, slen);
    if (slen == size) slen--;
    dst[slen] = '\0';
}

bool splitStr(const char *&str, char *buf, size_t size,
              const char *delimiter, size_t *length)
{
    const char *start = str;
    const char *lineEnd = str;

    if (!*str) return false;

    auto isSplitDelimiter = [&]()
    {
        const char *d = delimiter;

        while (*d)
        {
            if (*str == *d) return true;
            d++;
        }

        return false;
    };

    while (*str)
    {
        if (isSplitDelimiter()) break;
        lineEnd = ++str;
    }

    if (isSplitDelimiter()) str++;

    const size_t realLength = lineEnd - start;

    if (buf)
    {
        size_t charsToCopy = std::min<size_t>(realLength, size - 1);
        memcpy(buf, start, charsToCopy);
        buf[charsToCopy] = '\0';
        if (length) *length = charsToCopy;
    }
    else if (length)
    {
        *length = realLength;
    }

    return true;
}

bool getLine(const char *&str, char *buf, size_t size)
{
    return splitStr(str, buf, size, "\r\n");
}

size_t splitStr(std::vector<std::string> &strs, const char *str,
                const char *delimiter, bool allowEmpty)
{
    char buf[16384];
    size_t length;

    while (splitStr(str, buf, sizeof(buf), delimiter, &length))
    {
        if (allowEmpty || length > 0) strs.push_back({buf, length});
    }

    return strs.size();
}

size_t splitLines(std::vector<std::string> &lines, const char *str, bool emptyLines)
{
    return splitStr(lines, str, "\r\n", emptyLines);
}

size_t getTokenLength(const char *&str, const char *delimiter)
{
    size_t length;
    if (splitStr(str, nullptr, 0, delimiter, &length)) return length;
    return size_t(-1);
}

size_t getLineLength(const char *&str)
{
    return getTokenLength(str, "\r\n");
}

std::list<size_t> getLineLengths(const char *str)
{
    std::list<size_t> lineLengths;
    size_t lineLength;

    while ((lineLength = getLineLength(str)) != size_t(-1))
    {
        lineLengths.push_back(lineLength);
    }

    return lineLengths;
}

size_t getLongestLineLength(const char *str)
{
    size_t lineLength;
    size_t longestLineLength = 0;

    while ((lineLength = getLineLength(str)) != size_t(-1))
    {
        if (lineLength > longestLineLength) longestLineLength = lineLength;
    }

    return longestLineLength;
}

size_t getLongestLineLength(const std::list<size_t> &lineLengths)
{
    size_t longestLineLength = 0;

    for (size_t lineLength : lineLengths)
    {
        if (lineLength > longestLineLength)
        {
            longestLineLength = lineLength;
        }
    }

    return longestLineLength;
}

// Crypto

std::string &sha256(const std::string &msg, std::string &result)
{
    result.clear();

    CryptoPP::SHA256 hash;
    CryptoPP::byte digest[CryptoPP::SHA256::DIGESTSIZE];
    hash.CalculateDigest(digest, (CryptoPP::byte*)msg.c_str(), msg.length());

    CryptoPP::HexEncoder encoder;
    encoder.Attach(new CryptoPP::StringSink(result));
    encoder.Put(digest, sizeof(digest));
    encoder.MessageEnd();

    std::transform(result.begin(), result.end(), result.begin(), ::tolower);

    return result;
}

std::string &base64(const std::string &msg, std::string &result)
{
    result.clear();

    CryptoPP::StringSource(msg, true,
        new CryptoPP::Base64Encoder(new CryptoPP::StringSink(result), false)
    );

    return result;
}

// XML

const char *__XML_ERROR__ = "- XML Error -";

const char *getXMLStr(rapidxml::xml_node<> *node, const char *nodeName)
{
    auto *result = node->first_node(nodeName);
    if (result) return result->value();
    return __XML_ERROR__;
}

std::string getXMLSubValStr(rapidxml::xml_node<> *node, const char *nodeName, const char *subValName)
{
    const char *xmlStr = getXMLStr(node, nodeName);

    if (!strcmp(xmlStr, __XML_ERROR__))
        return __XML_ERROR__;

    std::vector<std::string> subVals;
    const size_t subValNameLength = strlen(subValName);

    if (!splitStr(subVals, xmlStr, " ", false))
        return __XML_ERROR__;

    for (const std::string &subVal : subVals)
    {
        if (!subVal.compare(0, subValNameLength, subValName))
            return subVal.c_str() + subValNameLength;
    }

    return __XML_ERROR__;
}

XMLNumType getXMLNum(rapidxml::xml_node<> *node, const char *nodeName)
{
    auto *result = node->first_node(nodeName);
    if (result) return strtoull(result->value(), nullptr, 10);
    return __XML_NUM_ERROR__;
}

XMLNumType getXMLHexNum(rapidxml::xml_node<> *node, const char *nodeName)
{
    auto *result = node->first_node(nodeName);
    if (result) return strtoull(result->value(), nullptr, 16);
    return __XML_NUM_ERROR__;
}

const char *getIndentation(const size_t indent)
{
    if (indent > 4) abort();
    constexpr const char *str[] = {"", " ", "  ", "   ", "    "};
    return str[indent];
}

void XMLElementPrinter::element(const char *_element, const bool newLine, size_t _indentation)
{
    elements[numElement] = _element;
    if (numElement) ++indentation;
    else indentation = _indentation;
    ss << getIndentation(indentation) << '<' << elements[numElement] << '>';
    if (newLine) ss << "\n";
    ++numElement;
}

void XMLElementPrinter::elementEnd(const bool indent)
{
    if (!numElement) return;
    if (indent) ss << getIndentation(indentation);
    ss << "</" << elements[numElement - 1] << ">";
    if (numElement > 1 && indentation) --indentation;
    --numElement;
    if (numElement > 0) ss << '\n';
}

std::string XMLElementPrinter::getStr()
{
    while (numElement) elementEnd(true);
    return ss.str();
}

void XMLElementPrinter::reset()
{
    numElement = 0;
    indentation = 0;
    ss.str(std::string());
}

XMLElementPrinter::XMLElementPrinter()
{
    ss << "<?xml version=\"1.0\" ?>\n";
}

XMLElementPrinter::~XMLElementPrinter() {}

XMLNodePrinter::XMLNodePrinter(XMLElementPrinter &elementPrinter, const char *node)
                             : elementPrinter(elementPrinter)
{
    elementPrinter.element(node, true);
}

XMLNodePrinter::~XMLNodePrinter()
{
    elementPrinter.elementEnd(true);
}

// Time

TimeType nanoSecondsBase;
TimeType now;

#ifdef _WIN32
ULONGLONG WINAPI (*GetTickCount64Ptr)();
#endif

TimeType getNanoSeconds()
{
#ifdef _WIN32
    DWORD ticks = GetTickCount64Ptr ? GetTickCount64Ptr() : GetTickCount();
    return TimeType(ticks * 1000000LL) - nanoSecondsBase;
#elif defined(__linux__) && !defined(USE_GETTIMEOFDAY)
    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
        return TimeType(((tp.tv_sec * 1000000000LL) + tp.tv_nsec)) - nanoSecondsBase;
#endif // WIN32
#ifndef _WIN32
    struct timeval tv;
    if (gettimeofday(&tv, nullptr) == 0)
        return TimeType(((tv.tv_sec * 1000000000LL) + (tv.tv_usec * 1000))) - nanoSecondsBase;
    abort();
#endif
}

void updateTime()
{
    now = getMilliSeconds();
}

static void initNanoClock()
{
#ifdef _WIN32
    HMODULE kernel32 = GetModuleHandle("kernel32.dll");
    if (kernel32)
    {
        FARPROC ptr = GetProcAddress(kernel32, "GetTickCount64");
        if (ptr) GetTickCount64Ptr = (decltype(GetTickCount64Ptr))ptr;
    }
#endif
    nanoSecondsBase = getNanoSeconds();
}

const std::string &fmtMillis(TimeType millis, StrBuf &buf, const FmtMillisFlags flags)
{
    auto getVal = [&](const TimeType timePeriod)
    {
        unsigned int val = millis / timePeriod;
        millis %= timePeriod;
        return val;
    };

    const unsigned int years = getVal(oneYear);
    const unsigned int days = getVal(oneDay);
    const unsigned int hours = getVal(oneHour);
    const unsigned int minutes = getVal(oneMinute);
    const unsigned int seconds = getVal(oneSecond);

    auto printVal = [&](const unsigned int val, const char spec, const FmtMillisFlags flag)
    {
        if ((flags & flag) != flag || !val) return;
        buf.format("%u%c ", val, spec);
    };

    printVal(years, 'y', FMT_YEARS);
    printVal(days, 'd', FMT_DAYS);
    printVal(hours, 'h', FMT_HOURS);
    printVal(minutes, 'm', FMT_MINUTES);
    printVal(seconds, 's', FMT_SECONDS);

    if (buf.empty()) buf = ((flags & FMT_SECONDS) == FMT_SECONDS) ? "0s" : "-";
    else buf.popChar(); // space

    return buf;
}

// Cross Platform

#ifdef _WIN32

TimeType delay(TimeType ms)
{
    TimeType start = getMilliSeconds();
    Sleep(ms);
    return getMilliSeconds() - start;
}

#else

TimeType delay(TimeType ms)
{
    TimeType start = getMilliSeconds();
    usleep(ms * 1000);
    return getMilliSeconds() - start;
}

#endif /* _WIN32 */

// GLIBC Hacks

#if defined(__linux__) && defined(__x86_64__)

/*
 * Get rid of the glibc 2.14 reference.
 * memmove() is the same as memcpy() with one more extra if().
 */

// objdump -T huawei_band_tool | grep GLIBC | awk '{print $5}' | sort -u

extern "C" {

OPTNONE USED
void *memcpy(void *__restrict dest, const void *__restrict src, size_t n)
{
    return memmove(dest, src, n);
}

}

#endif

// Initialization

void initTools()
{
    initNanoClock();
}
