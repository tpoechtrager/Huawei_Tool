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

#include <string>
#include <sstream>
#include <fstream>
#include <limits>
#include <algorithm>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "atomic.h"

#include <rapidxml.hpp>

#include "compiler.h"
#include "version.h"

#define abort() \
do \
{ \
    fprintf(stderr, "ABORT: %s:%d [%s]\n", __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    abort(); \
} while (0)

// Numeric

constexpr float minVal(float) { return -100000.f; }
template<typename T> constexpr T minVal(T) { return std::numeric_limits<T>::min(); }
template<typename T> constexpr T maxVal(T) { return std::numeric_limits<T>::max(); }

// String

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
void copystr(BUF (&dst)[N], const char *src)
{
    size_t slen = std::min<size_t>(strlen(src), N);
    memcpy(dst, src, slen);
    if (slen == N) slen--;
    dst[slen] = '\0';
}

template <typename BUF, size_t N>
bool getLine(const char *&str, BUF (&buf)[N])
{
    static_assert(N > 1, "");
    const char *start = str;
    const char *lineEnd = str;

    if (!*str) return false;

    auto isLineEnd = [&]()
    {
        return *str == '\r' || *str == '\n';
    };

    while (*str)
    {
        if (isLineEnd()) break;
        lineEnd = ++str;
    }

    while (isLineEnd()) str++;

    size_t length = std::min<size_t>(lineEnd - start, N - 1);
    memcpy(buf, start, length);
    buf[length] = '\0';

    return true;
}

// Crypto

std::string &sha256(const std::string &msg, std::string &result);
std::string &base64(const std::string &msg, std::string &result);

// XML

const char *getXMLStr(rapidxml::xml_node<> *node, const char *nodeName);
unsigned long long getXMLNum(rapidxml::xml_node<> *node, const char *nodeName);
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

    template <typename T> void printElement(const char *_element, T &&_val)
    {
        element(_element);
        ss << _val;
        elementEnd();
    }

    template <typename T> XMLElementPrinter &operator<<(const T &v)
    {
        ss << v;
        return *this;
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
    XMLNodePrinter(XMLElementPrinter &elementPrinter, const char *node)
            : elementPrinter(elementPrinter)
    {
        elementPrinter.element(node, true);
    }
    ~XMLNodePrinter()
    {
        elementPrinter.elementEnd(true);
    }
};

// Time

typedef uint64_t TimeType;
typedef uint32_t TimeType32;

constexpr TimeType oneSecond = 1 * 1000;
constexpr TimeType oneMinute = 60 * 1000;
constexpr TimeType oneHour = 60 * 60 * 1000;
constexpr TimeType oneDay = 24 * 60 * 60 * 1000;

extern TimeType now;

TimeType getNanoSeconds();
void updateTime();

inline TimeType getMicroSeconds() { return getNanoSeconds() / 1000; }
inline TimeType getMilliSeconds() { return getMicroSeconds() / 1000; }

#ifdef cxx14
#define TT(a, b) {a, b}
#else
#define TT(a, b) std::make_tuple(a, b)
#endif

cxx14_constexpr TimeType getTimeVal(std::tuple<TimeType, TimeType> vals)
{
    if (std::get<1>(vals) == TimeType()) return std::get<0>(vals);
    return std::get<1>(vals);
}

inline TimeType timeElapsedGE(const TimeType timePoint, const TimeType cmpVal)
{
    return now - timePoint >= cmpVal;
}

inline TimeType getTimeDiff(const std::tuple<TimeType, TimeType> a,
                            const std::tuple<TimeType, TimeType> b)
{
    return getTimeVal(a) - getTimeVal(b);
}

inline TimeType getTimeDiffInSeconds(const std::tuple<TimeType, TimeType> a,
                                     const std::tuple<TimeType, TimeType> b)
{
    return getTimeDiff(a, b) / 1000;
}

// Cross Platform

#ifdef _WIN32
#define snprintf _snprintf
extern "C" char *strsep(char **stringp, const char *delim);
#endif

void w32ConsoleWait(const char *msg = nullptr);
void clearScreen();
void delay(unsigned ms);

// Debugging

extern bool writeDebugLog;

class Message
{
private:
    const char *msg;
    std::ostream *os;
    bool printprefix;
    const bool *enabled;
public:
    static constexpr char endl() { return '\n'; }
    bool isendl(char c) { return c == '\n'; }
    template<typename T>
    bool isendl(T&&) { return false; }

    void assignStream(std::ostream &os_)
    {
        os = &os_;
        msg = nullptr;
    }

    template<typename T>
    Message &operator<<(T &&v)
    {
        std::ios::pos_type pos = os->tellp();
        if (pos != std::ios::pos_type(-1) && pos >= 5LL*1024LL*1024LL)
        {
            *os << "[Reached logfile limit]\n" << std::endl;
            return *this;
        }
        if (enabled && !*enabled)
        {
            return *this;
        }
        if (printprefix && !isendl(v) /* ignore empty lines */)
        {
              *os << "[" << getMilliSeconds() << "] ";
              if (msg) *os << msg << ": ";
              printprefix = false;
        }
        if (isendl(v))
        {
            printprefix = true;
            *os << std::endl;
        }
        else *os << v;
        return *this;
    }

    Message(const char *msg, std::ostream &os, bool *enabled = nullptr)
      : msg(msg), os(&os), printprefix(true), enabled(enabled) {}
};

extern Message warn;
extern Message err;
extern Message dbg;
extern Message info;

void disableDebugLog(const char *msg = "");
void enableDebugLog();

#define fun_log(stream) \
    stream << "File:  " <<  __FILE__ \
           << "  Line:  " << __LINE__ \
           << "  Function:  " << __PRETTY_FUNCTION__ << ":  "

#define dbglog dbg //fun_log(dbg)
#define errfun fun_log(err)

// Initialization

void initTools();

#endif // __TOOLS_H__
