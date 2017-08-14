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
#include "at_tcp.h"
#include "web.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <fstream>
#include <config4cpp/Configuration.h>

#include "atomic.h"

atomic<bool> w32ExitInstantly(false);
atomic<bool> waitingForInput(false);
atomic<bool> shouldExit(false);

namespace huawei_tool {

void exit_success(bool printSuccess, bool breakBeforePrintingSuccess)
{
    if (printSuccess)
    {
        if (breakBeforePrintingSuccess) printf("\n");
        printf("SUCCESS\n");
    }
    web::logout();
    web::deinit();
    w32ConsoleWait();
    exit(0);
}

void exit_error(bool printError)
{
    static int recursion = 0;
    if (++recursion >= 2)
    {
        errfun << "Recursion detected" << err.endl();
        errfun << "Exiting ungracefully" << err.endl();
        w32ConsoleWait("Press enter to exit");
        exit(2);
    }
    if (!shouldExit)
    {
        if (printError) fprintf(stderr, "\nERROR\n");
        info << "Check debug.log to see what's going on" << info.endl();
    }
    web::logout();
    web::deinit();
    w32ConsoleWait();
    exit(1);
}

#ifdef WORK_IN_PROGRESS
void wip()
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

        if (shouldExit) exit_error(false);
        delay(3000);
    } while (!shouldExit);

    connected:;

    dbglog << "Connected successfully" << dbg.endl();

    printf("Waiting for signal strength data... This may take up to 10 seconds.\n");

    do
    {
        if (!at_tcp::process(100)) break;

    } while (!shouldExit);

    at_tcp::disconnect();
    at_tcp::deinit();

    exit(1);
}
#endif

int main(int argc, char **argv)
{
    config4cpp::Configuration *cfg;

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
    bool showSignalStrengthNoCLS = false;
    const char *relayRequest = nullptr;
    bool relayLoop = false;
    int relayLoopDelay = 0;
    bool rc = false;
    bool printSuccess = true;
    bool printError = true;
    bool breakBeforePrintingSuccess = false;

    cfg = config4cpp::Configuration::create();

    if (/*file_exists()*/true)
    {
        try
        {
            cfg->parse("huawei_band_tool_config.txt");
            copystr(web::routerIP, cfg->lookupString("", "router_ip"));
            copystr(web::routerUser, cfg->lookupString("", "router_user"));
            copystr(web::routerPass, cfg->lookupString("", "router_pass"));
        }
        catch(const config4cpp::ConfigurationException &ex)
        {
            errfun << ex.c_str() << err.endl();
            err << err.endl();
        }
    }

    cfg->destroy();
    cfg = nullptr;

    auto printHelp = [&argv]()
    {
        fprintf(stderr,
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
                " --show-signal-strength-no-cls\n"
                " --relay <url>\n"
                " --relay-loop\n"
                " --relay-loop-delay <milliseconds>\n"
                " --router-ip <ip or host>\n"
                " --router-user <username>\n"
                " --router-pass <password>\n"
                " --win32-exit-instantly\n"
                "\nVersion: " VERSION " \"" CODENAME "\" (Built on: " __DATE__ " " __TIME__ ")\n"
                "Copyright: unknown @ lteforum.at | unknown.lteforum@gmail.com\n\n", argv[0]);
        w32ConsoleWait();
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
                err << "missing argument to '" << arg << "'" << err.endl();
                fprintf(stderr, "\n");
                printHelp();
            }
            return argument;
        };

        if (!strcmp(arg, "--network-mode")) networkMode = getArgument();
        else if (!strcmp(arg, "--network-band")) networkBand = getArgument();
        else if (!strcmp(arg, "--lte-band")) lteBand = getArgument();
        else if (!strcmp(arg, "--lte-band-bitmask-from-string"))
        {
            // Useless cast to shut up the erroneous GCC warning
            auto bandBitmask = (unsigned long long)getLTEBandFromStr(getArgument());
            printf("%llX\n", bandBitmask);
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
        else if (!strcmp(arg, "--show-signal-strength-no-cls")) showSignalStrengthNoCLS = true;
        else if (!strcmp(arg, "--relay")) relayRequest = getArgument();
        else if (!strcmp(arg, "--relay-loop")) relayLoop = true;
        else if (!strcmp(arg, "--relay-loop-delay")) relayLoopDelay = atoi(getArgument());
        else if (!strcmp(arg, "--router-ip")) copystr(web::routerIP, getArgument());
        else if (!strcmp(arg, "--router-user")) copystr(web::routerUser, getArgument());
        else if (!strcmp(arg, "--router-pass")) copystr(web::routerPass, getArgument());
        else if (!strcmp(arg, "--win32-exit-instantly")) w32ExitInstantly = true;
#ifdef WORK_IN_PROGRESS
        else if (!strcmp(arg, "--wip")) wip();
#endif
        else printHelp();
    }

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

        if (shouldExit) exit_error(false);
        delay(3000);
    } while (true);

    login_sucessful:;

    dbglog << "Login successful" << dbg.endl();
    dbglog << dbg.endl();

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
        printError = plmnList; /* In case of wrong number entered */
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
        rc = web::cli::relay(relayRequest, relayLoop, relayLoopDelay);
        printSuccess = false;
        printError = false;
    }
    else if (showSignalStrength || showSignalStrengthNoCLS)
    {
        rc = web::cli::experimental::showSignalStrength(showSignalStrengthNoCLS);
        printSuccess = false;
        printError = false;
    }
    else
    {
        err << "You are doing something wrong" << err.endl();
        web::logout();
        web::deinit();
        fprintf(stderr, "\n");
        printHelp();
    }

    if (rc) exit_success(printSuccess, breakBeforePrintingSuccess);
    else exit_error(printError);

    return 1;
}

} // namespace huawei_tool

int main(int argc, char **argv)
{
    initTools();

    std::fstream dbgLogFile("debug.log", std::ios_base::out | std::ios_base::trunc);
    if (dbgLogFile.good()) dbg.assignStream(dbgLogFile);
    else err << "failed to open debug logfile" << err.endl();

    const char *tool = argv[0];
    if (strstr(tool, "huawei")) return huawei_tool::main(argc, argv);
    err << "unknown tool" << err.endl();
    err << "did you rename the executable?" << err.endl();
    return 1;
}
