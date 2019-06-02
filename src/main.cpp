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
#include "huawei_tools.h"
#include "cli_tools.h"
#include "at_tcp.h"
#include "web.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <config4cpp/Configuration.h>

namespace cli {

void NORETURN exit_success(bool printSuccess, bool breakBeforePrintingSuccess)
{
    if (printSuccess)
    {
        if (breakBeforePrintingSuccess) outf("\n");
        outf("SUCCESS\n");
    }
    web::logout();
    web::deinit();
    at_tcp::disconnect();
    at_tcp::deinit();
    windows::exitWait = true;
    windows::wait();
    exit(0);
}

void NORETURN exit_error(bool printError)
{
    if (!checkExit())
    {
        if (printError) outf(stderr, "\nERROR\n");
        info.linef("Check debug.log to see what's going on");
    }
    web::logout();
    web::deinit();
    at_tcp::disconnect();
    at_tcp::deinit();
    windows::exitWait = true;
    windows::wait();
    exit(1);
}

int main(int argc, char **argv)
{
    config4cpp::Configuration *cfg;

    bool rc = false;
    bool printSuccess = true;
    bool printError = true;
    bool breakBeforePrintingSuccess = false;
    bool windowsAppendArgumentsToWindowTitle = false;

    const char *networkMode = nullptr;
    const char *networkBand = nullptr;
    const char *lteBand = "80000";
    bool bandShow = false;
    bool plmnList = false;
    const char *plmn = nullptr;
    const char *plmnMode = nullptr;
    const char *plmnRat = nullptr;
    bool selectPlmn = false;
    bool showPlmn = false;
    const char *antennaType = nullptr;
    bool showAntennaType = false;
    bool showSignalStrength = false;
    bool showWlanClients = false;
    bool showTraffic = false;
    const char *relayRequest = nullptr;
    const char *relayPostData = nullptr;
    bool relayLoop = false;
    int relayLoopDelay = 0;
    bool connect = false;
    bool disconnect = false;
    bool showAtTcpSignalStrength = false;

    cfg = config4cpp::Configuration::create();

    try
    {
        cfg->parse("huawei_band_tool_config.txt");

        // Web

        copystr(web::routerIP, cfg->lookupString("", "web_router_ip"));
        copystr(web::routerUser, cfg->lookupString("", "web_router_user"));
        copystr(web::routerPass, cfg->lookupString("", "web_router_pass"));
        web::cli::trafficColumnSpacing = cfg->lookupInt("", "web_cli_traffic_column_spacing");

        // AT TCP

        copystr(at_tcp::routerIP, cfg->lookupString("", "at_tcp_router_ip"));
        at_tcp::routerPort = cfg->lookupInt("", "at_tcp_router_port");

        if (!at_tcp::routerIP[0]) copystr(at_tcp::routerIP, web::routerIP);

        const char *columns = cfg->lookupString("", "at_tcp_cli_signal_strength_columns");
        copystr(at_tcp::cli::columns, columns);
        at_tcp::cli::columnSpacing = cfg->lookupInt("", "at_tcp_cli_column_spacing");

        // Console

        if (cfg->lookupBoolean("", "cli_hide_cursor"))
        {
            cli::hideCursor_ = true;
            cli::hideCursor();
            atexit(+[]{ cli::showCursor(); });
        }

        if (cfg->lookupBoolean("", "cli_hide_cursor_status"))
            cli::status::hideCursor_ = true;

        if (cfg->lookupBoolean("", "cli_append_arguments_to_window_title"))
            windowsAppendArgumentsToWindowTitle = true;

        if (cfg->lookupBoolean("", "cli_auto_resize_console"))
            cli::windows::autoResizeConsole = true;

        if (cfg->lookupBoolean("", "cli_auto_resize_console_once"))
            cli::windows::autoResizeConsoleOnce = true;
    }
    catch (const config4cpp::ConfigurationException &ex)
    {
        errfunf("%s\n", ex.c_str());
        cfg->destroy();
        windows::wait();
        return 1;
    }

    cfg->destroy();
    cfg = nullptr;

    windows::setConsoleTitle(windowsAppendArgumentsToWindowTitle ? argv : nullptr);

    auto printHelp = [&argv]()
    {
        outf(stderr,
             "%s \n"
             " --network-mode <mode>\n"
             " --network-band <band>\n"
             " --lte-band +<bandstr> or <bitmask>\n"
             " --lte-band-bitmask-from-string <band(s)>\n"
             " --show-band\n"
             " --list-plmn\n"
             " --plmn <plmn>\n"
             " --plmn-mode <mode>\n"
             " --plmn-rat <rat>\n"
             " --select-plmn\n"
             " --show-plmn\n"
             " --set-antenna-type <type>\n"
             " --show-antenna-type\n"
             " --show-signal-strength\n"
             " --show-wlan-clients\n"
             " --show-traffic\n"
#ifdef WORK_IN_PROGRESS
             " --show-at-tcp-signal-strength\n"
             " --at-tcp-signal-strength-columns <columns>\n"
#endif
             " --no-clear-screen\n"
             " --relay <url>\n"
             " --relay-post-data <data>\n"
             " --relay-loop\n"
             " --relay-loop-delay <milliseconds>\n"
             " --connect\n"
             " --disconnect\n"
             " --windows-exit-instantly\n"
             "\nVersion: " VERSION " \"" CODENAME "\" (Built on: " __DATE__ " " __TIME__ ")\n"
             "GitHub: https://github.com/0xAA/Huawei_Tool\n\n"
             "Copyright: unknown @ lteforum.at | unknown.lteforum@gmail.com\n\n", argv[0]);
        windows::wait();
        exit(1);
    };

    if (argc == 1) printHelp();

    for (int i = 1; i < argc; ++i)
    {
        const char *arg = argv[i];

        auto getArgument = [&]()
        {
            char *argument = argv[++i];
            if (!argument)
            {
                err.linef("Missing argument to '%s'", arg);
                outf(stderr, "\n");
                printHelp();
            }
            return argument;
        };

        if (!strcmp(arg, "--network-mode")) networkMode = getArgument();
        else if (!strcmp(arg, "--network-band")) networkBand = getArgument();
        else if (!strcmp(arg, "--lte-band")) lteBand = getArgument();
        else if (!strcmp(arg, "--lte-band-bitmask-from-string"))
        {
            // Useless cast to silence the erroneous GCC warning
            auto bandBitmask = (unsigned long long)getLTEBandFromStr(getArgument());
            outf("%llX\n", bandBitmask);
            return 0;
        }
        else if (!strcmp(arg, "--show-band")) bandShow = true;
        else if (!strcmp(arg, "--list-plmn")) plmnList = true;
        else if (!strcmp(arg, "--plmn")) plmn = getArgument();
        else if (!strcmp(arg, "--plmn-mode")) plmnMode = getArgument();
        else if (!strcmp(arg, "--plmn-rat")) plmnRat = getArgument();
        else if (!strcmp(arg, "--select-plmn")) selectPlmn = true;
        else if (!strcmp(arg, "--show-plmn")) showPlmn = true;
        else if (!strcmp(arg, "--set-antenna-type")) antennaType = getArgument();
        else if (!strcmp(arg, "--show-antenna-type")) showAntennaType = true;
        else if (!strcmp(arg, "--show-signal-strength")) showSignalStrength = true;
        else if (!strcmp(arg, "--show-wlan-clients")) showWlanClients = true;
        else if (!strcmp(arg, "--show-traffic")) showTraffic = true;
        else if (!strcmp(arg, "--no-clear-screen")) cli::status::noClearScreen = true;
        else if (!strcmp(arg, "--relay")) relayRequest = getArgument();
        else if (!strcmp(arg, "--relay-post-data")) relayPostData = getArgument();
        else if (!strcmp(arg, "--relay-loop")) relayLoop = true;
        else if (!strcmp(arg, "--relay-loop-delay")) relayLoopDelay = atoi(getArgument());
        else if (!strcmp(arg, "--connect")) connect = true;
        else if (!strcmp(arg, "--disconnect")) disconnect = true;
        else if (!strcmp(arg, "--windows-exit-instantly")) windows::exitInstantly = true;
#ifdef WORK_IN_PROGRESS
        else if (!strcmp(arg, "--show-at-tcp-signal-strength")) showAtTcpSignalStrength = true;
        else if (!strcmp(arg, "--at-tcp-signal-strength-columns")) copystr(at_tcp::cli::columns, getArgument());
#endif
        else printHelp();
    }

    if (showAtTcpSignalStrength)
    {
        at_tcp::init();

        do
        {
            switch (at_tcp::connect())
            {
                using namespace at_tcp;
                case at_tcp::AT_TCP_Error::OK: goto connected;
                case at_tcp::AT_TCP_Error::COULD_NOT_RESOLVE_HOSTNAME: exit_error(false);
                case at_tcp::AT_TCP_Error::COULD_NOT_CONNECT: break;
            }
            if (checkExit()) exit_error(false);
            delay(3000);
        } while (!checkExit());

        connected:;
        #warning reconnect
        dbg.linef("Connected successfully");
    }
    else
    {
        if (plmn && (!plmnRat || !plmnMode)) printHelp();
        if (networkMode && (!networkBand || !lteBand)) printHelp();
        if (!web::routerIP[0]) printHelp();

        web::init();

        do
        {
            switch (web::login())
            {
                case HuaweiErrorCode::OK: goto login_sucessful;
                case HuaweiErrorCode::ERROR_LOGIN_TOO_MANY_USERS_LOGINED:
                case HuaweiErrorCode::ERROR_LOGIN_ALREADY_LOGINED: break;
                default: exit_error(false);
            }

            if (checkExit()) exit_success(false, false);
            delay(3000);
        } while (true);

        login_sucessful:;

        dbg.linef("Login successful");
    }

    if (antennaType)
    {
        rc = web::cli::setAntennaType(antennaType);
        printSuccess = true;
        printError = false;
    }
    else if (showAntennaType)
    {
        rc = web::cli::showAntennaType();
        printSuccess = false;
        printError = false;
    }
    else if (showPlmn)
    {
        rc = web::cli::showPlmn();
        printSuccess = false;
        printError = false;
    }
    else if (plmnList || selectPlmn)
    {
        rc = web::cli::selectPlmn(selectPlmn);
        printSuccess = selectPlmn;
        printError = true;
    }
    else if (plmn)
    {
        rc = web::cli::setPlmn(plmn, plmnMode, plmnRat);
        printSuccess = true;
        printError = false;
    }
    else if (networkMode)
    {
        rc = web::cli::setNetworkMode(networkMode, networkBand, lteBand);
        printSuccess = true;
        printError = false;
        breakBeforePrintingSuccess = true;
    }
    else if (bandShow)
    {
        rc = web::cli::showNetworkMode();
        printSuccess = false;
        printError = false;
    }
    else if (relayRequest)
    {
        rc = web::cli::relay(relayRequest, relayPostData, relayLoop, relayLoopDelay);
        printSuccess = false;
        printError = false;
    }
    else if (showSignalStrength)
    {
        rc = web::cli::experimental::showSignalStrength();
        printSuccess = false;
        printError = false;
    }
    else if (showWlanClients)
    {
        rc = web::cli::showWlanClients();
        printSuccess = false;
        printError = false;
    }
    else if (showTraffic)
    {
        rc = web::cli::showTraffic();
        printSuccess = false;
        printError = false;
    }
    else if (connect || disconnect)
    {
        rc = connect ? web::cli::connect() : web::cli::disconnect();
        printSuccess = true;
        printError = true;
    }
    else if (showAtTcpSignalStrength)
    {
        rc = at_tcp::cli::showSignalStrength();
        printSuccess = false;
        printError = false;
    }
    else
    {
        err.linef("You are doing something wrong");
        at_tcp::disconnect();
        at_tcp::deinit();
        web::logout();
        web::deinit();
        outf(stderr, "\n");
        printHelp();
    }

    if (rc) exit_success(printSuccess, breakBeforePrintingSuccess);
    else exit_error(printError);
}

} // namespace cli

int main(int argc, char **argv)
{
    initTools();
    cli::init();
    atexit(cli::deinit);

    FILE *dbgLogFile = fopen("debug.log", "w");
    if (dbgLogFile) dbg.assignStream(dbgLogFile);
    else err.linef("Failed to open debug logfile");

    return cli::main(argc, argv);
}
