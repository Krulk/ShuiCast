//=============================================================================
// ShuiCast v0.47
//-----------------------------------------------------------------------------
// Oddcast       Copyright (C) 2000-2010 Ed Zaleski (Oddsock)
// Edcast        Copyright (C) 2011-2012 Ed Zaleski (Oddsock)
// Edcast-Reborn Copyright (C) 2011-2014 RadioRio(?)
// AltaCast      Copyright (C) 2012-2016 DustyDrifter
// ShuiCast      Copyright (C) 2017-2018 TorteShui
//-----------------------------------------------------------------------------
// Changelog:
// - v0.47 ... Initial version
//-----------------------------------------------------------------------------
// This file is part of ShuiCast.
// 
// ShuiCast is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 2 of the License, or (at your option) any later version.
// 
// ShuiCast is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Foobar. If not, see <http://www.gnu.org/licenses/>.
//=============================================================================

#pragma once
#pragma warning( disable : 4100 4351 4505 )  // unreferenced param, default initialization of array members, unreferenced local function

//-----------------------------------------------------------------------------
// Config section - TODO: own config.h?
//-----------------------------------------------------------------------------

#define HAVE_VORBIS  1
#define HAVE_LAME    1
#define HAVE_AACP    1
#define HAVE_FLAC    1
#define HAVE_FAAC    1  // use 0 for release versions
#define HAVE_FHGAACP 1  // use 0 for release versions

// AAC = libfaac, AAC+ = winamps enc_aacplus.dll (older winamp) or enc_fhgaac.dll (later winamp)
// if you can get your hands on older winamp that has the enc_aacplus.dll, you can copy that to the folder where dsp_shuicast is, and you'll have aacplus working
// or, install dsp_edcastfh - and copy enc_fhgaac.dll, nsutil.dll and libmp4v2.dll from the new winamp (the last two dll's are in the winamp folder, not in plugins)
// you MAY need Microsoft Visual C++ 2008 redistributables for this to work

#define USE_NEW_CONFIG 0
#define USE_BASS_FLOAT 0

//-----------------------------------------------------------------------------
// Header section
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>
#endif

//#include <stdio.h>
//#include <time.h>

#ifndef _WIN32
#include <sys/ioctl.h>
#else
#include <mmsystem.h>
#endif

#ifdef _DMALLOC_
#include <dmalloc.h>
#endif

#if 0//def _UNICODE
#define char_t wchar_t
#define atoi _wtoi
#define LPCSTR LPCWSTR
#define strcpy wcscpy
#define strcmp wcscmp
#define strlen wcslen
#define fopen _wfopen
#define strstr wcsstr
#define sprintf swprintf
#define strcat wcscat
#define fgets fgetws
#else
#define char_t char
#endif

#include "libshuicast_cbuffer.h"
#include "libshuicast_limiters.h"
#include "libshuicast_socket.h"
#include "libshuicast_resample.h"

#if HAVE_VORBIS
#include <vorbis/vorbisenc.h>
#endif

#if HAVE_LAME
#ifdef _WIN32
#include <BladeMP3EncDLL.h>
#else
#include <lame/lame.h>
#endif
#endif

#if HAVE_FAAC
#undef MPEG2  // hack *cough* hack
typedef signed int  int32_t;
#include <faac.h>
#endif

#if HAVE_AACP
#include "libshuicast_enc_if.h"
typedef AudioCoder* (*CREATEAUDIO3TYPE)(int, int, int, unsigned int, unsigned int *, char_t *);
typedef unsigned int( *GETAUDIOTYPES3TYPE )(int, char_t *);
#endif

#if HAVE_FLAC
#include <FLAC/stream_encoder.h>
#include <FLAC/metadata.h>
#endif

#if HAVE_FHGAACP
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.VC90.CRT' version='9.0.21022.8' processorArchitecture='x86' publicKeyToken='1fc8b3b9a1e18e3b'\"")
//#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.VC90.CRT' version='9.0.30729.6161' processorArchitecture='x86' publicKeyToken='1fc8b3b9a1e18e3b'\"")
#endif

//-----------------------------------------------------------------------------
// Debug section
//-----------------------------------------------------------------------------

#define LM_FORCE  0
#define LM_ERROR  1
#define LM_INFO   2
#define LM_DEBUG  3

#ifdef _WIN32
#define LOG_FORCE LM_FORCE, TEXT(__FILE__), __LINE__
#define LOG_ERROR LM_ERROR, TEXT(__FILE__), __LINE__
#define LOG_INFO  LM_INFO, TEXT(__FILE__), __LINE__
#define LOG_DEBUG LM_DEBUG, TEXT(__FILE__), __LINE__
#define FILE_LINE TEXT(__FILE__), __LINE__
#else
#define LOG_FORCE LM_FORCE, __FILE__, __LINE__
#define LOG_ERROR LM_ERROR, __FILE__, __LINE__
#define LOG_INFO  LM_INFO, __FILE__, __LINE__
#define LOG_DEBUG LM_DEBUG, __FILE__, __LINE__
#define FILE_LINE __FILE__, __LINE__
#endif

//-----------------------------------------------------------------------------
// Type definitions
//-----------------------------------------------------------------------------

#ifndef FALSE
#define FALSE false
#endif
#ifndef TRUE
#define TRUE true
#endif

typedef enum
{
    ENCODER_NONE,
    ENCODER_LAME,
    ENCODER_OGG,
    ENCODER_FLAC,
    ENCODER_AAC,
    ENCODER_AACP_HE,
    ENCODER_AACP_HE_HIGH,
    ENCODER_AACP_LC,
    ENCODER_FG_AACP_AUTO,
    ENCODER_FG_AACP_LC,
    ENCODER_FG_AACP_HE,
    ENCODER_FG_AACP_HEV2
}
EncoderType;

typedef enum
{
    SERVER_NONE,
    SERVER_SHOUTCAST,
    SERVER_ICECAST,
    SERVER_ICECAST2
}
ServerType;

typedef struct
{
    int     cbrflag;
    int     out_samplerate;
    int     quality;
#ifdef _WIN32
    int     mode;
#elif HAVE_LAME
    MPEG_mode   mode;
#endif
    int     brate;
    int     copywrite;
    int     original;
    int     strict_ISO;
    int     disable_reservoir;
    char_t  VBR_mode[25];
    int     VBR_mean_bitrate_kbps;
    int     VBR_min_bitrate_kbps;
    int     VBR_max_bitrate_kbps;
    int     lowpassfreq;
    int     highpassfreq;
}
LAMEOptions;

#if 0

typedef struct
{
    char_t  RIFF[4];
    long    chunkSize;
    char_t  WAVE[4];
}
RIFFChunk;

typedef struct
{
    char_t         chunkID[4];
    long           chunkSize;
    short          wFormatTag;
    unsigned short wChannels;
    unsigned long  dwSamplesPerSec;
    unsigned long  dwAvgBytesPerSec;
    unsigned short wBlockAlign;
    unsigned short wBitsPerSample;

    /* Note: there may be additional fields here, depending upon wFormatTag. */
}
FormatChunk;

#define FormatID 'fmt '   /* chunkID for Format Chunk. NOTE: There is a space at the end of this ID. */

typedef struct
{
    char_t  chunkID[4];
    long    chunkSize;
    short * waveformData;
}
DataChunk;

#endif

typedef struct
{
    unsigned int   main_chunk;
    unsigned int   length;
    unsigned int   chunk_type;
    unsigned int   sub_chunk;
    unsigned int   sc_len;
    unsigned short format;
    unsigned short modus;
    unsigned int   sample_fq;
    unsigned int   byte_p_sec;
    unsigned short byte_p_spl;
    unsigned short bit_p_spl;
    unsigned int   data_chunk;
    unsigned int   data_length;
}
WavHeader;

typedef void (*EncoderCallback) ( void*, void* );

typedef void (*VUMeterCallback) ( double, double, double, double );

//-----------------------------------------------------------------------------
// Main class definition
//-----------------------------------------------------------------------------

class CEncoder
{
public:

    CEncoder ( int encoderNumber );
   ~CEncoder ();

    int     Load ();
    void    LoadConfigs                  ( char *currentDir, char *subdir, char * logPrefix, char *currentConfigDir, bool inWinamp ) const;
    void    SetConfigDir                 ( char_t *dirname );
    int     ReadConfigFile               ( const int readOnly = 0 );
    void    WriteConfigFile              ();
    void    SetConfigFileName            ( char_t *configFile );
    void    DeleteConfigFile             ();
    void    SetSaveDirectory             ( const char_t * const saveDir );
    char_t* GetSaveDirectory             ();
    void    SetDefaultLogFileName        ( char_t *filename );
    char_t* GetLogFile                   ();
    void    SetLogFile                   ( char_t *logFile );
    void    LogMessage                   ( int type, const char_t *source, int line, const char_t *fmt, ... );
    int     GetAppData                   ( bool checkonly, int locn, DWORD flags, LPCSTR subdir, LPCSTR configname, LPSTR strdestn ) const;
    bool    CheckLocalDir                ( LPCSTR dir, LPCSTR file ) const;

    int     ConnectToServer              ();
    int     DisconnectFromServer         ();
    int     SendToServer                 ( int sd, char_t *data, int length, int type );
    void    GetCurrentSongTitle          ( char_t *song, char_t *artist, char_t *full ) const;
    int     SetCurrentSongTitle          ( char_t *song );
    char_t* GetLockedMetadata            ();
    void    SetLockedMetadata            ( const char_t * const buf );
    void    AddVorbisComment             ( char_t *comment );
    void    FreeVorbisComments           ();
    int     HandleOutput                 ( float *samples, int nsamples, int nchannels, int in_samplerate, int asioChannel = -1, int asioChannel2 = -1 );
    int     HandleOutputFast             ( Limiters *limiter, int dataoffset = 0 );

    char_t* GetWindowsRecordingDevice    ();
    void    SetWindowsRecordingDevice    ( char_t *device );
    char_t* GetWindowsRecordingSubDevice ();
    void    SetWindowsRecordingSubDevice ( char_t *device );

    void    AddUISettings                ();
    void    AddBasicEncoderSettings      ();
    void    AddMultiEncoderSettings      ();
    void    AddDSPSettings               ();
    void    AddStandaloneSettings        ();
    void    AddASIOSettings              ();

    inline int GetNumEncoders () const
    {
        return m_NumEncoders;
    }

    inline void IncNumEncoders ()
    {
        ++m_NumEncoders;
    }

    inline void DecNumEncoders ()
    {
        --m_NumEncoders;
    }

    inline EncoderType GetEncoderType () const
    {
        return m_Type;
    }

    inline void SetEncoderType ( const EncoderType type )
    {
        m_Type = type;
    }

    inline bool IsEncoderType ( const EncoderType type ) const
    {
        return (m_Type == type);
    }

    inline ServerType GetServerType () const
    {
        return m_ServerType;
    }

    inline void SetServerType ( const ServerType type )
    {
        m_ServerType = type;
    }

    inline bool IsServerType ( const ServerType type ) const
    {
        return (m_ServerType == type);
    }

    inline long GetCurrentSamplerate() const
    {
        return m_CurrentSamplerate;
    }

    inline int GetCurrentBitrate () const
    {
        return m_CurrentBitrate;
    }

    inline void SetUseBitrate ( const bool useBitrate )
    {
        m_UseBitrate = useBitrate;
    }

    inline bool GetUseBitrate () const
    {
        return m_UseBitrate;
    }

    inline int GetCurrentChannels () const
    {
        return m_CurrentChannels;
    }

    inline double GetAttenuation () const
    {
        return m_Attenuation;
    }

    inline void SetAttenuation ( const double attenuation )
    {
        m_Attenuation = attenuation;
    }

    inline int IsConnected () const
    {
        return m_IsConnected;
    }

    inline int GetSCSocket () const
    {
        return m_SCSocket;
    }

    inline long GetLastWindowPosX () const
    {
        return m_LastX;
    }

    inline long GetLastWindowPosY () const
    {
        return m_LastY;
    }

    inline void SetLastWindowPos ( const long x, const long y )
    {
        m_LastX = x;
        m_LastY = y;
    }

    inline void SetLimiterFlag ( const int flag )
    {
        m_Limiter = flag;
    }

    inline void SetLimiterValues ( const int db, const int pre, const int gain )
    {
        m_LimitdB  = db;
        m_LimitPre = pre;
        m_GaindB   = gain;
    }

    inline void SetStartMinimizedFlag ( const int flag )
    {
        m_StartMinimized = flag;
    }

    inline int GetStartMinimizedFlag () const
    {
        return m_StartMinimized;
    }

    inline void SetAutoConnect ( const int flag )
    {
        m_AutoConnect = flag;
    }

    inline int GetAutoConnect() const
    {
        return m_AutoConnect;
    }

    inline void SetForceStop ( const bool forceStop )
    {
        m_ForceStop = forceStop;
    }

    inline int SkipCloseWarning () const
    {
        return m_SkipCloseWarning;
    }

#if HAVE_FLAC
    inline void SetFLACFailure ( const bool failure )
    {
        m_FLACFailure = failure;
    }
#endif

    inline int GetLockedMetadataFlag () const  // TODO: rename GetXXXFlag to IsXXX
    {
        return m_LockSongTitle;
    }

    inline void SetLockedMetadataFlag ( const int flag ) 
    {
        m_LockSongTitle = flag;
    }

    inline int GetVUMeterType () const
    {
        return m_ShowVUMeter;
    }

    inline void SetVUMeterType ( const int type )
    {
        m_ShowVUMeter = type;
    }

    inline void SetDestURLCallback ( EncoderCallback pCallback )
    {
        m_DestURLCallback = pCallback;
    }

    inline void SetSourceURLCallback ( EncoderCallback pCallback )
    {
        m_SourceURLCallback = pCallback;
    }

    inline void SetServerStatusCallback ( EncoderCallback pCallback )
    {
        m_ServerStatusCallback = pCallback;
    }

    inline void SetGeneralStatusCallback ( EncoderCallback pCallback )
    {
        //m_GeneralStatusCallback = pCallback;
    }

    inline void SetWriteBytesCallback ( EncoderCallback pCallback )
    {
        m_WriteBytesCallback = pCallback;
    }

    inline void SetServerTypeCallback ( EncoderCallback pCallback )
    {
        m_ServerTypeCallback = pCallback;
    }

    inline void SetServerNameCallback ( EncoderCallback pCallback )
    {
        m_ServerNameCallback = pCallback;
    }

    inline void SetStreamTypeCallback ( EncoderCallback pCallback )
    {
        m_StreamTypeCallback = pCallback;
    }

    inline void SetBitrateCallback ( EncoderCallback pCallback )
    {
        m_BitrateCallback = pCallback;
    }

    //inline void SetVUMeterCallback ( VUMeterCallback pCallback )
    //{
    //    m_VUMeterCallback = pCallback;
    //}

    inline void SetServerStatus ( const char_t *msg ) const
    {
        if ( m_ServerStatusCallback )
        {
            m_ServerStatusCallback( (void*)this, (void*)msg );
        }
    }

#define DAY_SCHEDULE(dow) \
    private: \
        int       m_##dow##Enable  = 0; \
        int       m_##dow##OnTime  = 0; \
        int       m_##dow##OffTime = 0; \
    public: \
        inline int Get##dow##Enable () const \
        { \
	        return m_##dow##Enable; \
        } \
        inline int Get##dow##OnTime () const \
        { \
	        return m_##dow##OnTime; \
        } \
        inline int Get##dow##OffTime () const \
        { \
	        return m_##dow##OffTime; \
        } \
        inline void Get##dow##Enable ( const int val ) \
        { \
	        m_##dow##Enable = val; \
        } \
        inline void Get##dow##OnTime ( const int val ) \
        { \
	        m_##dow##OnTime = val; \
        } \
        inline void Get##dow##OffTime ( const int val ) \
        { \
	        m_##dow##OffTime = val; \
        }

    DAY_SCHEDULE( Monday    )
    DAY_SCHEDULE( Tuesday   )
    DAY_SCHEDULE( Wednesday )
    DAY_SCHEDULE( Thursday  )
    DAY_SCHEDULE( Friday    )
    DAY_SCHEDULE( Saturday  )
    DAY_SCHEDULE( Sunday    )

protected:

    void      LoadDLL              ( const char_t *name, const char_t *msg = NULL );

    void      LoadConfig           ();
    void      StoreConfig          ();
    void      AddConfigVariable    ( char_t *variable );
    void      GetConfigVariable    ( char_t *appName, char_t *paramName, char_t *defaultvalue, char_t *destValue, int destSize, char_t *desc );
    long      GetConfigVariable    ( char_t *appName, char_t *paramName, long defaultvalue, char_t *desc );
    void      PutConfigVariable    ( char_t *appName, char_t *paramName, char_t *destValue );
    void      PutConfigVariable    ( char_t *appName, char_t *paramName, long value );

    void      OpenArchiveFile      ();
    void      CloseArchiveFile     ();

    int       TriggerDisconnect    ();
    int       UpdateSongTitle      ( int forceURL );
    void      Icecast2SendMetadata ();
    int       OggEncodeDataout     ();
    int       DoEncoding           ( float *samples, int numsamples, Limiters *limiter = NULL );
    int       ConvertAudio         ( float *in_samples, float *out_samples, int num_in_samples, int num_out_samples );
    int       InitResampler        ( long inSampleRate, long inNCH );
    int       ResetResampler       ();

#if HAVE_FAAC
    void      AddToFIFO            ( float *samples, int numsamples );
#endif

    char_t   *GetDescription       ( const char_t *param ) const;
    void      ReplaceString        ( char_t *source, char_t *dest, char_t *from, char_t *to ) const;
    char_t   *URLize               ( char_t *input ) const;

private:

    EncoderType           m_Type                    = ENCODER_NONE;
    ServerType            m_ServerType              = SERVER_NONE;

public:  // TODO
    long                  m_CurrentSamplerate       = 0;  // TODO: add m_ for all, make private
    int                   m_CurrentBitrate          = 0;
    int                   m_CurrentBitrateMin       = 0;
    int                   m_CurrentBitrateMax       = 0;
    int                   m_CurrentChannels         = 0;
    char_t                m_AttenuationTable[30]    = {};
private:
    double                m_Attenuation             = 0;
    int                   m_SCSocket                = 0;
    int                   m_SCSocketCtrl            = 0;
    CMySocket             m_DataChannel;
    CMySocket             m_CtrlChannel;

    bool                  m_SCFlag                  = false;
    char_t                m_SourceURL[1024]         = {};
public:  // TODO
    char_t                m_Server[256]             = {};
    char_t                m_Port[10]                = {};
    char_t                m_Password[256]           = {};
    int                   m_IsConnected             = 0;
    char_t                m_CurrentSong[1024]       = {};
    int                   m_PubServ                 = 0;
    char_t                m_ServIRC[20]             = {};
    char_t                m_ServICQ[20]             = {};
    char_t                m_ServAIM[20]             = {};
    char_t                m_ServURL[1024]           = {};
    char_t                m_ServDesc[1024]          = {};
    char_t                m_ServName[1024]          = {};
    char_t                m_ServGenre[100]          = {};
    char_t                m_Mountpoint[100]         = {};
private:
    int                   m_AutoConnect             = 0;  // is used
    int                   m_AutoReconnect           = 0;  // TODO
public:  // TODO
    int                   m_ReconnectSec            = 0;
    bool                  m_CurrentlyEncoding       = false;
    char_t                m_OggQuality[25]          = {};
    int                   m_LiveRecordingFlag       = 0;
    int                   m_Limiter                 = 0;
    int                   m_LimitPre                = 0;
    int                   m_LimitdB                 = 0;
    int                   m_GaindB                  = 0;
private:
    int                   m_StartMinimized          = 0;
    bool                  m_UseBitrate              = false;
private:
    char_t                m_OggBitQual[40]          = {};
public:  // TODO
    char_t                m_EncodeType[25]          = {};
    char_t                m_SaveDirectory[1024]     = {};
    char_t                m_LogFile[1024]           = {};
    int                   m_LogLevel                = 0;
private:
    FILE                 *m_LogFilePtr              = NULL;
public:  // TODO
    int                   m_SaveDirectoryFlag       = 0;
    int                   m_SaveAsWAV               = 0;
    FILE                 *m_SaveFilePtr             = NULL;
    int                   m_AsioSelectChannel       = 0;
    char_t                m_AsioChannel[255]        = {};
    int                   m_EnableScheduler         = 0;

private:

    bool                  m_Ice2songChange          = false;
    bool                  m_InHeader                = false;
    long                  m_ArchiveWritten          = 0;
    int                   m_ShowVUMeter             = 0;

    char_t                m_SongTitle[1024]         = {};    // TODO: must have same length as m_CurrentSong!
    char_t                m_ManualSongTitle[1024]   = {};
    int                   m_LockSongTitle           = 0;
    int                   m_NumEncoders             = 0;     // TODO: make static s_NumEncoders?

    res_state             m_Resampler               = {};    // TODO: this should be a class
    bool                  m_ResamplerInitialized    = false;
    EncoderCallback       m_SourceURLCallback       = NULL;  // TODO: use subclasses instead of callbacks? maybe not
    EncoderCallback       m_DestURLCallback         = NULL;
    EncoderCallback       m_ServerStatusCallback    = NULL;
  //EncoderCallback       m_GeneralStatusCallback   = NULL;
    EncoderCallback       m_WriteBytesCallback      = NULL;
    EncoderCallback       m_ServerTypeCallback      = NULL;
    EncoderCallback       m_ServerNameCallback      = NULL;
    EncoderCallback       m_StreamTypeCallback      = NULL;
    EncoderCallback       m_BitrateCallback         = NULL;
    VUMeterCallback       m_VUMeterCallback         = NULL;
public:  // TODO
    time_t                m_StartTime               = 0;  // TODO: Get/Set/Reset methods
    time_t                m_EndTime                 = 0;
private:
    char_t                m_SourceDesc[255]         = {};
public:  // TODO
    char_t                m_ServerTypeName[25]      = {};  // TODO: make getter to return string based on m_ServerType

#if 0
#ifdef _WIN32
    WAVEFORMATEX          m_WaveFormat              = {};
    HWAVEIN               m_InHandle                = NULL;
    WAVEHDR               m_WaveBuffer1             = {};
    WAVEHDR               m_WaveBuffer2             = {};
#else
    int                   m_InHandle                = 0;  // for advanced recording
#endif
    unsigned long         m_Result                  = 0;
    short int             WAVsamplesbuffer1[1152*2] = {};
    short int             WAVsamplesbuffer2[1152*2] = {};
#endif
    char_t                m_AdvRecDevice[255]       = {};
    int                   m_LiveInSamplerate        = 0;  // TODO: unused

    HINSTANCE             m_hDLL                    = NULL;
    char_t                m_ConfigFileName[255]     = {};

private:

    long                  m_LastX                   = 0;
    long                  m_LastY                   = 0;

#if HAVE_LAME  // TODO: these things are candidates for child classes
    LAMEOptions           m_LAMEOptions             = {};
    int                   m_LAMEHighpassFlag        = 0;
    int                   m_LAMELowpassFlag         = 0;
    int                   m_LAMEPreset              = 0;
#ifdef _WIN32
    BEINITSTREAM          m_beInitStream            = NULL;
    BEENCODECHUNK         m_beEncodeChunk           = NULL;
    BEDEINITSTREAM        m_beDeinitStream          = NULL;
    BECLOSESTREAM         m_beCloseStream           = NULL;
    BEVERSION             m_beVersion               = NULL;
    BEWRITEVBRHEADER      m_beWriteVBRHeader        = NULL;
    HBE_STREAM            m_hbeStream               = 0;
    DWORD                 m_LAMESamples             = 0;
    DWORD                 m_LAMEMP3Buffer           = 0;
#else
    lame_global_flags    *m_LAMEGlobalFlags         = NULL;
    char_t                m_LAMEBasicPreset[255]    = {};
    char_t                m_LAMEAltPreset[255]      = {};
#endif
#endif

#if HAVE_VORBIS
    ogg_stream_state      m_OggStreamState          = {};
    vorbis_dsp_state      m_VorbisDSPState          = {};
    vorbis_block          m_VorbisBlock             = {};
    vorbis_info           m_VorbisInfo              = {};
#endif

#if HAVE_AACP
    CREATEAUDIO3TYPE      m_CreateAudio3            = NULL;
    GETAUDIOTYPES3TYPE    m_GetAudioTypes3          = NULL;
    AudioCoder         *(*m_FinishAudio3)    (       char_t *filename, AudioCoder *coder ) = NULL;
    void                (*m_PrepareToFinish) ( const char_t *filename, AudioCoder *coder ) = NULL;
    AudioCoder           *m_AACPEncoder             = NULL;
#endif

#if ( HAVE_FAAC || HAVE_FHGAACP )
    faacEncHandle         m_AACEncoder              = NULL;
    unsigned long         m_AACSamplesInput         = 0;
    unsigned long         m_AACMaxBytesOutput       = 0;
    float                *m_AACFIFO                 = NULL;
    long                  m_AACFIFOEndPos           = 0;
#endif

#if HAVE_FLAC
    FLAC__StreamEncoder  *m_FLACEncoder             = NULL;
    FLAC__StreamMetadata *m_FLACMetadata            = NULL;
    bool                  m_FLACFailure             = false;
#endif

public:  // TODO

    char_t                m_AACQuality[25]          = {};
    char_t                m_AACCutoff[25]           = {};
    int                   encoderNumber             = 0;
    bool                  m_ForcedDisconnect        = false;
    time_t                m_ForcedDisconnectSecs    = 0;
    char_t                externalMetadata[20]      = {};
    char_t                externalURL[255]          = {};
    char_t                externalFile[255]         = {};
    char_t                externalInterval[25]      = {};
    char_t               *vorbisComments[30]        = {};
    int                   numVorbisComments         = 0;
    char_t                outputControl[255]        = {};
    char_t                metadataAppendString[255] = {};
    char_t                metadataRemoveStringBefore[255] = {};
    char_t                metadataRemoveStringAfter[255]  = {};
    char_t                metadataWindowClass[255]  = {};
    bool                  metadataWindowClassInd    = false;

private:

    char_t               *m_ConfigVars[255]         = {};
    int                   m_NumConfigVars           = 0;
    pthread_mutex_t       m_Mutex                   = {};

    char_t                m_RecDevice[255]          = {};
    char_t                m_RecSubDevice[255]       = {};
public:  // TODO
    int                   m_JointStereo             = 0;
    int                   m_ForceDSPRecording       = 0;
    int                   m_ThreeHourBug            = 0;
private:
    bool                  m_ForceStop               = false;
    int                   m_SkipCloseWarning        = 0;
    int                   m_AsioRate                = 0;
    WavHeader             m_WavHeader               = {};
  //CBUFFER               m_CircularBuffer;
};
