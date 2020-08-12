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

#ifndef __HUAWEI_TOOLS_H__
#define __HUAWEI_TOOLS_H__

#include "tools.h"

#include <string>
#include <type_traits>
#include <cstring>
#include <cmath>
#include <map>

// Signal

enum NetType : int
{
    LTE,
    WCDMA,
    GSM,
    INVALID
};

template <typename T>
struct ValueStorage
{
    T val;
    TimeType lastUpdate;
    void operator =(const T in)
    {
        val = in;
        lastUpdate = now;
    }
    T operator*() const { return val; }
    constexpr ValueStorage() : val(T()), lastUpdate(T()) {}
};

template<typename T = int, bool IS_SPEED_VALUE = false>
struct SignalValue
{
    ValueStorage<T> current;
    ValueStorage<T> min;
    ValueStorage<T> max;
    ValueStorage<T> first;
    ValueStorage<T> prev;
    float peakBW;
    double sum;
    size_t count;

    bool isSet() const { return count > 0; }
    float avg() const { return count ? sum / count : T(); }

    float getAvgSpeedInMbits(TimeType timeDiff = TimeType(-1)) const
    {
        const bool overall = timeDiff != TimeType(-1);
        if (!overall && count < 2) return float();
        constexpr ValueStorage<T> dummy;
        const ValueStorage<T> &prevVal = overall ? dummy : prev;
        if (!overall) timeDiff = getTimeDiff(current.lastUpdate, prevVal.lastUpdate);
        if (!timeDiff) return float();
        return (current.val - prevVal.val) / (float)timeDiff * oneSecond / 1024.f / 1024.f * 8.f;
    }

    float getDataTransferredInMB(const bool total = false) const
    {
        if (!count) return T();
        constexpr ValueStorage<T> dummy;
        const ValueStorage<T> &v = total ? dummy : first;
        return byteToMegaByte(current.val - v.val);
    }

    const std::string &getTrafficStr(StrBuf &buf) const
    {
        constexpr const char *specifiers[] =
        {
            "Byte", "KB", "MB", "GB", "TB", "PB", nullptr
        };

        size_t i = 0;
        const char *specifier = specifiers[0];
        double traffic = current.val;

        while (traffic > 1024.0)
        {
            i++;
            if (!specifiers[i]) break;
            specifier = specifiers[i];
            traffic /= 1024.0;
        }

        buf.format("%.2f %s", traffic, specifier);

        return buf;
    }

    TimeType getDuration() const
    {
        return *current * oneSecond;
    }

    TimeType getInterpolatedDuration() const
    {
        TimeType timeDiff = getElapsedTime(current.lastUpdate);
        TimeType duration = getDuration();
        if (timeDiff > MAX_INTERPOLATION_MS) return duration;
        return duration + timeDiff;
    }

    enum GetType : int
    {
        GET_CURRENT,
        GET_MIN,
        GET_WORST,
        GET_MAX,
        GET_BEST,
        GET_FIRST,
        GET_PREVIOUS,
        GET_AVERAGE,
        GET_INVALID
    };

    static const char *const getTypeStrs[];
    static const char *getGetTypeStr(const GetType type);
    static GetType getGetTypeByStr(const char *type);

    template<typename TT = T>
    TT getVal(const int type = GET_CURRENT) const
    {
        if (!isSet()) return TT();

        switch (type)
        {
            case GET_CURRENT:  return *current;
            case GET_MIN:      return *min;
            case GET_WORST:    return *min;
            case GET_MAX:      return *max;
            case GET_BEST:     return *max;
            case GET_FIRST:    return *first;
            case GET_PREVIOUS: return *prev;
            case GET_AVERAGE:
            {
                if (std::is_integral<TT>::value) return round(avg());
                else return avg();
            }
            default:;
        }

        return TT();
    }

#warning min max range ignore
    template<typename TT>
    void update(const TT val_)
    {
        if (val_ == TT(__XML_NUM_ERROR__)) return;
        T val;
        if (std::is_floating_point<TT>::value && std::is_integral<T>::value)
        {
            // Round if ValueStorage is an integral type
            // but a floating point has been passed to us.
            val = round(val_);
        }
        else
        {
            val = val_;
        }
        if (isSet() && current.val == val) return;
        prev = current;
        current = val;
        if (!count) first = val;
        if (val < min.val) min = val;
        if (val > max.val) max = val;
        sum += val;
        count++;
        if (IS_SPEED_VALUE)
        {
            float currentBW = getAvgSpeedInMbits();
            if (currentBW > peakBW) peakBW = currentBW;
        }
    }

    void update(const char *val)
    {
        if (!strcmp(val, __XML_ERROR__)) return;
        if (!strncmp(val, ">=", 2)) val += 2;

        if (strchr(val, '.')) update((T)strtod(val, nullptr));
        else update((T)strtoull(val, nullptr, 10));
    }

    void update(const std::string &val)
    {
        update(val.c_str());
    }

    void reset()
    {
        memset((void*)this, 0, sizeof(*this));
        min = maxVal(T());
        max = minVal(T());
    }

    SignalValue() { reset(); }
};

struct TrafficStats
{
    const char *desc = "";
    SignalValue<TimeType> CD;
    SignalValue<unsigned long long, true> DL;
    SignalValue<unsigned long long, true> UP;

    bool isSet()
    {
        return CD.isSet() && DL.isSet() && UP.isSet();
    }

    void reset()
    {
        CD.reset();
        DL.reset();
        UP.reset();
    }

    TrafficStats(const char *desc) : desc(desc) {}
};

struct ConnStatus
{
    bool down;
    TimeType downDuration;
    TimeType firstDown;
    size_t count;

    void update(const bool down_, const TimeType now)
    {
        count++;
        if (down_)
        {
            if (!firstDown) firstDown = now;
        }
        else if (!down_ && down)
        {
            downDuration += now - firstDown;
            firstDown = 0;
        }
        down = down_;
    }

    bool isDown() { return down; }
    bool wasOrIsDown() { return isDown() || downDuration > 0; }

    float getDownDurationInSeconds(const TimeType now)
    {
        if (!wasOrIsDown()) return 0.f;
        return (downDuration + (firstDown ? now - firstDown : 0)) / 1000.f;
    }

    bool isSet() { return count > 0; }

    void reset()
    {
        count = 0;
        down = false;
        firstDown = 0;
        downDuration = 0;
    }
};

struct Signal
{
    #warning WEB
    SignalValue<> RSCP;
    SignalValue<> ECIO;
    SignalValue<> RSRP;
    SignalValue<> RSRQ;
    SignalValue<> RSSI;
    SignalValue<> SINR;

    SignalValue<> CQI[2];
    SignalValue<> DLMCS[2];
    SignalValue<> UPMCS;

    SignalValue<> TXPWrPPUSCH;
    SignalValue<> TXPWrPPUCCH;
    SignalValue<> TXPWrPSRS;
    SignalValue<> TXPWrPPRACH;

    XMLNumType band;
    XMLNumType cell;
    XMLNumType DLBW;
    XMLNumType UPBW;
    XMLNumType mode;
    XMLNumType networkTypeEx;

    std::string operatorName;
    std::string operatorNameShort;
    XMLNumType PLMN;

    struct AT
    {
        // CERSSI

        struct CERSSI_LTE
        {
            static constexpr int MAX_ANTENNAS = 4;
            bool isSet() const;

            int numAntennas;
            SignalValue<> RSRQ;
            SignalValue<> RSRP[MAX_ANTENNAS];
            SignalValue<> SINR[MAX_ANTENNAS];
            SignalValue<> RI;
            SignalValue<> CQI[2];
        } cerssiLTE;

        struct CERSSI_WCDMA
        {
            bool isSet() const;

            SignalValue<> RSCP;
            SignalValue<> ECIO;
        } cerssiWCDMA;

        struct CERSSI_GSM
        {
            bool isSet() const;

            SignalValue<> RSSI;
        } cerssiGSM;

        // HCSQ

        struct HCSQ_LTE
        {
            bool isSet() const;

            SignalValue<> RSRP;
            SignalValue<> RSRQ;
            SignalValue<> RSSI;
            SignalValue<> SINR;
        } hcsqLTE;

        struct HCSQ_WCDMA
        {
            bool isSet() const;

            SignalValue<> RSSI;
            SignalValue<> RSCP;
            SignalValue<> ECIO;
        } hcsqWCDMA;

        struct HCSQ_GSM
        {
            bool isSet() const;

            SignalValue<> RSSI;
        } hcsqGSM;

        // CQI / RSSI

        struct RSSI
        {
            bool isSet() const;

            SignalValue<> RSSILevel;
        } rssi;

        NetType getNetType() const;
    } at;
};

extern Signal sig;

// avoid name clash with ::signal

namespace x {
static auto &signal = sig;
}

// Network

cxx14_constexpr float getSignalStrengthInPercent(const int RSSILevel)
{
    return 100.f / 31.f * RSSILevel;
}

enum AntennaType : int
{
    ATENNA_TYPE_ERROR = -1,
    ATENNA_TYPE_AUTO = 0,
    ATENNA_TYPE_EXTERN = 1,
    ATENNA_TYPE_INTERN = 2
};

constexpr const char *antennaTypeStrs[] = {"auto", "extern", "intern"};

constexpr const char *getAntennaTypeStr(const AntennaType type)
{
    return antennaTypeStrs[type];
}

cxx14_constexpr AntennaType getAntennaTypeFromStr(const char *type)
{
    for (size_t i = 0; i < sizeof(antennaTypeStrs)/sizeof(*antennaTypeStrs); ++i)
        if (strEqual(antennaTypeStrs[i], type)) return (AntennaType)i;
    return AntennaType::ATENNA_TYPE_ERROR;
}

cxx14_constexpr int getBandFreq(const int band)
{
    switch (band)
    {
        case 1:  return 2100;
        case 2:  return 1900;
        case 3:  return 1800;
        case 4:  return 1700;
        case 5:  return 850;
        case 7:  return 2600;
        case 8:  return 900;
        case 10: return 1700;
        case 11: return 1500;
        case 12:
        case 13:
        case 14:
        case 17: return 1700;
        case 18:
        case 19: return 850;
        case 20: return 800;
        case 21: return 1500;
        case 22: return 3500;
        case 24: return 1600;
        case 25: return 1900;
        case 26: return 850;
        case 27: return 800;
        case 28:
        case 29: return 700;
        case 30: return 2300;
        case 31: return 450;
        case 32: return 1500;
        case 33:
        case 34: return 2100;
        case 35:
        case 36:
        case 37: return 1900;
        case 38: return 2600;
        case 39: return 1900;
        case 40: return 2300;
        case 41: return 2500;
        case 42: return 3500;
        case 43: return 3700;
        case 44: return 700;
        case 45: return 1500;
        case 46: return 5200;
        case 47: return 5900;
        case 48: return 3600;
        case 65: return 2100;
        case 66: return 1700;
        case 67:
        case 68: return 700;
        case 69: return 2600;
        case 70: return 2000;
        case 71: return 600;
        default: return -1;
    }
}

cxx14_constexpr const char *getRatStr(const int rat)
{
    switch (rat)
    {
        case 0:  return "GSM";
        case 2:  return "UMTS";
        case 7:  return "LTE";
        default: return "??";
    }
}

cxx14_constexpr const char *getNetworkTypeExStr(int networkTypeEx)
{
    switch (networkTypeEx)
    {
        case 0:    return "NO SERVICE";
        case 1:    return "GSM";
        case 2:    return "GPRS";
        case 3:    return "EDGE";
        case 20:   return "LTE NR";
        case 41:   return "UMTS";
        case 42:   return "HSDPA";
        case 43:   return "HSUPA";
        case 44:   return "HSPA";
        case 45:   return "HSPA+";
        case 46:   return "DC-HSPA+";
        case 101:  return "LTE";
        case 1011: return "LTE+";
        case 111:  return "NR";
        default:   return "??";
    }
}

cxx14_constexpr const char *getLTEModulationStr(int CQI)
{
    if (CQI >= 1 && CQI <= 6) return "QPSK";
    else if (CQI >= 7 && CQI <= 9) return "16QAM";
    else if (CQI >= 10 && CQI <= 15) return "64QAM";
    else if (CQI == 0) return "OFF";
    return "??";
}

enum LTEBand : unsigned long long
{
    LTE_BAND_ERROR    = 0x0,
    LTE_BAND_800      = 0x80000,
    LTE_BAND_900      = 0x80,
    LTE_BAND_1500     = 0x80000000,
    LTE_BAND_1800     = 0x4,
    LTE_BAND_2100     = 0x1,
    LTE_BAND_2300_TDD = 0x8000000000,
    LTE_BAND_2600     = 0x40,
    LTE_BAND_2600_TDD = 0x2000000000,
    LTE_BAND_ALL      = 0x7FFFFFFFFFFFFFFF,
};

constexpr LTEBand LTEBandTable[] =
{
    LTE_BAND_ERROR,
    LTE_BAND_800,
    LTE_BAND_900,
    LTE_BAND_1500,
    LTE_BAND_1800,
    LTE_BAND_2100,
    LTE_BAND_2300_TDD,
    LTE_BAND_2600,
    LTE_BAND_2600_TDD,
    LTE_BAND_ALL,
};

constexpr const char *LTEBandStrs[] =
{
    "ERROR",
    "800",
    "900",
    "1500",
    "1800",
    "2100",
    "2300|TDD",
    "2600",
    "2600|TDD",
    "ALL",
};

std::string getLTEBandStr(LTEBand lteBand, std::string &str);

cxx14_constexpr LTEBand getLTEBandFromStr2(const char *type)
{
    for (size_t i = 0; i < sizeof(LTEBandStrs)/sizeof(*LTEBandStrs); ++i)
        if (strEqual(LTEBandStrs[i], type)) return LTEBandTable[i];
    return LTEBand::LTE_BAND_ERROR;
}

LTEBand getLTEBandFromStr(const char *band);

#ifdef cxx14
static_assert(getLTEBandFromStr2("800") == LTE_BAND_800, "");
static_assert(getLTEBandFromStr2("900") == LTE_BAND_900, "");
static_assert(getLTEBandFromStr2("1500") == LTE_BAND_1500, "");
static_assert(getLTEBandFromStr2("1800") == LTE_BAND_1800, "");
static_assert(getLTEBandFromStr2("2100") == LTE_BAND_2100, "");
static_assert(getLTEBandFromStr2("2300|TDD") == LTE_BAND_2300_TDD, "");
static_assert(getLTEBandFromStr2("2600") == LTE_BAND_2600, "");
static_assert(getLTEBandFromStr2("2600|TDD") == LTE_BAND_2600_TDD, "");
static_assert(getLTEBandFromStr2("ALL") == LTE_BAND_ALL, "");
static_assert(getLTEBandFromStr2("ERROR") == LTE_BAND_ERROR, "");
#endif

// Taken from:
// https://github.com/HSPDev/Huawei-E5180-API/blob/master/README.md

#ifdef _WIN32
#undef ERROR
#undef ERROR_BUSY
#endif

enum HuaweiErrorCode : int
{
    OK = 0x7FFFFFFF,
    ERROR = 0xBADCAFE,

    ERROR_BUSY = 100004,
    ERROR_CHECK_SIM_CARD_CAN_UNUSEABLE = 101004,
    ERROR_CHECK_SIM_CARD_PIN_LOCK = 101002,
    ERROR_CHECK_SIM_CARD_PUN_LOCK = 101003,
    ERROR_COMPRESS_LOG_FILE_FAILED = 103102,
    ERROR_CRADLE_CODING_FAILED = 118005,
    ERROR_CRADLE_GET_CRURRENT_CONNECTED_USER_IP_FAILED = 118001,
    ERROR_CRADLE_GET_CRURRENT_CONNECTED_USER_MAC_FAILED = 118002,
    ERROR_CRADLE_GET_WAN_INFORMATION_FAILED = 118004,
    ERROR_CRADLE_SET_MAC_FAILED = 118003,
    ERROR_CRADLE_UPDATE_PROFILE_FAILED = 118006,
    ERROR_DEFAULT = -1,
    ERROR_DEVICE_AT_EXECUTE_FAILED = 103001,
    ERROR_DEVICE_COMPRESS_LOG_FILE_FAILED = 103015,
    ERROR_DEVICE_GET_API_VERSION_FAILED = 103006,
    ERROR_DEVICE_GET_AUTORUN_VERSION_FAILED = 103005,
    ERROR_DEVICE_GET_LOG_INFORMATON_LEVEL_FAILED = 103014,
    ERROR_DEVICE_GET_PC_AISSST_INFORMATION_FAILED = 103012,
    ERROR_DEVICE_GET_PRODUCT_INFORMATON_FAILED = 103007,
    ERROR_DEVICE_NOT_SUPPORT_REMOTE_OPERATE = 103010,
    ERROR_DEVICE_PIN_MODIFFY_FAILED = 103003,
    ERROR_DEVICE_PIN_VALIDATE_FAILED = 103002,
    ERROR_DEVICE_PUK_DEAD_LOCK = 103011,
    ERROR_DEVICE_PUK_MODIFFY_FAILED = 103004,
    ERROR_DEVICE_RESTORE_FILE_DECRYPT_FAILED = 103016,
    ERROR_DEVICE_RESTORE_FILE_FAILED = 103018,
    ERROR_DEVICE_RESTORE_FILE_VERSION_MATCH_FAILED = 103017,
    ERROR_DEVICE_SET_LOG_INFORMATON_LEVEL_FAILED = 103013,
    ERROR_DEVICE_SET_TIME_FAILED = 103101,
    ERROR_DEVICE_SIM_CARD_BUSY = 103008,
    ERROR_DEVICE_SIM_LOCK_INPUT_ERROR = 103009,
    ERROR_DHCP_ERROR = 104001,
    ERROR_DIALUP_ADD_PRORILE_ERROR = 107724,
    ERROR_DIALUP_DIALUP_MANAGMENT_PARSE_ERROR = 107722,
    ERROR_DIALUP_GET_AUTO_APN_MATCH_ERROR = 107728,
    ERROR_DIALUP_GET_CONNECT_FILE_ERROR = 107720,
    ERROR_DIALUP_GET_PRORILE_LIST_ERROR = 107727,
    ERROR_DIALUP_MODIFY_PRORILE_ERROR = 107725,
    ERROR_DIALUP_SET_AUTO_APN_MATCH_ERROR = 107729,
    ERROR_DIALUP_SET_CONNECT_FILE_ERROR = 107721,
    ERROR_DIALUP_SET_DEFAULT_PRORILE_ERROR = 107726,
    ERROR_DISABLE_AUTO_PIN_FAILED = 101008,
    ERROR_DISABLE_PIN_FAILED = 101006,
    ERROR_ENABLE_AUTO_PIN_FAILED = 101009,
    ERROR_ENABLE_PIN_FAILED = 101005,
    ERROR_FIRST_SEND = 1,
    ERROR_FORMAT_ERROR = 100005,
    ERROR_GET_CONFIG_FILE_ERROR = 100008,
    ERROR_GET_CONNECT_STATUS_FAILED = 102004,
    ERROR_GET_NET_TYPE_FAILED = 102001,
    ERROR_GET_ROAM_STATUS_FAILED = 102003,
    ERROR_GET_SERVICE_STATUS_FAILED = 102002,
    ERROR_LANGUAGE_GET_FAILED = 109001,
    ERROR_LANGUAGE_SET_FAILED = 109002,
    ERROR_LOGIN_ALREADY_LOGINED = 108003,
    ERROR_LOGIN_MODIFY_PASSWORD_FAILED = 108004,
    ERROR_LOGIN_NO_EXIST_USER = 108001,
    ERROR_LOGIN_PASSWORD_ERROR = 108002,
    ERROR_LOGIN_TOO_MANY_TIMES = 108007,
    ERROR_LOGIN_TOO_MANY_USERS_LOGINED = 108005,
    ERROR_LOGIN_USERNAME_OR_PASSWORD_ERROR = 108006,
    ERROR_NET_CURRENT_NET_MODE_NOT_SUPPORT = 112007,
    ERROR_NET_MEMORY_ALLOC_FAILED = 112009,
    ERROR_NET_NET_CONNECTED_ORDER_NOT_MATCH = 112006,
    ERROR_NET_REGISTER_NET_FAILED = 112005,
    ERROR_NET_SIM_CARD_NOT_READY_STATUS = 112008,
    ERROR_NOT_SUPPORT = 100002,
    ERROR_NO_DEVICE = -2,
    ERROR_NO_RIGHT = 100003,
    ERROR_SYSTEM_BUSY = 100004,
    ERROR_NO_SIM_CARD_OR_INVALID_SIM_CARD = 101001,
    ERROR_ONLINE_UPDATE_ALREADY_BOOTED = 110002,
    ERROR_ONLINE_UPDATE_CANCEL_DOWNLODING = 110007,
    ERROR_ONLINE_UPDATE_CONNECT_ERROR = 110009,
    ERROR_ONLINE_UPDATE_GET_DEVICE_INFORMATION_FAILED = 110003,
    ERROR_ONLINE_UPDATE_GET_LOCAL_GROUP_COMMPONENT_INFORMATION_FAILED = 110004,
    ERROR_ONLINE_UPDATE_INVALID_URL_LIST = 110021,
    ERROR_ONLINE_UPDATE_LOW_BATTERY = 110024,
    ERROR_ONLINE_UPDATE_NEED_RECONNECT_SERVER = 110006,
    ERROR_ONLINE_UPDATE_NOT_BOOT = 110023,
    ERROR_ONLINE_UPDATE_NOT_FIND_FILE_ON_SERVER = 110005,
    ERROR_ONLINE_UPDATE_NOT_SUPPORT_URL_LIST = 110022,
    ERROR_ONLINE_UPDATE_SAME_FILE_LIST = 110008,
    ERROR_ONLINE_UPDATE_SERVER_NOT_ACCESSED = 110001,
    ERROR_PARAMETER_ERROR = 100006,
    ERROR_PB_CALL_SYSTEM_FUCNTION_ERROR = 115003,
    ERROR_PB_LOCAL_TELEPHONE_FULL_ERROR = 115199,
    ERROR_PB_NULL_ARGUMENT_OR_ILLEGAL_ARGUMENT = 115001,
    ERROR_PB_OVERTIME = 115002,
    ERROR_PB_READ_FILE_ERROR = 115005,
    ERROR_PB_WRITE_FILE_ERROR = 115004,
    ERROR_SAFE_ERROR = 106001,
    ERROR_SAVE_CONFIG_FILE_ERROR = 100007,
    ERROR_SD_DIRECTORY_EXIST = 114002,
    ERROR_SD_FILE_EXIST = 114001,
    ERROR_SD_FILE_IS_UPLOADING = 114007,
    ERROR_SD_FILE_NAME_TOO_LONG = 114005,
    ERROR_SD_FILE_OR_DIRECTORY_NOT_EXIST = 114004,
    ERROR_SD_IS_OPERTED_BY_OTHER_USER = 114004,
    ERROR_SD_NO_RIGHT = 114006,
    ERROR_SET_NET_MODE_AND_BAND_FAILED = 112003,
    ERROR_SET_NET_MODE_AND_BAND_WHEN_DAILUP_FAILED = 112001,
    ERROR_SET_NET_SEARCH_MODE_FAILED = 112004,
    ERROR_SET_NET_SEARCH_MODE_WHEN_DAILUP_FAILED = 112002,
    ERROR_SMS_DELETE_SMS_FAILED = 113036,
    ERROR_SMS_LOCAL_SPACE_NOT_ENOUGH = 113053,
    ERROR_SMS_NULL_ARGUMENT_OR_ILLEGAL_ARGUMENT = 113017,
    ERROR_SMS_OVERTIME = 113018,
    ERROR_SMS_QUERY_SMS_INDEX_LIST_ERROR = 113020,
    ERROR_SMS_SAVE_CONFIG_FILE_FAILED = 113047,
    ERROR_SMS_SET_SMS_CENTER_NUMBER_FAILED = 113031,
    ERROR_SMS_TELEPHONE_NUMBER_TOO_LONG = 113054,
    ERROR_STK_CALL_SYSTEM_FUCNTION_ERROR = 116003,
    ERROR_STK_NULL_ARGUMENT_OR_ILLEGAL_ARGUMENT = 116001,
    ERROR_STK_OVERTIME = 116002,
    ERROR_STK_READ_FILE_ERROR = 116005,
    ERROR_STK_WRITE_FILE_ERROR = 116004,
    ERROR_UNKNOWN = 100001,
    ERROR_UNLOCK_PIN_FAILED = 101007,
    ERROR_USSD_AT_SEND_FAILED = 111018,
    ERROR_USSD_CODING_ERROR = 111017,
    ERROR_USSD_EMPTY_COMMAND = 111016,
    ERROR_USSD_ERROR = 111001,
    ERROR_USSD_FUCNTION_RETURN_ERROR = 111012,
    ERROR_USSD_IN_USSD_SESSION = 111013,
    ERROR_USSD_NET_NOT_SUPPORT_USSD = 111022,
    ERROR_USSD_NET_NO_RETURN = 11019,
    ERROR_USSD_NET_OVERTIME = 111020,
    ERROR_USSD_TOO_LONG_CONTENT = 111014,
    ERROR_USSD_XML_SPECIAL_CHARACTER_TRANSFER_FAILED = 111021,
    ERROR_WIFI_PBC_CONNECT_FAILED = 117003,
    ERROR_WIFI_STATION_CONNECT_AP_PASSWORD_ERROR = 117001,
    ERROR_WIFI_STATION_CONNECT_AP_WISPR_PASSWORD_ERROR = 117004,
    ERROR_WIFI_WEB_PASSWORD_OR_DHCP_OVERTIME_ERROR = 117002,
    ERROR_WRONG_TOKEN = 125001,
    ERROR_WRONG_SESSION = 125002,
    ERROR_WRONG_SESSION_TOKEN = 125003
};

std::string huaweiErrStr(HuaweiErrorCode errcode);

#endif // __HUAWEI_TOOLS_H__
