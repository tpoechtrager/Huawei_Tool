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

#ifndef __CLI_TOOLS_H__
#define __CLI_TOOLS_H__

#include "compiler.h"
#include "atomic.h"
#include "tools.h"

#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>

namespace cli {

extern atomic<bool> shouldExit;
extern bool hideCursor_;

bool checkExit();

// Debug Messages

struct ConsoleSize
{
    int X;
    int Y;

    bool operator==(const ConsoleSize &in);
    bool operator!=(const ConsoleSize &in);
};

void hideCursor(int hide = 1);
void showCursor();
void clearScreen();
void resetCursor();
ConsoleSize getConsoleSize();
void out(FILE *stream, const char *text, size_t length = 0);
void outf(const char *fmt, ...) PRINTFARGS(1, 2);
void outf(FILE *stream, const char *fmt, ...) PRINTFARGS(2, 3);
void vout(FILE *stream, const char *fmt, va_list args);

int readChar();

// Debug Messages

class DebugMessage
{
private:
    const char *msg;
    FILE *stream;
    const bool *enabled;
public:
    bool isEnabled() const;

    void assignStream(FILE *stream_);
    void linef(const char *fmt, ...) PRINTFARGS(2, 3);

    DebugMessage(const char *msg, FILE *stream, bool *enabled = nullptr);
    ~DebugMessage();
};

extern DebugMessage warn;
extern DebugMessage err;
extern DebugMessage dbg;
extern DebugMessage info;

void disableDebugLog(const char *msg);
void enableDebugLog();

#define errfunf(fmt, ...) \
    ::cli::err.linef("File: %s  Line: %d  Function:  %s: " \
                     fmt, __FILE__, __LINE__, __PRETTY_FUNCTION__, \
                     ## __VA_ARGS__)

#define errfunf_once(fmt, ...) \
do { \
    static atomic<bool> once(false); \
    if (once) break; \
    once = true; \
    errfunf("[only shown once] " fmt, ## __VA_ARGS__); \
} while (false)

#define dbglinef_once(fmt, ...) \
do { \
    ::cli::dbg.linef("[only logged once] " fmt, ## __VA_ARGS__); \
} while (false)

// Debug Messages End

namespace windows {

static constexpr int MAX_COLS = 130;
static constexpr int MIN_COLS = 80;
static constexpr int MAX_ROWS = 40;
static constexpr int MIN_ROWS = 2;

#ifdef _WIN32
extern atomic<bool> exitInstantly;
extern bool exitWait;
extern bool autoResizeConsole;
extern bool autoResizeConsoleOnce;
void wait(const char *msg = nullptr);
void setConsoleTitle(char **argv = nullptr);
#else
MAYBE_UNUSED static bool exitInstantly = false;
MAYBE_UNUSED static bool exitWait = false;
MAYBE_UNUSED static bool autoResizeConsole = false;
MAYBE_UNUSED static bool autoResizeConsoleOnce = false;
MAYBE_UNUSED static inline void wait(const char * = nullptr) {}
MAYBE_UNUSED static void setConsoleTitle(char ** = nullptr) {}
#endif

} // namespace windows

namespace status {

extern bool noClearScreen;
extern bool hideCursor_;

struct Column
{
    std::string header;
    std::vector<std::string> row;
};

void format(const char *fmt, ...) PRINTFARGS(1, 2);
void append(const char *text, size_t length = 0);
void append(const char *text, size_t length, int spacing);
void addChar(const char c, const size_t repeat = 1);
void popChar();
void addColumns(const std::vector<Column> &columns, int spacing);
void clear();
void show();
void exit();

} // namespace status

void init();
void deinit();

} // namespace cli

using namespace cli;

#endif // __CLI_TOOLS__
