#pragma once

#include <stdint.h>

namespace UBX
{
    static constexpr uint8_t SYNC_CHAR1 = 0xB5;
    static constexpr uint8_t SYNC_CHAR2 = 0x62;
    static constexpr size_t MAX_PACKET_LEN = 256;

    enum class MessageClass : uint8_t 
    {
        MSG_CLASS_NAV = 0x01,
        MSG_CLASS_RXM = 0x02,
        MSG_CLASS_INF = 0x04,
        MSG_CLASS_ACK = 0x05,
        MSG_CLASS_CFG = 0x06,
        MSG_CLASS_MON = 0x0A,
        MSG_CLASS_AID = 0x0B,
        MSG_CLASS_TIM = 0x0D,
        MSG_CLASS_MGA = 0x13,
        MSG_CLASS_LOG = 0x21
    };

    struct __attribute__((__packed__)) Header
    {
        uint8_t       syncChars[2];
        MessageClass  msgClass;
        uint8_t       msgId;
        uint16_t      msgLength; // Excludes header and CRC bytes
	};

    /****************************** ACK *************************************/

    namespace ACK
    {
        enum Id : uint8_t
        {
            ID_NACK = 0x00,
            ID_ACK  = 0x01
        };

        struct __attribute__((__packed__)) MSG_ACK
        {
            MessageClass  clsID;
            uint8_t  msgID;
        };

        struct __attribute__((__packed__)) MSG_NACK
        {
            MessageClass  clsID;
            uint8_t  msgID;
        };

    } // namespace ACK

    /****************************** MON *************************************/

    namespace MON
    {
        enum Id : uint8_t
        {
            ID_VER = 0x04
        };

    } // namespace MON

    /****************************** CFG *************************************/

    namespace CFG
    {
        enum Id : uint8_t
        {
            ID_PRT       = 0x00,
            ID_ANT       = 0x13,
            ID_CFG       = 0x09,
            ID_DAT       = 0x06,
            ID_GNSS      = 0x3E,
            ID_INF       = 0x02,
            ID_ITFM      = 0x39,
            ID_LOGFILTER = 0x47,
            ID_MSG       = 0x01,
            ID_NAV5      = 0x24,
            ID_NAVX5     = 0x23,
            ID_NMEA      = 0x17,
            ID_NVS       = 0x22,
            ID_PM2       = 0x3B,
            ID_RATE      = 0x08,
            ID_RINV      = 0x34,
            ID_RST       = 0x04,
            ID_RXM       = 0x11,
            ID_SBAS      = 0x16,
            ID_TP5       = 0x31,
            ID_USB       = 0x1B,
            ID_ODO       = 0x1E
        };

        namespace PRT
        {
            enum PortID : uint8_t
            {
                PORTID_DDC = 0,
                PORTID_UART = 1,
                PORTID_USB = 3,
                PORTID_SPI = 4,
            };

            enum Mode : uint32_t
            {
                MODE_CHARLEN_5BIT = 0b00 << 6,
                MODE_CHARLEN_6BIT = 0b01 << 6,
                MODE_CHARLEN_7BIT = 0b10 << 6,
                MODE_CHARLEN_8BIT = 0b11 << 6,

                MODE_PARITY_EVEN = 0b000 << 9,
                MODE_PARITY_ODD  = 0b001 << 9,
                MODE_PARITY_NO   = 0b100 << 9,
                MODE_PARITY_8BIT = 0b010 << 9,

                MODE_STOP_BITS_1   = 0b00 << 12,
                MODE_STOP_BITS_1_5 = 0b01 << 12,
                MODE_STOP_BITS_2   = 0b10 << 12,
                MODE_STOP_BITS_0_5 = 0b11 << 12
            };

            enum ProtoMask : uint32_t
            {
                PROTOMASK_UBX   = 1 << 0,
                PROTOMASK_NMEA  = 1 << 1,
                PROTOMASK_RTCM  = 1 << 2,
                PROTOMASK_RTCM3 = 1 << 5
            };

            struct __attribute__((__packed__)) MSG_UART
            {
                uint8_t  portID;
                uint8_t  reserved1;
                uint16_t txReady;
                uint32_t mode;
                uint32_t baudRate;
                uint16_t inProtoMask;
                uint16_t outProtoMask;
                uint16_t flags;
                uint8_t  reserved2[2];
            };
        } // namespace PRT

        namespace MSG
        {
            struct __attribute__((__packed__)) MSG_MSG
            {
                MessageClass  msgClass;
                uint8_t  msgID;
                uint8_t  rate;
            };
        } // namespace MSG

        namespace GNSS
        {
            enum GNSSID : uint8_t
            {
                GNSSID_GPS     = 0,
                GNSSID_SBAS    = 1,
                GNSSID_GALILEO = 2,
                GNSSID_BEIDOU  = 3,
                GNSSID_IMES    = 4,
                GNSSID_QZSS    = 5,
                GNSSID_GLONASS = 6
            };

            enum FLAGS : uint32_t
            {
                FLAGS_ENABLE     = 1 << 0,

                FLAGS_GPS_L1CA    = 0x01 << 16,
                FLAGS_GPS_L2C     = 0x10 << 16,
                FLAGS_GPS_L5      = 0x20 << 16,
                FLAGS_SBAS_L1CA   = 0x01 << 16,
                FLAGS_GALILEO_E1  = 0x01 << 16,
                FLAGS_GALILEO_E5A = 0x10 << 16,
                FLAGS_GALILEO_E5B = 0x20 << 16,
                FLAGS_BEIDOU_B1I  = 0x01 << 16,
                FLAGS_BEIDOU_B2I  = 0x10 << 16,
                FLAGS_BEIDOU_B2A  = 0x80 << 16,
                FLAGS_IMES_L1     = 0x01 << 16,
                FLAGS_QZSS_L1CA   = 0x01 << 16,
                FLAGS_QZSS_L1S    = 0x04 << 16,
                FLAGS_QZSS_L2C    = 0x10 << 16,
                FLAGS_QZSS_L5     = 0x20 << 16,
                FLAGS_GLONASS_L1  = 0x01 << 16,
                FLAGS_GLONASS_L2  = 0x10 << 16
            };

            struct __attribute__((__packed__)) MSG_GNSS
            {
                uint8_t version;
                uint8_t numTrkChHw;
                uint8_t numTrkChUse;
                uint8_t numConfigBlocks;
                struct {
                    GNSSID   gnssId;
                    uint8_t  resTrkCh;
                    uint8_t  maxTrkCh;
                    uint8_t  reserved1;
                    uint32_t flags;
                } data[7];
            };

            static constexpr size_t sizefasdasd = sizeof(MSG_GNSS);
        } // namespace GNSS

        namespace ODO
        {
            struct __attribute__((__packed__)) MSG_ODO
            {
                uint8_t version;
                uint8_t reserved1[3];
                uint8_t flags;
                uint8_t odoCfg;
                uint8_t reserved2[6];
                uint8_t cogMaxSpeed;
                uint8_t cogMaxPosAcc;
                uint8_t reserved3[2];
                uint8_t velLpGain;
                uint8_t cogLpGain;
                uint8_t reserved4[2];
            };
        } // namespace ODO

        namespace RST
        {
            enum ResetMode : uint8_t
            {
                RESETMODE_HARDWARE_RESET_IMMEDIATE      = 0x00,
                RESETMODE_SOFTWARE_RESET                = 0x01,
                RESETMODE_SOFTWARE_RESET_GNSS_ONLY      = 0x02,
                RESETMODE_HARDWARE_RESET_AFTER_SHUTDOWN = 0x04,
                RESETMODE_CONTROLLED_GNSS_STOP          = 0x08,
                RESETMODE_CONTROLLED_GNSS_START         = 0x09
            };

            struct __attribute__((__packed__)) MSG_RST
            {
                uint16_t  navBbrMask;
                ResetMode resetMode;
                uint8_t   reserved1;
            };
        } // namespace RST

        namespace TP5
        {
            struct __attribute__((__packed__)) MSG_TP5
            {
                uint8_t  tpIdx;
                uint8_t  version;
                uint8_t  reserved1[2];
                int16_t  antCableDelay;
                int16_t  rfGroupDelay;
                uint32_t freqPeriod;
                uint32_t freqPeriodLock;
                uint32_t pulseLenRatio;
                uint32_t pulseLenRatioLock;
                int32_t  userConfigDelay;
                uint32_t flags;
            };
        } // namespace TP5

        namespace PMS
        {
            enum PowerSetupValue : uint8_t
            {
                POWERSETUPVALUE_FULL_POWER     = 0x00,
                POWERSETUPVALUE_BALANCED       = 0x01,
                POWERSETUPVALUE_INTERVAL       = 0x02,
                POWERSETUPVALUE_AGGRESSIVE_1HZ = 0x03,
                POWERSETUPVALUE_AGGRESSIVE_2HZ = 0x04,
                POWERSETUPVALUE_AGGRESSIVE_4HZ = 0x05
            };

            struct __attribute__((__packed__)) MSG_PMS
            {
                uint8_t version;
                PowerSetupValue powerSetupValue;
                uint16_t period;
                uint16_t onTime;
                uint8_t reserved1[2];
            };
        } // namespace PMS

        namespace PM2
        {
            enum Flags : uint32_t
            {
                FLAGS_EXTINTSEL = 1 << 4,
                FLAGS_EXTINTWAKE = 1 << 5,
                FLAGS_EXTINTBACKUP = 1 << 6,
                FLAGS_EXTINTINACTIVE = 1 << 7,
                FLAGS_LIMITPEAKCURR_DISABLED = 0b00 << 8,
                FLAGS_LIMITPEAKCURR_ENABLED = 0b01 << 8,
                FLAGS_WAITTIMEFIX = 1 << 10,
                FLAGS_UPDATERTC = 1 << 11,
                FLAGS_UPDATEEPH = 1 << 12,
                FLAGS_DONOTENTEROFF = 1 << 16,
                FLAGS_MODE_ON_OFF = 0b00 << 17,
                FLAGS_MODE_CYCLIC = 0b01 << 17
            };

            // Version 0x02
            struct __attribute__((__packed__)) MSG_PM2
            {
                uint8_t  version;
                uint8_t  reserved1;
                uint8_t  maxStartupStateDur;
                uint8_t  reserved2;
                uint32_t flags;
                uint32_t updatePeriod;
                uint32_t searchPeriod;
                uint32_t gridOffset;
                uint16_t onTime;
                uint16_t minAcqTime;
                uint8_t  reserved3[20];
                uint32_t extintInactivityMs;
            };
        } // namespace PM2

        namespace CFG
        {
            enum ClearMask : uint32_t
            {
                CLEARMASK_IOPORT   = (1 << 0),
                CLEARMASK_MSGCONF  = (1 << 1),
                CLEARMASK_INFMSG   = (1 << 2),
                CLEARMASK_NAVCONF  = (1 << 3),
                CLEARMASK_RXMCONF  = (1 << 4),
                CLEARMASK_SENCONF  = (1 << 8),
                CLEARMASK_RINVCONF = (1 << 9),
                CLEARMASK_ANTCONF  = (1 << 10),
                CLEARMASK_LOGCONF  = (1 << 11),
                CLEARMASK_FTSCONF  = (1 << 12)
            };

            enum DevMask : uint8_t
            {
                DEVMASK_BBR      = (1 << 0),
                DEVMASK_FLASH    = (1 << 1),
                DEVMASK_EEPROM   = (1 << 2),
                DEVMASK_SPIFLASH = (1 << 4)
            };


            struct __attribute__((__packed__)) MSG_CFG
            {
                uint32_t clearMask;
                uint32_t saveMask;
                uint32_t loadMask;
                uint8_t  deviceMask;
            };
        } // namespace RST

        namespace RXM
        {
            enum LpMode : uint8_t
            {
                CONTINUOUS_MODE = 0,
                POWER_SAVE_MODE = 1
            };

            struct __attribute__((__packed__)) MSG_RXM
            {
                uint8_t reserved1;
                LpMode lpMode;
            };
        } // namespace RXM

        namespace NAV5
        {
            enum Mask : uint16_t
            {
                MASK_DYN              = 1 << 0,
                MASK_MIN_EL           = 1 << 1,
                MASK_POS_FIX_MODE     = 1 << 2,
                MASK_DR_LIM           = 1 << 3,
                MASK_POS_MASK         = 1 << 4,
                MASK_TIME_MASK        = 1 << 5,
                MASK_STATIC_HOLD_MASK = 1 << 6,
                MASK_DGPS_MASK        = 1 << 7,
                MASK_CNO_THRESHOLD    = 1 << 8,
                MASK_UTC              = 1 << 10,
            };

            enum DynModel : uint8_t
            {
                DYNMODEL_PORTABLE = 0,
                DYNMODEL_STATIONARY = 2,
                DYNMODEL_PEDESTRIAN = 3,
                DYNMODEL_AUTOMOTIVE = 4,
                DYNMODEL_SEA = 5,
                DYNMODEL_AIRBORNE_1G = 6,
                DYNMODEL_AIRBORNE_2G = 7,
                DYNMODEL_AIRBORNE_4G = 8,
                DYNMODEL_WRIST_WORN_WATCH = 9,
                DYNMODEL_BIKE = 10
            };

            enum FixMode : uint8_t
            {
                FIXMODE_2D_ONLY    = 1,
                FIXMODE_3D_ONLY    = 2,
                FIXMODE_AUTO_2D_3D = 3
            };

            struct __attribute__((__packed__)) MSG_NAV5
            {
                uint16_t mask;
                DynModel dynModel;
                FixMode  fixMode;
                int32_t  fixedAlt;
                uint32_t fixedAltVar;
                int8_t   minElev;
                uint8_t  drLimit;
                uint16_t pDop;
                uint16_t tDop;
                uint16_t pAcc;
                uint16_t tAcc;
                uint8_t  staticHoldThresh;
                uint8_t  dgnssTimeOut;
                uint8_t  cnoThreshNumSVs;
                uint8_t  cnoThresh;
                uint8_t  reserved1[2];
                uint16_t staticHoldMaxDist;
                uint8_t  utcStandard;
                uint8_t  reserved2[5];
            };
        } // namespace NAV5

        namespace NAVX5
        {
            enum Mask1 : uint16_t
            {
                MASK1_MIN_MAX        = 1 << 2,
                MASK1_MIN_CNO        = 1 << 3,
                MASK1_INITIAL_3D_FIX = 1 << 6,
                MASK1_WKN_ROLL       = 1 << 9,
                MASK1_ACK_AID        = 1 << 10,
                MASK1_PPP            = 1 << 13,
                MASK1_AOP            = 1 << 14
            };

            enum Mask2 : uint32_t
            {
                MASK2_ADR            = 1 << 6,
                MASK2_SIG_ATTEN_COMP = 1 << 7
            };

            enum AopCfg : uint8_t
            {
                AOPCFG_AOP_DISABLED  = 0,
                AOPCFG_AOP_ENABLED   = 1 << 0
            };

            // Version 0x0002 of NAVX5
            struct __attribute__((__packed__)) MSG_NAVX5
            {
                uint16_t version;
                uint16_t mask1;
                uint32_t mask2;
                uint8_t  reserved1[2];
                uint8_t  minSVs;
                uint8_t  maxSVs;
                uint8_t  minCNO;
                uint8_t  reserved2;
                uint8_t  iniFix3D;
                uint8_t  reserved3[2];
                uint8_t  ackAiding;
                uint16_t wknRollover;
                uint8_t  sigAttenCompMode;
                uint8_t  reserved4;
                uint8_t  reserved5[2];
                uint8_t  reserved6[2];
                uint8_t  usePPP;
                uint8_t  aopCfg;
                uint8_t  reserved7[2];
                uint16_t aopOrbMaxErr;
                uint8_t  reserved8[4];
                uint8_t  reserved9[3];
                uint8_t  useAdr;
            };
        } // namespace NAVX5

    } // namespace CFG

    /****************************** NAV *************************************/

    namespace NAV
    {
        enum Id : uint8_t
        {
            ID_AOPSTATUS = 0x60,
            ID_CLOCK     = 0x22,
            ID_DGPS      = 0x31,
            ID_DOP       = 0x04,
            ID_POSECEF   = 0x01,
            ID_POSLLH    = 0x02,
            ID_PVT       = 0x07,
            ID_SBAS      = 0x32,
            ID_ORB       = 0x34,
            ID_SOL       = 0x06,
            ID_STATUS    = 0x03,
            ID_SVINFO    = 0x30,
            ID_TIMEGPS   = 0x20,
            ID_TIMEUTC   = 0x21,
            ID_VELECEF   = 0x11,
            ID_VELNED    = 0x12
        };

        namespace AOPSTATUS
        {
            enum AopCfg : uint8_t
            {
                AOPCFG_AOP_DISABLED  = 0,
                AOPCFG_AOP_ENABLED   = 1 << 0
            };

            struct __attribute__((__packed__)) MSG_AOPSTATUS
            {
                uint32_t iTow;
                AopCfg aopCfg;
                uint8_t status;
                uint8_t reserved1[10];
            };
        } // namespace AOPSTATUS

        namespace DOP
        {
            struct __attribute__((__packed__)) MSG_DOP
            {
                uint32_t iTow;
                uint16_t gDOP;
                uint16_t pDOP;
                uint16_t tDOP;
                uint16_t vDOP;
                uint16_t hDOP;
                uint16_t nDOP;
                uint16_t eDOP;
            };
        } // namespace DOP

        namespace PVT
        {
            enum FixType : uint8_t
            {
                FIXTYPE_NO = 0,
                FIXTYPE_DEAD_RECKONING_ONLY = 0,
                FIXTYPE_2D = 2,
                FIXTYPE_3D = 3,
                FIXTYPE_GNSS_AND_DEAD_RECKONING = 4,
                FIXTYPE_TIME_ONLY = 5,
            };

            enum VALID : uint8_t
            {
                VALID_VALID_DATE     = 1 << 0,
                VALID_VALID_TIME     = 1 << 1,
                VALID_FULLY_RESOLVED = 1 << 2,
                VALID_MAG            = 1 << 3,
            };

            struct __attribute__((__packed__)) MSG_PVT
            {
                uint32_t iTow;         // GPS time of week of the navigation epoch
                uint16_t year;
                uint8_t  month;
                uint8_t  day;
                uint8_t  hour;
                uint8_t  min;
                uint8_t  sec;
                uint8_t  valid;        // Validity flags
                uint32_t tAcc;         // Time accuracy estimate in nanoseconds
                int32_t  nano;
                FixType  fixType;      // GNSSfix Type
                uint8_t  flags;        // Fix status flags
                uint8_t  flags2;       // Additional flags
                uint8_t  numSV;        // Number of satellites used in Nav Solution
                int32_t  lon;          // Longitude
                int32_t  lat;          // Latitude
                int32_t  height;       // Height above ellipsoid
                int32_t  hMSL;         // Height above mean sea level
                uint32_t hAcc;         // Horizontal accuracy estimate
                uint32_t vAcc;         // Vertical accuracy estimate
                int32_t  velN;         // NED north velocity
                int32_t  velE;         // NED east velocity
                int32_t  velD;         // NED down velocity
                int32_t  gSpeed;       // Ground speed (2D)
                int32_t  headMot;      // Heading of motion (2D)
                uint32_t sAcc;         // Speed accuracy estimate
                uint32_t headAcc;      // Heading accuracy estimate (both motion and  vehicle)
                uint16_t pDOP;         // Position DOP
                uint8_t  flags3;       // Additional flags
                uint8_t  reserved1[5];
                int32_t  headVeh;      // Heading of vehicle (2D)
                int16_t  magDec;       // Magnetic declination
                uint16_t magAcc;       // Magnetic declination accuracy
            };
        } // namespace PVT

    } // namespace NAV

    /****************************** MGA *************************************/

    namespace MGA
    {
        enum Id : uint8_t
        {
            ID_INI_TIME_UTC = 0x40,
            ID_ACK = 0x60,
            ID_DBD = 0x80
        };

        struct __attribute__((__packed__)) MSG_ACK
        {
            uint8_t  type;
            uint8_t  version;
            uint8_t  infoCode;
            uint8_t  msgId;
            uint32_t msgPayloadStart;
        };

        struct __attribute__((__packed__)) MSG_INI_TIME_UTC
        {
            uint8_t  type;
            uint8_t  version;
            uint8_t  ref;
            int8_t   leapSecs;
            uint16_t year;
            uint8_t  month;
            uint8_t  day;
            uint8_t  hour;
            uint8_t  minute;
            uint8_t  second;
            uint8_t  reserved1;
            uint32_t ns;
            uint16_t tAccS;
            uint8_t  reserved2[2];
            uint32_t tAccNs;
        };
    } // namespace ACK

} // namespace UBX
