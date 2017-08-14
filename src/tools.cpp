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

#include <sys/time.h>
#include <csignal>
#include <iostream>
#include <cryptopp/cryptlib.h>
#include <cryptopp/sha.h>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/base64.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include "version.h"
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

extern atomic<bool> w32ExitInstantly;
extern atomic<bool> waitingForInput;
extern atomic<bool> shouldExit;

// Crypto

std::string &sha256(const std::string &msg, std::string &result)
{
    result.clear();

    CryptoPP::SHA256 hash;
    byte digest[CryptoPP::SHA256::DIGESTSIZE];
    hash.CalculateDigest(digest, (byte*)msg.c_str(), msg.length());

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

const char *getXMLStr(rapidxml::xml_node<> *node, const char *nodeName)
{
    auto *result = node->first_node(nodeName);
    if (result) return result->value();
    return "";
}

unsigned long long getXMLNum(rapidxml::xml_node<> *node, const char *nodeName)
{
    auto *result = node->first_node(nodeName);
    if (result) return strtoull(result->value(), nullptr, 10);
    return -1;
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
    ss << "</" << elements[numElement - 1] << ">\n";
    if (numElement > 1 && indentation) --indentation;
    --numElement;
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

XMLElementPrinter::XMLElementPrinter() { ss << "<?xml version=\"1.0\" ?>\n"; }
XMLElementPrinter::~XMLElementPrinter() { }

// Time

TimeType nanoSecondsBase;
TimeType now;

#ifdef _WIN32
uint64_t clockSpeed;
#endif

TimeType getNanoSeconds()
{
#ifdef _WIN32
    TimeType ticks;
    if (!QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER *>(&ticks))) abort();
    double tmp = (ticks / static_cast<double>(clockSpeed)) * 1000000000.0;
    return static_cast<TimeType>(tmp) - nanoSecondsBase;
#elif defined(__APPLE__) && !defined(USE_GETTIMEOFDAY)
    union
    {
        AbsoluteTime at;
        TimeType tt;
    } tmp;
    tmp.tt = mach_absolute_time();
    Nanoseconds ns = AbsoluteToNanoseconds(tmp.at);
    tmp.tt = UnsignedWideToUInt64(ns) - nanoSecondsBase;
    return tmp.tt;
#elif !defined(USE_GETTIMEOFDAY)
    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
        return static_cast<TimeType>(((tp.tv_sec * 1000000000LL) + tp.tv_nsec)) - nanoSecondsBase;
#endif // WIN32
#ifndef _WIN32
    struct timeval tv;
    if (gettimeofday(&tv, nullptr) == 0)
        return static_cast<TimeType>(((tv.tv_sec * 1000000000LL) + (tv.tv_usec * 1000))) - nanoSecondsBase;
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
    LARGE_INTEGER speed;
    if (!QueryPerformanceFrequency(&speed)) speed.QuadPart = 0;
    if (speed.QuadPart <= 0) abort();
    clockSpeed = speed.QuadPart;
#endif
    nanoSecondsBase = getNanoSeconds();
}

// Signal handler

void signalHandler(int)
{
    static atomic<int> i(0);
    if (i >= 5)
    {
        fprintf(stderr, "As you wish... Shutting down ungracefully.\n");
        w32ConsoleWait("Press enter to exit");
        exit(2);
    }
    fprintf(stderr, "Shutting down. Please be patient (%d) ...\n", 5 - i);
    i++;
    extern atomic<bool> shouldExit;
    shouldExit = true;
    if (waitingForInput) exit(1);
}

static void setupSignalHandler()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}

// Cross Platform

#ifdef _WIN32

extern "C"
char *strsep(char **stringp, const char *delim)
{
    char *start = *stringp;
    char *p;

    p = (start != NULL) ? strpbrk(start, delim) : NULL;

    if (p == NULL)
    {
        *stringp = NULL;
    }
    else
    {
        *p = '\0';
        *stringp = p + 1;
    }

    return start;
}

static void w32SetupConsoleHandler()
{
    char title[256];
    wsprintf(title, _MT(TOOLNAME " v" VERSION));
    SetConsoleTitle((LPCTSTR)title);

    auto consoleHandler = [](DWORD ctrlType) -> BOOL
    {
        switch (ctrlType)
        {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
                w32ExitInstantly = true;
                signalHandler(SIGINT);
                delay(INT_MAX);
                return TRUE;
        }

        return FALSE;
    };

    SetConsoleCtrlHandler((PHANDLER_ROUTINE)+consoleHandler, TRUE);
}

void w32ConsoleWait(const char *msg)
{
    if (w32ExitInstantly) return;
    if (msg) fprintf(stderr, "%s\n", msg);
    waitingForInput = true;
    getchar();
    waitingForInput = false;
}

void clearScreen()
{
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, TEXT(' '), dwConSize, {}, &cCharsWritten);
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, {}, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, {});
}

void delay(unsigned ms)
{
    Sleep(ms);
}

#else

void w32ConsoleWait(const char *)
{
}

void clearScreen()
{
    printf("%s", "\e[1;1H\e[2J");
}

static void w32SetupConsoleHandler()
{
}

void delay(unsigned ms)
{
    usleep(ms * 1000);
}

#endif /* _WIN32 */

// Debugging

bool writeDebugLog = true;

Message warn("warning", std::cerr);
Message err("error", std::cerr);
Message dbg("debug", std::cerr, &writeDebugLog);
Message info("info", std::cout);

void disableDebugLog(const char *msg)
{
    if (!writeDebugLog) return;
    dbglog << msg << "Disabling debug log\n" << dbg.endl();
    writeDebugLog = false;
}

void enableDebugLog()
{
    if (writeDebugLog) return;
    writeDebugLog = true;
    dbglog << "Enabling debug log\n" << dbg.endl();
}

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
    setupSignalHandler();
    w32SetupConsoleHandler();
}
