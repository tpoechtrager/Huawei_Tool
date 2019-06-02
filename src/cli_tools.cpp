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

#include "cli_tools.h"
#include "atomic.h"

#include <cstdarg>
#include <csignal>
#include <algorithm>

#ifdef _WIN32
#include <cmath>
#include <windows.h>
#include <shlwapi.h>
#include <conio.h>
#include "version.h"
#undef IN
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace cli {

namespace {

atomic<unsigned> textPrintCount(0);
atomic<unsigned> textPrintCountError(0);
atomic<bool> tty(false);

#ifdef _WIN32
HANDLE console;
HWND consoleWindow;
#endif

} // anonymous namespace

#ifdef _WIN32
namespace windows {
static int adjustConsoleSize(int cols, int rows = -1);
static int adjustConsoleSize(const char *text);
}
#endif // _WIN32

atomic<bool> shouldExit(false);
bool hideCursor_ = false;

bool checkExit()
{
    if (shouldExit)
    {
        if (windows::exitWait) return true;
        static bool once = false;
        if (once) return true;
        once = true;
        outf(stderr, "Shutting down. Please be patient...\n");
        return true;
    }
    return false;
}

bool ConsoleSize::operator==(const ConsoleSize &in)
{
    return X == in.X && Y == in.Y;
}

bool ConsoleSize::operator!=(const ConsoleSize &in)
{
    return X != in.X || Y != in.Y;
}

void hideCursor(int hide)
{
    static bool cursorPreviouslyHidden = false;
    if (hide == -1) hide = cursorPreviouslyHidden ? 1 : 0;
    cursorPreviouslyHidden = !!hide;

    if (tty)
    {
        printf("%s", hide ? "\e[?25l" : "\e[?25h");
        return;
    }

#ifdef _WIN32
    CONSOLE_CURSOR_INFO info;
    if (!GetConsoleCursorInfo(console, &info)) return;
    info.bVisible = hide ? FALSE : TRUE;
    if (!SetConsoleCursorInfo(console, &info)) return;
#endif
}

void showCursor()
{
    hideCursor(0);
}

void clearScreen()
{
    if (tty)
    {
        printf("%s", "\e[1;1H\e[2J");
        return;
    }

#ifdef _WIN32
    DWORD charsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD conSize;
    if (!GetConsoleScreenBufferInfo(console, &csbi)) return;
    conSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(console, TEXT(' '), conSize, {}, &charsWritten);
    FillConsoleOutputAttribute(console, csbi.wAttributes, conSize, {}, &charsWritten);
    if (!SetConsoleCursorPosition(console, {})) return;
#endif
}

void resetCursor()
{
    if (tty)
    {
        printf("%s", "\033[0;0H");
        return;
    }

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(console, &csbi)) return;
    if (!SetConsoleCursorPosition(console, {})) return;
#endif
}

ConsoleSize getConsoleSize()
{
#ifdef _WIN32
    if (tty) return {};
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(console, &csbi)) return {};
    return {csbi.dwSize.X, csbi.dwSize.Y};
#else
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) return {};
    return {size.ws_row, size.ws_col};
#endif
}

void out(FILE *stream, const char *text, size_t length)
{
    if (!length) length = strlen(text);
    fwrite(text, length, sizeof(char), stream);
    fflush(stream);
    if (stream != stdout && stream != stderr) return;
    if (stream == stderr) textPrintCountError++;
    textPrintCount++;
}

void outf(const char *fmt, ...)
{
    char text[16384];
    size_t length;
    va_list args;
    va_start(args, fmt);
    bool ok = vformat(text, sizeof(text), fmt, args, &length);
    va_end(args);
    if (!ok) return;
    out(stdout, text, length);
#ifdef _WIN32
    windows::adjustConsoleSize(text);
#endif
}

void outf(FILE *stream, const char *fmt, ...)
{
    char text[16384];
    size_t length;
    va_list args;
    va_start(args, fmt);
    bool ok = vformat(text, sizeof(text), fmt, args, &length);
    va_end(args);
    if (!ok) return;
    out(stream, text, length);
#ifdef _WIN32
    // Error messages should not cause a resize
    if (stream != stdout) return;
    windows::adjustConsoleSize(text);
#endif
}

void vout(FILE *stream, const char *fmt, va_list args)
{
    char text[16384];
    size_t length;
    if (!vformat(text, sizeof(text), fmt, args, &length)) return;
    out(stream, text, length);
    if (stream != stdout && stream != stderr) return;
#ifdef _WIN32
    windows::adjustConsoleSize(text);
#endif
}

#ifndef _WIN32
int kbhit()
{
    termios term;
    int bytesWaiting;
    if (tcgetattr(0, &term)) abort();
    termios term2 = term;
    term2.c_lflag &= ~ICANON;
    if (tcsetattr(0, TCSANOW, &term2)) abort();
    if (ioctl(0, FIONREAD, &bytesWaiting)) abort();
    if (tcsetattr(0, TCSANOW, &term)) abort();
    return bytesWaiting > 0 ? 1 : 0;
}
#endif // ! _WIN32

int readChar()
{
    do
    {
        if (checkExit()) return EOF;
        if (kbhit()) return getchar();
        delay(5);
    } while (true);
}

// Debug Messages

static bool writeDebugLog = true;

DebugMessage warn("warning", stderr);
DebugMessage err("error", stderr);
DebugMessage dbg("debug", stderr, &writeDebugLog);
DebugMessage info("info", stdout);

void DebugMessage::assignStream(FILE *stream_)
{
    stream = stream_;
    msg = nullptr;
}

void DebugMessage::linef(const char *fmt, ...)
{
    if (!isEnabled()) return;
    if (fmt)
    {
        if (msg) outf(stream, "[%llu] %s: ", getMilliSeconds(), msg);
        else outf(stream, "[%llu]: ", getMilliSeconds());
        va_list args;
        va_start(args, fmt);
        vout(stream, fmt, args);
        va_end(args);
        out(stream, "\n", 1);
    }
    else out(stream, "\n\n", 2);
}

bool DebugMessage::isEnabled() const
{
    return !enabled || *enabled;
}

DebugMessage::DebugMessage(const char *msg, FILE *stream, bool *enabled)
                         : msg(msg), stream(stream), enabled(enabled)
{
}

DebugMessage::~DebugMessage()
{
    if (stream != stdout && stream != stderr) fclose(stream);
}

void disableDebugLog(const char *msg)
{
    if (!writeDebugLog) return;
    dbg.linef("%sDisabling debug log", msg);
    writeDebugLog = false;
}

void enableDebugLog()
{
    if (writeDebugLog) return;
    writeDebugLog = true;
    dbg.linef("Enabling debug log");
}

// Debug Messages End

namespace windows {

#ifdef _WIN32

atomic<bool> exitInstantly(false);
bool exitWait = false;
bool autoResizeConsole = false;
bool autoResizeConsoleOnce = false;

void wait(const char *msg)
{
    if (exitInstantly || checkExit() || tty) return;
    if (msg) fprintf(stderr, "%s\n", msg);
    readChar();
}

void setConsoleTitle(char **argv)
{
    std::string title = TOOLNAME " v" VERSION;
    if (argv && argv[1])
    {
        title += " [";
        size_t i = 1;
        do
        {
            title += argv[i];
            if (!argv[++i]) break;
            title += " ";
        } while (true);
        title += "]";
    }
    char tmp[256] = "";
    wnsprintf(tmp, sizeof(tmp), _MT(title.c_str()));
    tmp[sizeof(tmp)-1] = '\0';
    SetConsoleTitle((LPCTSTR)tmp);
}

static int adjustConsoleSize(int cols, int rows)
{
    if (!autoResizeConsole || tty) return false;

    if (consoleWindow == GetForegroundWindow() && GetKeyState(VK_LBUTTON) < 0)
    {
        // Microsoft seems to resize the console while the window is being moved.
        // Why on earth are they doing this...? In order to prevent total
        // brokenness we do not resize the console until the user stopped moving
        // the window.
        return -1;
    }

    if (cols < MIN_COLS) cols = MIN_COLS;
    else if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows < MIN_ROWS) rows = MIN_ROWS;
    else if (rows > MAX_ROWS) rows = MAX_ROWS;

    static int lastCols = 0;
    static int lastRows = 0;

    bool outputLayoutChanged;
    bool resized = false;

    outputLayoutChanged = cols != lastCols || rows != lastRows;
    if (autoResizeConsoleOnce && !outputLayoutChanged) return false;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD *coord = &csbi.dwSize;
    SMALL_RECT *rect = &csbi.srWindow;

    if (!GetConsoleScreenBufferInfo(console, &csbi)) return false;

    cols++;
    rows++;

    int suggestedCols = cols;
    int suggestedRows = rows;

    if (outputLayoutChanged)
    {
        suggestedRows = roundf(cols / 4.f) - 3;
        if (suggestedRows < MIN_ROWS) suggestedRows = MIN_ROWS;
    }

    if (coord->X < suggestedCols || coord->Y < suggestedRows)
    {
        if (coord->X < suggestedCols) coord->X = suggestedCols;
        if (coord->Y < suggestedRows) coord->Y = suggestedRows;
        resized = true;
    }

    if (!SetConsoleScreenBufferSize(console, *coord)) return 0;

    int currentCols = rect->Right - rect->Left + 1;
    int currentRows = rect->Bottom - rect->Top + 1;

    if (currentCols < suggestedCols || currentRows < suggestedRows)
    {
		if (currentCols < suggestedCols) rect->Right += suggestedCols - currentCols;
		if (currentRows < suggestedRows) rect->Bottom += suggestedRows - currentRows;
        resized = true;

        if (!SetConsoleWindowInfo(console, TRUE, rect)) return 0;
    }

    lastCols = cols - 1;
    lastRows = rows - 1;

    return resized ? 1 : 0;
}

static int adjustConsoleSize(const char *text)
{
    if (!autoResizeConsole) return false;

    size_t longestLineLength = getLongestLineLength(text);

    if (longestLineLength > MAX_COLS)
    {
        // Do not even attempt to resize if the longest
        // line is longer than MAX_COLS
        return 0;
    }

    return adjustConsoleSize(longestLineLength);
}

#endif // _WIN32

} // namespace windows

namespace status {

bool noClearScreen = false;
bool hideCursor_ = false;

namespace {
bool cursorHidden_ = false;
std::string lastOutput;
char output[8192];
size_t outputPos = 0;
} // anonymous namespace

void append(const char *text, size_t length)
{
    if (!length) length = strlen(text);
    char *curBufferPos = output + outputPos;
    size_t bufferSizeLeft = sizeof(output) - outputPos;
    if (!bufferSizeLeft) return;
    size_t toAppend = std::min(length, bufferSizeLeft - 1);
    memcpy(curBufferPos, text, toAppend);
    curBufferPos[toAppend] = '\0';
    outputPos += toAppend;
}

void append(const char *text, size_t length, int spacing)
{
    if (length > (size_t)spacing) length = spacing;
    append(text, length);
    addChar(' ', spacing - length);
}

void addChar(const char c, const size_t repeat)
{  
    char *curBufferPos = output + outputPos;
    size_t bufferSizeLeft = sizeof(output) - outputPos;
    if (!bufferSizeLeft) return;
    size_t toAppend = std::min(repeat, bufferSizeLeft - 1);
    memset(curBufferPos, c, toAppend);
    curBufferPos[toAppend] = '\0';
    outputPos += toAppend;
}

void popChar()
{
    if (!outputPos) return;
    outputPos--;
}

void format(const char *fmt, ...)
{
    int length;
    char *curBufferPos = output + outputPos;
    size_t bufferSizeLeft = sizeof(output) - outputPos;
    va_list args;
    va_start(args, fmt);
    length = vsnprintf(curBufferPos, bufferSizeLeft, fmt, args);
    va_end(args);
    if (length <= 0 || (size_t)length >= bufferSizeLeft) return;
    outputPos += length;
}

void addColumns(const std::vector<Column> &columns, int spacing)
{
    if (columns.empty() || columns[0].row.empty()) return;
    if (spacing < 0) spacing = 0;
    else if (spacing > 128) spacing = 128;

    const size_t lineLength = columns.size() * spacing;

    addChar('-', lineLength);
    addChar('\n');

    for (auto &column : columns)
    {
        const std::string &header = column.header;
        append(header.data(), header.length(), spacing);
    }

    addChar('\n');
    addChar('-', lineLength);
    addChar('\n');

    const size_t numRows = columns[0].row.size();

    for (size_t i = 0; i < numRows; i++)
    {
        if (columns[0].row[i].empty())
        {
            addChar('-', lineLength);
            addChar('\n');
            continue;
        }

        for (size_t j = 0; j < columns.size(); j++)
        {
            if (columns[j].row.size() != numRows)
            {
                errfunf_once("Column row size mismatch");
                clear();
                return;
            }

            const std::string &row = columns[j].row[i];
            append(row.data(), row.length(), spacing);
        }

        addChar('\n');
    }
}

void clear()
{
    outputPos = 0;
    output[0] = '\0';
}

void show()
{
    if (checkExit()) return;
    if (outputPos == 0) return;

    // We do a lot of trickery in here to
    // avoid the need of clearScreen() on Windows

    // Add trailing new line
    if (outputPos > 0 && output[outputPos - 1] != '\n') addChar('\n');

    const bool useClearScreen = !noClearScreen;

    static unsigned previousTextPrintCount = 0;
    static unsigned previousTextPrintCountError = 0;
    static ConsoleSize previousConsoleSize = {};
    static std::list<size_t> previousLineLengths;

    ConsoleSize consoleSize = getConsoleSize();
    std::list<size_t> lineLengths = getLineLengths(output);
    bool consoleResized = false;

#ifdef _WIN32
    if (!tty)
    {
        // Fill the rows with spaces so
        // we don't need to call clearScreen()

        size_t longestLineLength = getLongestLineLength(lineLengths);
        bool spaceFill = false;

        for (size_t &lineLength : lineLengths)
        {
            if (lineLength != longestLineLength)
            {
                lineLength = longestLineLength;
                spaceFill = true;
            }
        }

        if (spaceFill)
        {
            std::vector<std::string> rows;
            splitLines(rows, output, true);
            clear();

            for (auto &row : rows)
            {
                append(row.data(), row.length());
                addChar(' ', longestLineLength - row.length());
                addChar('\n');
            }
        }

        switch (windows::adjustConsoleSize(longestLineLength, lineLengths.size()))
        {
            case 1: consoleResized = true; break;
        }
    }
#endif // _WIN32

    // If the current output has the same dimensions as the previous
    // output, the console has not been resized and nothing has
    // been printed in the meantime then we just reset the console cursor
    // instead of clearing the wohle window.

    bool forceRePrint = false;
    if (consoleSize != previousConsoleSize) consoleResized = true;

    if (consoleResized) forceRePrint = true;
    else if (textPrintCount != previousTextPrintCount) forceRePrint = true;
    else if (lineLengths != previousLineLengths) forceRePrint = true;

    bool outputChanged = lastOutput.compare(0, outputPos, output);

    if (textPrintCountError != previousTextPrintCountError && !outputChanged)
    {
        // Do not clear error messages unless the output has changed.

        clear();
        return;
    }

    if (/*status::*/hideCursor_ && !/*status::*/cursorHidden_)
    {
        /*status::*/cursorHidden_ = true;
        hideCursor(1);
    }

    if (outputChanged || forceRePrint)
    {
        if (useClearScreen)
        {
            if (forceRePrint) clearScreen();
            else resetCursor();
        }

        // printf() is very slow on Windows.
        // We use out() [fwrite()] instead.
        out(stdout, output, outputPos);

        lastOutput.assign(output, outputPos);
    }

    previousTextPrintCount = textPrintCount;
    previousTextPrintCountError = textPrintCountError;
    previousConsoleSize = consoleSize;
    previousLineLengths = std::move(lineLengths);

#ifdef _WIN32
    if (consoleResized)
    {
        // Work around a Windows bug.
        // The cursor shows up again when the
        // console is resized or maximized.

        hideCursor(-1);
    }
#endif

    clear();
}

void exit()
{
    if (/*status::*/cursorHidden_)
    {
        /*status::*/cursorHidden_ = false;
        showCursor();
    }
}

} // namespace status

// Signal Handler

namespace {

void signalHandler(int)
{
    static atomic<int> i(0);
    if (i >= 5) _exit(2);
    i++;
    shouldExit = true;
    windows::exitInstantly = true;
}

#ifdef _WIN32
static BOOL WINAPI windowsConsoleSignalHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            signalHandler(SIGTERM);
            // This ******** is needed on Windows 10.
            while (true) delay(1000);
            return TRUE;
    }

    return TRUE;
}
#endif

static void setupSignalHandler()
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#ifdef _WIN32
    SetConsoleCtrlHandler(windowsConsoleSignalHandler, TRUE);
#endif
}

} // anonymous namespace

void init()
{
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO dummy;
    console = GetStdHandle(STD_OUTPUT_HANDLE);
    consoleWindow = GetConsoleWindow();
    if (console == INVALID_HANDLE_VALUE) abort();
    HMODULE ntdll = GetModuleHandle("ntdll.dll");
    if (ntdll && GetProcAddress(ntdll, "wine_get_version")) tty = true;
    else if (!GetConsoleScreenBufferInfo(console, &dummy)) tty = true;
#else
    if (isatty(STDOUT_FILENO)) tty = true;
#endif // _WIN32

    setupSignalHandler();
}

void deinit()
{
    textPrintCount = 0;
    textPrintCountError = 0;
    status::exit();
}

} // namespace cli

#warning center option windows
#warning win64 crash
#warning mac test
#warning fixed console size vars
