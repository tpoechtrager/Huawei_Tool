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

#ifndef __TOOLS_H__
#define __TOOLS_H__

#include <vector>
#include <list>
#include <string>
#include <sstream>
#include <limits>
#include <algorithm>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdarg>

#include <rapidxml.hpp>

#include "compiler.h"
#include "version.h"
#include "cli_tools.h"

// Preprocessor

#define abort() \
do \
{ \
    fprintf(stderr, "ABORT: %s:%d [%s]\n", __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    ::cli::windows::wait(nullptr); \
    abort(); \
} while (0)

#define XSTR(s) STR(s)
#define STR(s) #s

// Time

typedef unsigned long long TimeType;
typedef unsigned int       TimeType32;

enum FmtMillisFlags : int
{
    FMT_YEARS = 0x01,
    FMT_DAYS = 0x02,
    FMT_HOURS = 0x04,
    FMT_MINUTES = 0x08,
    FMT_SECONDS = 0x10,
    FMT_ALL = FMT_YEARS | FMT_DAYS | FMT_HOURS | FMT_MINUTES | FMT_SECONDS
};

// Numeric

constexpr float minVal(float) { return -100000.f; }
template<typename T> constexpr T minVal(T) { return std::numeric_limits<T>::min(); }
template<typename T> constexpr T maxVal(T) { return std::numeric_limits<T>::max(); }

// String

bool vformat(char *text, size_t size, const char *fmt,
             va_list args, size_t *length = nullptr);

struct StrBuf : public std::string
{
    void format(const char *fmt, ...) PRINTFARGS(2, 3);
    void addChar(const char c, const size_t repeat = 1);
    void popChar();
    void fmtMillis(const TimeType millis, const FmtMillisFlags flags = FMT_ALL);
    std::vector<std::string> getLines() const;

    StrBuf &operator=(StrBuf&& strBuf);

    template <typename T>
    void operator=(const T &s) { *this = s; }
};

cxx14_constexpr bool strEqual(const char *str1, const char *str2)
{
    while (*str1 == *str2) 
    {
        if (!*str1) break;
        ++str1;
        ++str2;
    }
    return *str1 == *str2;
}

template <typename BUF, size_t N>
void copystr(BUF (&dst)[N], const char *src, const size_t size = N)
{
    size_t slen = std::min(std::min<size_t>(strlen(src), N), size);
    memcpy(dst, src, slen);
    if (slen == N) slen--;
    dst[slen] = '\0';
}

void copystr(char *dst, const char *src, const size_t size);

bool splitStr(const char *&str, char *buf, size_t size,
              const char *delimiter, size_t *length = nullptr);

bool getLine(const char *&str, char *buf, size_t size);

size_t splitStr(
    std::vector<std::string> &strs,
    const char *str, const char *delimiter,
    bool allowEmpty = true);

size_t splitLines(
    std::vector<std::string> &lines,
    const char *str, bool emptyLines = false);

size_t getTokenLength(const char *&str, const char *delimiter);
size_t getLineLength(const char *&str);
std::list<size_t> getLineLengths(const char *str);
size_t getLongestLineLength(const char *str);
size_t getLongestLineLength(const std::list<size_t> &lineLengths);

void strReplace(std::string &str, const char *needle, const char *replace);

// Crypto

std::string &sha256(const std::string &msg, std::string &result);
std::string &base64(const std::string &msg, std::string &result);

// XML

// Microsoft defines XML_ERROR in their msxml header,
// so we are using __XML_ERROR__ instead.

typedef unsigned long long XMLNumType;

extern const char *__XML_ERROR__;
constexpr XMLNumType __XML_NUM_ERROR__ = 0xFEFCFDFDFDFDFCFC;

const char *getXMLStr(rapidxml::xml_node<> *node, const char *nodeName);
std::string getXMLSubValStr(rapidxml::xml_node<> *node, const char *nodeName, const char *subStrName);

XMLNumType getXMLNum(rapidxml::xml_node<> *node, const char *nodeName);
XMLNumType getXMLHexNum(rapidxml::xml_node<> *node, const char *nodeName);
const char *getIndentation(const size_t indent);

class XMLElementPrinter
{
private:
    std::stringstream ss;
    size_t numElement = 0;
    const char *elements[5];
    size_t indentation;

public:
    void element(const char *_element, const bool newLine = false, size_t _indentation = 0);
    void elementEnd(const bool indent = false);

    template <typename T>
    void printElement(const char *_element, T &&_val)
    {
        element(_element);
        ss << _val;
        elementEnd();
    }

    std::string getStr();
    void reset();

    XMLElementPrinter();
    ~XMLElementPrinter();
};

class XMLNodePrinter
{
private:
    XMLElementPrinter &elementPrinter;

public:
    XMLNodePrinter(XMLElementPrinter &elementPrinter, const char *node);
    ~XMLNodePrinter();
};

// Time

constexpr TimeType oneSecond = 1000;
constexpr TimeType oneMinute = oneSecond * 60;
constexpr TimeType oneHour = oneMinute * 60;
constexpr TimeType oneDay = oneHour * 24;
constexpr TimeType oneYear = oneDay * 365;

// Stop time interpolation after XX milliseconds
constexpr TimeType MAX_INTERPOLATION_MS = 15 * oneSecond;

TimeType getNanoSeconds();

inline TimeType getMicroSeconds() { return getNanoSeconds() / 1000; }
inline TimeType getMilliSeconds() { return getMicroSeconds() / 1000; }

extern TimeType now;
void updateTime();

const std::string &fmtMillis(TimeType millis, StrBuf &buf,
    const FmtMillisFlags flags = FMT_ALL);

#define TP(a, b) (a ? a : b)

cxx14_constexpr TimeType getTimeVal(std::tuple<TimeType, TimeType> vals)
{
    if (std::get<1>(vals) == TimeType()) return std::get<0>(vals);
    return std::get<1>(vals);
}

inline TimeType getElapsedTime(const TimeType timePoint)
{
    return now - timePoint;
}

inline TimeType timeElapsedGE(const TimeType timePoint, const TimeType cmpVal)
{
    return getElapsedTime(timePoint) >= cmpVal;
}

inline TimeType timeElapsedLT(const TimeType timePoint, const TimeType cmpVal)
{
    return getElapsedTime(timePoint) < cmpVal;
}

inline TimeType getTimeDiff(const TimeType a, const TimeType b)
{
    return a - b;
}

inline TimeType getTimeDiffInSeconds(const TimeType a, const TimeType b)
{
    return getTimeDiff(a, b) / 1000;
}

inline TimeType interpolateDuration(const TimeType seconds, const TimeType lastUpdate)
{
    return (seconds * 1000) + getElapsedTime(lastUpdate);
}

extern TimeType delay(TimeType);

template<TimeType N, TimeType IN>
class RequestLimiter
{
private:
    static constexpr TimeType LIMIT = IN / N;
    static constexpr TimeType DELAYSTEP = 50;
    TimeType last = 0;
    TimeType toDelay = 0;
public:
    enum DelayCode : int
    {
        DONE = 0,
        DELAYING = 1
    };

    DelayCode limit()
    {
        updateTime();

        if (!toDelay)
        {
            if (!last || timeElapsedGE(last, LIMIT))
            {
                last = now;
                return DelayCode::DONE;
            }
            toDelay = LIMIT - getElapsedTime(last);
        }

        if (toDelay > 0)
        {
            TimeType delayNow;
            if (toDelay >= DELAYSTEP) delayNow = DELAYSTEP;
            else delayNow = toDelay;
            TimeType elapsed = delay(delayNow);
            if (elapsed > toDelay) elapsed = toDelay;
            toDelay -= elapsed;
            if (!toDelay)
            {
                last = now + elapsed;
                return DelayCode::DONE;
            }
            return DelayCode::DELAYING;
        }

        last = now;
        return DelayCode::DONE;
    }

    RequestLimiter()
    {
        updateTime();
        last = now;
    }
};

// Cross Platform

TimeType delay(TimeType ms);

// Initialization

void initTools();

#endif // __TOOLS_H__
