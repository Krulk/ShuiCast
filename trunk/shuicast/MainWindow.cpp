/*$T MainWindow.cpp GC 1.140 10/23/05 09:48:26 */

/*
 * MainWindow.cpp : implementation file ;
 */
#include "stdafx.h"
#include "shuicast.h"
#include "MainWindow.h"
#include "libshuicast.h"
#include <process.h>
#include <bass.h>
#include <math.h>
#include <afxinet.h>
#include <windows.h>

#include "About.h"
#include "SystemTray.h"
#include "libshuicast_limiters.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char				THIS_FILE[] = __FILE__;
#endif
#define WM_MY_NOTIFY	WM_USER + 10
#define WM_MY_MESSAGE	WM_USER+998
Limiters *				limiter = NULL;
Limiters *				dsplimiter = NULL;

CMainWindow				*pWindow;

int						shuicast_init(shuicastGlobals *g);

unsigned int			shuicastThread = 0;

shuicastGlobals			*g[MAX_ENCODERS];
shuicastGlobals			gMain;

int						m_BASSOpen = 0;

bool					gLiveRecording = false;
HRECORD					inRecHandle;
static int				oldLeft = 0;
static int				oldRight = 0;
HDC						specdc = 0;
HBITMAP					specbmp = 0;
BYTE					*specbuf;
DWORD					timer = 0;

extern char    logPrefix[255];
char	currentConfigDir[MAX_PATH] = "";

/*
 * define SPECWIDTH 320 // display width ;
 * #define SPECHEIGHT 127 // height (changing requires palette adjustments too)
 */
#define SPECWIDTH	76						/* display width */
#define SPECHEIGHT	30						/* height (changing requires palette adjustments too) */

static UINT BASED_CODE	indicators[] = { ID_STATUSPANE };

extern "C"
{
	int startshuicastThread(void *obj) {
		CMainWindow *pWindow = (CMainWindow *) obj;
		pWindow->startshuicast(-1);
		//Begin patch for multiple cpu
		HANDLE hProc = GetCurrentProcess();//Gets the current process handle
	      DWORD procMask;
	      DWORD sysMask;
	      HANDLE hDup;
	      DuplicateHandle(hProc,
	                    hProc,
	                    hProc,
	                    &hDup,
	                    0,
	                    FALSE,
	                    DUPLICATE_SAME_ACCESS);
	
	      GetProcessAffinityMask(hDup,&procMask,&sysMask);//Gets the current process affinity mask
	      DWORD newMask = 2;//new Mask, uses only the first CPU
	      BOOL res = SetProcessAffinityMask(hDup,(DWORD_PTR)newMask);//Set the affinity mask for the process
		//end patch multiple cpu 
		//_endthread();
		return(1);
	}
}
extern "C"
{
	int startSpecificshuicastThread(void *obj) {
		int		enc = (int) obj;
		/*
		 * CMainWindow *pWindow = (CMainWindow *)obj;
		 */
		int		ret = pWindow->startshuicast(enc);
		time_t	currentTime;
		currentTime = time(&currentTime);
		g[enc]->forcedDisconnectSecs = currentTime;
		//Begin patch multiple cpu
		HANDLE hProc = GetCurrentProcess();//Gets the current process handle
	      DWORD procMask;
	      DWORD sysMask;
	      HANDLE hDup;
	      DuplicateHandle(hProc,
	                    hProc,
	                    hProc,
	                    &hDup,
	                    0,
	                    FALSE,
	                    DUPLICATE_SAME_ACCESS);
	
	      GetProcessAffinityMask(hDup,&procMask,&sysMask);//Gets the current process affinity mask
	      DWORD newMask = 2;//new Mask, uses only the first CPU
	      BOOL res = SetProcessAffinityMask(hDup,(DWORD_PTR)newMask);//Set the affinity mask for the process
		//End patch multiple cpu 
		//_endthread();
		return(1);
	}
}

VOID CALLBACK ReconnectTimer(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	time_t	currentTime;
	currentTime = time(&currentTime);
	for(int i = 0; i < gMain.gNumEncoders; i++) {
		if(g[i]->forcedDisconnect) {
			int timeout = getReconnectSecs(g[i]);
			time_t timediff = currentTime - g[i]->forcedDisconnectSecs;
			if(timediff > timeout) {
				g[i]->forcedDisconnect = false;
				_beginthreadex(NULL,
							   0,
							   (unsigned(_stdcall *) (void *)) startSpecificshuicastThread,
							   (void *) i,
							   0,
							   &shuicastThread);
//Begin patch multiple cpu
HANDLE hProc = GetCurrentProcess();//Gets the current process handle
      DWORD procMask;
      DWORD sysMask;
      HANDLE hDup;
      DuplicateHandle(hProc,
                    hProc,
                    hProc,
                    &hDup,
                    0,
                    FALSE,
                    DUPLICATE_SAME_ACCESS);

      GetProcessAffinityMask(hDup,&procMask,&sysMask);//Gets the current process affinity mask
      DWORD newMask = 2;//new Mask, uses only the first CPU
      BOOL res = SetProcessAffinityMask(hDup,(DWORD_PTR)newMask);//Set the affinity mask for the process
//End patch multiple cpu 
			}
			else {
				char	buf[255] = "";
				wsprintf(buf, "Connecting in %d seconds", timeout - timediff);
				pWindow->outputStatusCallback(i + 1, buf, FILE_LINE);
			}
		}
	}
}

VOID CALLBACK MetadataTimer(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	if(!strcmp(gMain.externalMetadata, "FILE")) {
		FILE	*filep = fopen(gMain.externalFile, "r");
		if(!filep) {
			char	buf[1024] = "";
			wsprintf(buf, "Cannot open metadata file (%s)", gMain.externalFile);
			pWindow->generalStatusCallback(buf, FILE_LINE);
		}
		else {
			char	buffer[1024];
			memset(buffer, '\000', sizeof(buffer));
			fgets(buffer, sizeof(buffer) - 1, filep);

			char	*p1;
			p1 = strstr(buffer, "\r\n");
			if(p1) {
				*p1 = '\000';
			}

			p1 = strstr(buffer, "\n");
			if(p1) {
				*p1 = '\000';
			}

			fclose(filep);
			setMetadata(buffer);
		}
	}

	if(!strcmp(gMain.externalMetadata, "URL")) {
		char	szCause[255];

		TRY
		{
			CInternetSession	session("shuicast");
			CStdioFile			*file = NULL;
			file = session.OpenURL(gMain.externalURL, 1, INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE);
			if(file == NULL) {
				char	buf[1024] = "";
				wsprintf(buf, "Cannot open metadata URL (%s)", gMain.externalURL);
				pWindow->generalStatusCallback(buf, FILE_LINE);
			}
			else {
				CString metadata;
				file->ReadString(metadata);
				setMetadata((char *) LPCSTR(metadata));
				file->Close();
				delete file;
			}

			session.Close();
			delete session;
		}

		CATCH_ALL(error) {
			error->GetErrorMessage(szCause, 254, NULL);
			pWindow->generalStatusCallback(szCause, FILE_LINE);
		}

		END_CATCH_ALL;
	}
}

char	lastWindowTitleString[4096] = "";

#define UNICODE
VOID CALLBACK MetadataCheckTimer(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	pWindow->KillTimer(5);
	if(gMain.metadataWindowClassInd) {
		if(strlen(gMain.metadataWindowClass) > 0) {
			HWND	winHandle = FindWindow(gMain.metadataWindowClass, NULL);
			if(winHandle) {
				char	buf[1024] = "";
				GetWindowText(winHandle, buf, sizeof(buf) - 1);
				if(!strcmp(buf, lastWindowTitleString)) {
					;
				}
				else {
					setMetadata(buf);
					strcpy(lastWindowTitleString, buf);
				}
			}
		}
	}
	pWindow->SetTimer(5, 1000, (TIMERPROC) MetadataCheckTimer);
}

#undef UNICODE
VOID CALLBACK AutoConnectTimer(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	pWindow->DoConnect();
	pWindow->generalStatusCallback("", FILE_LINE);
	pWindow->KillTimer(pWindow->autoconnectTimerId);
}

void freeComment() {
	for(int i = 0; i < gMain.gNumEncoders; i++) {
		if (g[i]->numVorbisComments) {
			freeVorbisComments(g[i]);
		}
	}
	return;
}
void addComment(char *comment) {
	for(int i = 0; i < gMain.gNumEncoders; i++) {
		if(g[i]->gOggFlag) {
			if(g[i]->weareconnected) {
				addVorbisComment(g[i], comment);
			}
		}
	}

	return;
}

#if 1  // altacast
int handleAllOutput(float *samples, int nsamples, int nchannels, int in_samplerate)
{
	long	ileftMax = 0;
	long	irightMax = 0;
	long	leftMax = 0;
	long	rightMax = 0;
	double	sumLeft = 0.0;
	double	sumRight = 0.0;

	int		samplecounter = 0;
	if(nchannels == 2) {
		for(int i = 0; i < nsamples * 2; i = i + 2) {
			ileftMax = abs((int) ((float) samples[i] * 32767.f));
			irightMax = abs((int) ((float) samples[i + 1] * 32767.f));
			sumLeft = sumLeft + (ileftMax * ileftMax);
			sumRight = sumRight + (irightMax * irightMax);
			if(ileftMax > leftMax) {
				leftMax = ileftMax;
			}

			if(irightMax > rightMax) {
				rightMax = irightMax;
			}
		}
	}
	else {
		for(int i = 0; i < nsamples; i = i + 1) {
			ileftMax = abs((int) ((float) samples[i] * 32767.f));
			irightMax = ileftMax;
			sumLeft = sumLeft + (ileftMax * ileftMax);
			sumRight = sumRight + (irightMax * irightMax);
			if(ileftMax > leftMax) {
				leftMax = ileftMax;
			}

			if(irightMax > rightMax) {
				rightMax = irightMax;
			}
		}
	}

	/*
	 * if (nsamples > 0) { leftMax = leftMax/(nsamples);
	 * rightMax = rightMax/(nsamples);
	 * }
	 */
	double	RMSLeft = sqrt(sumLeft);
	double	RMSRight = sqrt(sumRight);

	double	newL = (double) 20 * log10((double) leftMax / 32768.0);
	double	newR = (double) 20 * log10((double) rightMax / 32768.0);

	/*
	 * double newL = (double)20 * log10((double)RMSLeft/32768.0);
	 * double newR = (double)20 * log10((double)RMSRight/32768.0);
	 */
	UpdatePeak((int) newL + 60, (int) newR + 60, 0, 0);
    /*
	if(gMain.vuShow == 2)
	{
		UpdatePeak((int) limiter->PeakL + 60, (int) limiter->PeakR + 60, 0, 0);
	}
	else
	{
		UpdatePeak((int) limiter->RmsL + 60, (int) limiter->RmsR + 60, (int) limiter->PeakL + 60, (int) limiter->PeakR + 60);
	}
    */
	for(int i = 0; i < gMain.gNumEncoders; i++) {
		handle_output(g[i], samples, nsamples, nchannels, in_samplerate);
	}

	return 1;
}
#else  // edcast-reborn
int handleAllOutput(float *samples, int nsamples, int nchannels, int in_samplerate)
{
	if(nchannels > 2)
	{
		if(limiter == NULL)
		{
			limiter = (Limiters *) new limitMultiMono(nchannels);
		}
		if(pWindow->m_Limiter)
		{
			limiter->multiLimit(samples, nsamples, in_samplerate, NULL, NULL, NULL);//0.55, -3.0);
		}
		else
		{
			limiter->multiLimit(samples, nsamples, in_samplerate, NULL, NULL, NULL);//0.0, 20.0);
		}
	}
	else
	{
		if(limiter != NULL)
		{
			if((nchannels == 1 && limiter->sourceIsStereo) || (nchannels == 2 && !limiter->sourceIsStereo))
			{
				delete limiter;
				limiter = NULL;
			}
		}
		if(limiter == NULL)
		{
			if(nchannels == 1)
			{
				limiter = (Limiters *) new limitMonoToStereoMono();
			}
			else
			{
				limiter = (Limiters *) new limitStereoToStereoMono();
			}
		}

		if(pWindow->m_Limiter)
		{
			limiter->limit(samples, nsamples, in_samplerate, pWindow->m_limitpre, pWindow->m_limitdb, pWindow->m_gaindb);
		}
		else
		{
			limiter->limit(samples, nsamples, in_samplerate, 0.0, 20.0);
		}
	}

	if(gMain.vuShow == 2)
	{
		UpdatePeak((int) limiter->PeakL + 60, (int) limiter->PeakR + 60, 0, 0);
	}
	else
	{
		UpdatePeak((int) limiter->RmsL + 60, (int) limiter->RmsR + 60, (int) limiter->PeakL + 60, (int) limiter->PeakR + 60);
	}
	
	for(int i = 0; i < gMain.gNumEncoders; i++) 
	{
		handleOut(g[i], limiter); // sub::
	}
	return 1;
}
#endif

//common
void UpdatePeak(int rmsL, int rmsR, int peakL, int peakR)
{
	pWindow->flexmeters.GetMeterInfoObject(0)->value = rmsL;
	pWindow->flexmeters.GetMeterInfoObject(1)->value = rmsR;
	pWindow->flexmeters.GetMeterInfoObject(0)->peak = peakL;
	pWindow->flexmeters.GetMeterInfoObject(1)->peak = peakR;
}

bool LiveRecordingCheck()
{
	return gLiveRecording;
}

bool HaveEncoderAlwaysDSP() // DSP
{
	for(int i = 0; i < gMain.gNumEncoders; i++) 
	{
		if(g[i]->gForceDSPrecording) // flagged for always DSP
		{
			return true;
		}
	}
	return false;
}
//common
int getLastX()
{
	return getLastXWindow(&gMain);
}

int getLastY()
{
	return getLastYWindow(&gMain);
}

void setLastX(int x)
{
	setLastXWindow(&gMain, x);
}

void setLastY(int y)
{
	setLastYWindow(&gMain, y);
}

void setLiveRecFlag(int live)
{
	gMain.gLiveRecordingFlag = live;
}

void setLimiterVals(int db, int pre, int gain)
{
	setLimiterValues(&gMain, db, pre, gain);
}

void setLimiter(int limiter)
{
	setLimiterFlag(&gMain, limiter);
}

void setStartMinimized(int mini)
{
	setStartMinimizedFlag(&gMain, mini);
}

void setAuto(int flag)
{
	setAutoConnect(&gMain, flag);
}

void writeMainConfig()
{
	writeConfigFile(&gMain);
}

int initializeshuicast() {
    char    currentlogFile[1024] = "";
    wsprintf(currentlogFile, "%s\\%s", currentConfigDir, logPrefix);

    setDefaultLogFileName(currentlogFile);
    setgLogFile(&gMain, currentlogFile);
    setConfigFileName(&gMain, currentlogFile);

	addUISettings(&gMain);


	return shuicast_init(&gMain);
}

void setMetadataFromMediaPlayer(char *metadata)
{
	if(gMain.metadataWindowClassInd) 
	{
		return;
	}

	if(!strcmp(gMain.externalMetadata, "DISABLED")) 
	{
		setMetadata(metadata);
	}
}

void setMetadata(char *metadata)
{
	char	modifiedSong[4096] = "";
	char	modifiedSongBuffer[4096] = "";
	char	*pData;

	strcpy(modifiedSong, metadata);
	if(strlen(gMain.metadataRemoveStringAfter) > 0) {
		char	*p1 = strstr(modifiedSong, gMain.metadataRemoveStringAfter);
		if(p1) {
			*p1 = '\000';
		}
	}

	if(strlen(gMain.metadataRemoveStringBefore) > 0) {
		char	*p1 = strstr(modifiedSong, gMain.metadataRemoveStringBefore);
		if(p1) {
			memset(modifiedSongBuffer, '\000', sizeof(modifiedSongBuffer));
			strcpy(modifiedSongBuffer, p1 + strlen(gMain.metadataRemoveStringBefore));
			strcpy(modifiedSong, modifiedSongBuffer);
		}
	}

	if(strlen(gMain.metadataAppendString) > 0) {
		strcat(modifiedSong, gMain.metadataAppendString);
	}

	pData = modifiedSong;

	pWindow->m_Metadata = modifiedSong;
	pWindow->inputMetadataCallback(0, (void *) pData, FILE_LINE);

	for(int i = 0; i < gMain.gNumEncoders; i++) {
		if(getLockedMetadataFlag(&gMain)) {
			if(setCurrentSongTitle(g[i], (char *) getLockedMetadata(&gMain))) {
				pWindow->inputMetadataCallback(i, (void *) getLockedMetadata(&gMain), FILE_LINE);
			}
		}
		else {
			if(setCurrentSongTitle(g[i], (char *) pData)) {
				pWindow->inputMetadataCallback(i, (void *) pData, FILE_LINE);
			}
		}
	}
}

void LoadConfigs(char *currentDir, char *logFile)
{
	char	configFile[1024] = "";
	char	currentlogFile[1024] = "";

    
	strcpy(currentConfigDir, currentDir);



	wsprintf(configFile, "%s\\%s", currentConfigDir, logPrefix);
	wsprintf(currentlogFile, "%s\\%s", currentConfigDir, logPrefix);

    setDefaultLogFileName(currentlogFile);
    setgLogFile(&gMain, currentlogFile);
    setConfigFileName(&gMain, configFile);
	addUISettings(&gMain);
	readConfigFile(&gMain);
}

BOOL CALLBACK BASSwaveInputProc(HRECORD handle, const void *buffer, DWORD length, void *user) {
	int			n;
	char		*name;
	static char currentDevice[1024] = "";

	if(gLiveRecording) {
		for(n = 0; name = (char *)BASS_RecordGetInputName(n); n++) {
			float currentVolume;

			int s = BASS_RecordGetInput(n, &currentVolume);
			if(!(s & BASS_INPUT_OFF)) {
				if(strcmp(currentDevice, name)) {
					strcpy(currentDevice, name);

					char	msg[255] = "";
					wsprintf(msg, "Recording from %s", currentDevice);
					pWindow->generalStatusCallback((void *) msg, FILE_LINE);
				}

				/*
				 * setCurrentRecordingName(currentDevice);
				 */
			}
		}

		unsigned int	c_size = length;	/* in bytes. */
		short			*z = (short *) buffer;	/* signed short for pcm data. */

		int				numsamples = c_size / sizeof(short);

		int				nch = 2;
		int				srate = 44100;
		float			*samples;

		/*
		 * float samples[8196*16];
		 */
		samples = (float *) malloc(sizeof(float) * numsamples * 2);
		memset(samples, '\000', sizeof(float) * numsamples * 2);

		long	avgLeft = 0;
		long	avgRight = 0;
		int		flip = 0;

		for(int i = 0; i < numsamples; i++) {
			signed int	sample;
			sample = z[i];
			samples[i] = sample / 32767.f;

			/* clipping */
			if(samples[i] > 1.0) {
				samples[i] = 1.0;
			}

			if(samples[i] < -1.0) {
				samples[i] = -1.0;
			}
		}

		handleAllOutput((float *) samples, numsamples / nch, nch, srate);

		/*
		 * int ret;
		 * for (int j=0;
		 * j<gMain.gNumEncoders;
		 * j++) { ret = handle_output(g[j], (float *)samples, numsamples/nch, nch, srate);
		 * }
		 */
		free(samples);
		return 1;
	}
	else {
		return 0;
	}

	return 0;
}

/*
 =======================================================================================================================
    void CALLBACK UpdateSpectrum(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2) { HDC dc;
    int x,y;
    float fft[512];
    // get the FFT data BASS_ChannelGetData(i nRecHandle,fft,BASS_DATA_FFT1024);
    int b0=0;
    memset(specbuf,0,SPECWIDTH*SPECHEIGHT);
    #define BANDS 15 for (x=0;
    x<BANDS;
    x++) { float sum=0;
    int sc,b1=pow(2,x*10.0/(BANDS-1));
    if (b1>511) b1=511;
    if (b1<=b0) b1=b0+1;
    // make sure it uses at least 1 FFT bin sc=10+b1-b0;
    for (;
    b0<b1;
    b0++) sum+=fft[1+b0];
    y=(sqrt(sum/log10(sc))*1.7*SPECHEIGHT)-4;
    // scale it if (y>SPECHEIGHT) y=SPECHEIGHT;
    // cap it while (--y>=0) memset(specbuf+y*SPECWIDTH+x*(SPECWIDTH/BANDS),y+1,0.9*(SPECWIDTH/BANDS));
    // draw bar } // update the display dc=GetDC(pWindow->m_hWnd);
    //int bmpPos = (int)(pWindow->gWindowHeight / 2.475);
    BitBlt(dc,315,70,SPECWIDTH,SPECHEIGHT,specdc,0,0,SRCCOPY);
    ReleaseDC(pWindow->m_hWnd,dc);
    }
 =======================================================================================================================
 */
void stopRecording() {
	BASS_ChannelStop(inRecHandle);
	m_BASSOpen = 0;
	BASS_RecordFree();
	gLiveRecording = false;
}

int startRecording(int m_CurrentInputCard) {
	char	buffer[1024] = "";
	char	buf[255] = "";

	int		ret = BASS_RecordInit(m_CurrentInputCard);
	m_BASSOpen = 1;

	if(!ret) {
		DWORD	errorCode = BASS_ErrorGetCode();
		switch(errorCode) {
			case BASS_ERROR_ALREADY:
				pWindow->generalStatusCallback((char *) "Recording device already opened!", FILE_LINE);
				return 0;

			case BASS_ERROR_DEVICE:
				pWindow->generalStatusCallback((char *) "Recording device invalid!", FILE_LINE);
				return 0;

			case BASS_ERROR_DRIVER:
				pWindow->generalStatusCallback((char *) "Recording device driver unavailable!", FILE_LINE);
				return 0;

			default:
				pWindow->generalStatusCallback((char *) "There was an error opening the preferred Digital Audio In device!", FILE_LINE);
				return 0;
		}
	}

	inRecHandle = BASS_RecordStart(44100, 2, 0, &BASSwaveInputProc, NULL);

	int		n = 0;
	char	*name;
	for(n = 0; name = (char *)BASS_RecordGetInputName(n); n++) {
		float vol = 0.0;
		int s = BASS_RecordGetInput(n, &vol);
		if(!(s & BASS_INPUT_OFF)) {
			char	msg[255] = "";
			wsprintf(msg, "Recording from %s", name);
			pWindow->generalStatusCallback((void *) msg, FILE_LINE);
		}
	}

	gLiveRecording = true;

	return 1;
}

/* CMainWindow dialog */
const char	*kpcTrayNotificationMsg_ = "shuicast";

CMainWindow::CMainWindow(CWnd *pParent /* NULL */ ) :
	CDialog(CMainWindow::IDD, pParent),
	bMinimized_(false),
	pTrayIcon_(0),
	nTrayNotificationMsg_(RegisterWindowMessage(kpcTrayNotificationMsg_))
{
	//{{AFX_DATA_INIT(CMainWindow)
	m_Bitrate = _T("");
	m_Destination = _T("");
	m_Bandwidth = _T("");
	m_Metadata = _T("");
	m_ServerDescription = _T("");
	m_LiveRec = FALSE;
	m_RecDevices = _T("");
	m_RecCards = _T("");
	m_RecVolume = 0;
	m_AutoConnect = FALSE;
	m_startMinimized = FALSE;
	m_Limiter = FALSE;
	m_StaticStatus = _T("");
	//}}AFX_DATA_INIT
	hIcon_ = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	memset(g, '\000', sizeof(g));
	m_BASSOpen = 0;
	pWindow = this;
	memset(m_currentDir, '\000', sizeof(m_currentDir));
	strcpy(m_currentDir, ".");
	LogMessage(&gMain, LOG_DEBUG, "CMainWindow complete");
}

CMainWindow::~CMainWindow() 
{
	for(int i = 0; i < MAX_ENCODERS; i++) 
	{
		if(g[i]) 
		{
			free(g[i]);
		}
	}
}

void CMainWindow::InitializeWindow()
{
	configDialog = new CConfig();
	configDialog->Create((UINT) IDD_CONFIG, this);
	configDialog->parentDialog = this;

	editMetadata = new CEditMetadata();
	editMetadata->Create((UINT) IDD_EDIT_METADATA, this);
	editMetadata->parentDialog = this;

	aboutBox = new CAbout();
	aboutBox->Create((UINT) IDD_ABOUT, this);
	LogMessage(&gMain, LOG_DEBUG, "CMainWindow::InitializeWindow complete");
}

LRESULT CMainWindow::startMinimized(WPARAM wParam, LPARAM lParam)
{
	if(gMain.gStartMinimized)
	{
		bMinimized_ = true;
		SetupTrayIcon();
		SetupTaskBarButton();
	}
	return 0L;
}

void CMainWindow::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CMainWindow)
	DDX_Control(pDX, IDC_RECCARDS, m_RecCardsCtrl);
	DDX_Control(pDX, IDC_VUONOFF, m_OnOff);
	DDX_Control(pDX, IDC_AUTOCONNECT, m_AutoConnectCtrl);
	DDX_Control(pDX, IDC_RECVOLUME, m_RecVolumeCtrl);
	DDX_Control(pDX, IDC_RECDEVICES, m_RecDevicesCtrl);
	DDX_Control(pDX, IDC_CONNECT, m_ConnectCtrl);
	DDX_Control(pDX, IDC_LIVEREC, m_LiveRecCtrl);
	DDX_Control(pDX, IDC_ENCODERS, m_Encoders);
	DDX_Text(pDX, IDC_METADATA, m_Metadata);
	DDX_Check(pDX, IDC_LIVEREC, m_LiveRec);
	DDX_CBString(pDX, IDC_RECDEVICES, m_RecDevices);
	DDX_CBString(pDX, IDC_RECCARDS, m_RecCards);
	DDX_Slider(pDX, IDC_RECVOLUME, m_RecVolume);
	DDX_Check(pDX, IDC_AUTOCONNECT, m_AutoConnect);
	DDX_Text(pDX, IDC_STATIC_STATUS, m_StaticStatus);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CMainWindow, CDialog)
//{{AFX_MSG_MAP(CMainWindow)
	ON_BN_CLICKED(IDC_CONNECT, OnConnect)
	ON_BN_CLICKED(IDC_ADD_ENCODER, OnAddEncoder)
	ON_NOTIFY(NM_DBLCLK, IDC_ENCODERS, OnDblclkEncoders)
	ON_NOTIFY(NM_RCLICK, IDC_ENCODERS, OnRclickEncoders)
	ON_COMMAND(ID_POPUP_CONFIGURE, OnPopupConfigure)
	ON_COMMAND(ID_POPUP_CONNECT, OnPopupConnect)
	ON_BN_CLICKED(IDC_LIVEREC, OnLiverec)
	ON_COMMAND(ID_POPUP_DELETE, OnPopupDelete)
	ON_CBN_SELCHANGE(IDC_RECDEVICES, OnSelchangeRecdevices)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_MANUALEDIT_METADATA, OnManualeditMetadata)
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_COMMAND(ID_ABOUT_ABOUT, OnAboutAbout)
	ON_COMMAND(ID_ABOUT_HELP, OnAboutHelp)
	ON_WM_PAINT()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_METER, OnMeter)
	ON_NOTIFY(LVN_KEYDOWN, IDC_ENCODERS, OnKeydownEncoders)
	ON_BN_CLICKED(IDC_BUTTON1, OnButton1)
	ON_NOTIFY(NM_SETFOCUS, IDC_ENCODERS, OnSetfocusEncoders)
	ON_WM_QUERYDRAGICON()
	ON_CBN_SELCHANGE(IDC_RECCARDS, OnSelchangeReccards)
	//}}AFX_MSG_MAP
	ON_WM_SYSCOMMAND()
	ON_COMMAND(IDI_RESTORE, OnSTRestore)
END_MESSAGE_MAP()

void CMainWindow::generalStatusCallback(void *pValue, char *source, int line)
{
	LogMessage(&gMain, LM_INFO, source, line, "%s", (char *)pValue);
	SetDlgItemText(IDC_STATIC_STATUS, (char *) pValue);
	m_StaticStatus = (char*) pValue;
}

void CMainWindow::inputMetadataCallback(int enc, void *pValue, char *source, int line)
{
	SetDlgItemText(IDC_METADATA, (char *) pValue);
	if (enc == 0) {
		LogMessage(&gMain, LM_INFO, source, line, "%s", (char *)pValue);
	}
	else {
		LogMessage(g[enc-1],LM_INFO, source, line,  "%s", (char *)pValue);
	}
}

void CMainWindow::outputStatusCallback(int enc, void *pValue, char *source, int line, bool bSendToLog)
{
	if(enc != 0) 
	{
		if(bSendToLog)
		{
			LogMessage(g[enc-1], LM_INFO, source, line, "%s", (char *)pValue);
		}
		else
		{
			LogMessage(g[enc-1], LM_DEBUG, source, line, "%s", (char *)pValue);
		}

		int numItems = m_Encoders.GetItemCount();
		if(enc - 1 >= numItems) 
		{
			m_Encoders.InsertItem(enc - 1, (char *) "");
		}

		m_Encoders.SetItemText(enc - 1, 1, (char *) pValue);
	}
}

void CMainWindow::outputMountCallback(int enc, void *pValue)
{
	if(enc != 0)
	{
		int numItems = m_Encoders.GetItemCount();
		if(enc - 1 >= numItems) 
		{
			m_Encoders.InsertItem(enc - 1, (char *) "");
		}

		m_Encoders.SetItemText(enc - 1, 2, (char *) pValue);
	}
}

void CMainWindow::outputChannelCallback(int enc, void *pValue)
{
	if(enc != 0)
	{
		int numItems = m_Encoders.GetItemCount();
		if(enc - 1 >= numItems) 
		{
			m_Encoders.InsertItem(enc - 1, (char *) "");
		}

		m_Encoders.SetItemText(enc - 1, 3, (char *) pValue);
	}
}

void CMainWindow::writeBytesCallback(int enc, void *pValue)
{

	/* pValue is a long */
	static time_t startTime[MAX_ENCODERS];
	static time_t endTime[MAX_ENCODERS];
	static long bytesWrittenInterval[MAX_ENCODERS];
	static long totalBytesWritten[MAX_ENCODERS];
	static int	initted = 0;
	char		kBPSstr[255] = "";

	if(!initted) {
		initted = 1;
		memset(&startTime, '\000', sizeof(startTime));
		memset(&endTime, '\000', sizeof(endTime));
		memset(&bytesWrittenInterval, '\000', sizeof(bytesWrittenInterval));
		memset(&totalBytesWritten, '\000', sizeof(totalBytesWritten));
	}

	if(enc != 0) {
		int		enc_index = enc - 1;
		long	bytesWritten = (long) pValue;

		if(bytesWritten == -1) {
			strcpy(kBPSstr, "");
			outputStatusCallback(enc, kBPSstr, FILE_LINE);
			startTime[enc_index] = 0;
			return;
		}

		if(startTime[enc_index] == 0) {
			startTime[enc_index] = time(&(startTime[enc_index]));
			bytesWrittenInterval[enc_index] = 0;
		}

		bytesWrittenInterval[enc_index] += bytesWritten;
		totalBytesWritten[enc_index] += bytesWritten;
		endTime[enc_index] = time(&(endTime[enc_index]));
		if((endTime[enc_index] - startTime[enc_index]) > 4) {
			time_t		bytespersec = bytesWrittenInterval[enc_index] / (endTime[enc_index] - startTime[enc_index]);
			long	kBPS = (long)((bytespersec * 8) / 1000);
			if(strlen(g[enc_index]->gMountpoint) > 0) {
				wsprintf(kBPSstr, "%ld Kbps (%s)", kBPS, g[enc_index]->gMountpoint);
			}
			else {
				wsprintf(kBPSstr, "%ld Kbps", kBPS);
			}

			outputStatusCallback(enc, kBPSstr, FILE_LINE, false);
			startTime[enc_index] = 0;
		}
	}
}

void CMainWindow::outputServerNameCallback(int enc, void *pValue)
{

	/*
	 * SetDlgItemText(IDC_SERVER_DESC, (char *)pValue);
	 */
	;
}

void CMainWindow::outputBitrateCallback(int enc, void *pValue)
{
	if(enc != 0) {
		int numItems = m_Encoders.GetItemCount();
		if(enc - 1 >= numItems) {
			m_Encoders.InsertItem(enc - 1, (char *) pValue);
		}
		else {
			m_Encoders.SetItemText(enc - 1, 0, (char *) pValue);
		}
	}
}

void CMainWindow::outputStreamURLCallback(int enc, void *pValue)
{

	/*
	 * SetDlgItemText(IDC_DESTINATION_LOCATION, (char *)pValue);
	 */
}

void CMainWindow::stopshuicast()
{
	for(int i = 0; i < gMain.gNumEncoders; i++) 
	{
		setForceStop(g[i], 1);
		disconnectFromServer(g[i]);
		g[i]->forcedDisconnect = false;
	}
}

int CMainWindow::startshuicast(int which)
{
	if(which == -1)
	{
		for(int i = 0; i < gMain.gNumEncoders; i++) 
		{
			if(!g[i]->weareconnected) 
			{
				setForceStop(g[i], 0);
				if(!connectToServer(g[i])) 
				{
					g[i]->forcedDisconnect = true;
					continue;
				}
			}
		}
	}
	else 
	{
		if(!g[which]->weareconnected) 
		{
			setForceStop(g[which], 0);

			int ret = connectToServer(g[which]);
			if(ret == 0) 
			{
				g[which]->forcedDisconnect = true;
			}
		}
	}

	return 1;
}

void CMainWindow::DoConnect() 
{
	OnConnect();
	generalStatusCallback("AutoConnect", FILE_LINE);
	KillTimer(autoconnectTimerId);
}

void CMainWindow::OnConnect()
{
	static bool connected = false;

	if(!connected) 
	{
		_beginthreadex(NULL, 0, (unsigned(_stdcall *) (void *)) startshuicastThread, (void *) this, 0, &shuicastThread);
//Begin patch multiple cpu
HANDLE hProc = GetCurrentProcess();//Gets the current process handle
      DWORD procMask;
      DWORD sysMask;
      HANDLE hDup;
      DuplicateHandle(hProc,
                    hProc,
                    hProc,
                    &hDup,
                    0,
                    FALSE,
                    DUPLICATE_SAME_ACCESS);

      GetProcessAffinityMask(hDup,&procMask,&sysMask);//Gets the current process affinity mask
      DWORD newMask = 2;//new Mask, uses only the first CPU
      BOOL res = SetProcessAffinityMask(hDup,(DWORD_PTR)newMask);//Set the affinity mask for the process
//End patch multiple cpu 

		connected = true;
		m_ConnectCtrl.SetWindowText("Disconnect");
		KillTimer(2);
		reconnectTimerId = SetTimer(2, 1000, (TIMERPROC) ReconnectTimer);

	}
	else 
	{
		stopshuicast();
		connected = false;
		m_ConnectCtrl.SetWindowText("Connect");
		KillTimer(2);
	}
}

void CMainWindow::OnAddEncoder()
{

	int orig_index = gMain.gNumEncoders;
	g[orig_index] = (shuicastGlobals *) malloc(sizeof(shuicastGlobals));

	memset(g[orig_index], '\000', sizeof(shuicastGlobals));

	g[orig_index]->encoderNumber = orig_index + 1;


    char    currentlogFile[1024] = "";

	wsprintf(currentlogFile, "%s\\%s_%d", currentConfigDir, logPrefix, g[orig_index]->encoderNumber);



	setDefaultLogFileName(currentlogFile);

	setgLogFile(g[orig_index], currentlogFile);
	setConfigFileName(g[orig_index], gMain.gConfigFileName);
	gMain.gNumEncoders++;
	initializeGlobals(g[orig_index]);
    addBasicEncoderSettings(g[orig_index]);


	shuicast_init(g[orig_index]);
}

void CMainWindow::SetupEncoderDisplay() // override in multi
{
	RECT	rect;

	rect.left = 340;
	rect.top = 190;
	m_Encoders.InsertColumn(0, "Encoder Settings");
	m_Encoders.InsertColumn(1, "Transfer Rate");
	m_Encoders.InsertColumn(2, "Mount");

	m_Encoders.SetColumnWidth(0, 190);
	m_Encoders.SetColumnWidth(1, 110);
	m_Encoders.SetColumnWidth(2, 90);

	m_Encoders.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_Encoders.SendMessage(LB_SETTABSTOPS, 0, NULL);
}

BOOL CMainWindow::OnInitDialog()
{
	CDialog::OnInitDialog();

	//SetWindowText("shuicast");

	RECT	rect;

	rect.left = 340;
	rect.top = 190;

	m_Encoders.InsertColumn(0, "Encoder Settings");
	m_Encoders.InsertColumn(1, "Transfer Rate");

	m_Encoders.SetColumnWidth(0, 195);
	m_Encoders.SetColumnWidth(1, 200);

	m_Encoders.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_Encoders.SendMessage(LB_SETTABSTOPS, 0, NULL);
	liveRecOn.LoadBitmap(IDB_LIVE_ON);
	liveRecOff.LoadBitmap(IDB_LIVE_OFF);

#ifdef SHUICASTSTANDALONE
	gMain.gLiveRecordingFlag = 1;
#endif
	m_LiveRec = gMain.gLiveRecordingFlag;
	if(m_LiveRec) {
		m_LiveRecCtrl.SetBitmap(HBITMAP(liveRecOn));
	}
	else {
		m_LiveRecCtrl.SetBitmap(HBITMAP(liveRecOff));
	}

	for(int i = 0; i < gMain.gNumEncoders; i++) 
	{
		if(!g[i]) 
		{
			g[i] = (shuicastGlobals *) malloc(sizeof(shuicastGlobals));
			memset(g[i], '\000', sizeof(shuicastGlobals));
		}

		g[i]->encoderNumber = i + 1;
		char    currentlogFile[1024] = "";

		wsprintf(currentlogFile, "%s\\%s_%d", currentConfigDir, logPrefix, g[i]->encoderNumber);
		setDefaultLogFileName(currentlogFile);
		setgLogFile(g[i], currentlogFile);
		setConfigFileName(g[i], gMain.gConfigFileName);
		initializeGlobals(g[i]);
	    addBasicEncoderSettings(g[i]);

		shuicast_init(g[i]);
	}

	int		count = 0;	/* the device counter */
	char	*pDesc = (char *) 1;

	BASS_RecordInit(0);

	m_BASSOpen = 1;

	int		n;
	char	*name;
	int		currentInput = 0;

	BASS_DEVICEINFO info;

	for (int a=0; BASS_RecordGetDeviceInfo(a, &info); a++) {
		if (info.flags&BASS_DEVICE_ENABLED) {
			m_RecCardsCtrl.AddString(info.name);
			if (!strcmp(getWindowsRecordingDevice(&gMain), "")) {
				m_RecCards = info.name;
				m_CurrentInputCard = a;
			}
			else {
				if (!strcmp(getWindowsRecordingDevice(&gMain), info.name)) {
					m_RecCards = info.name;
					m_CurrentInputCard = a;
				}
			}
		}
		
	}
	for(n = 0; name = (char *)BASS_RecordGetInputName(n); n++) {
		float vol = 0.0;
		int s = BASS_RecordGetInput(n, &vol);
		m_RecDevicesCtrl.AddString(name);
		if(s & BASS_INPUT_OFF) {
			;
		}
		else {
			m_RecDevices = name;
			//m_RecVolume = LOWORD(s);
			m_RecVolume = (int)vol*100;
			m_CurrentInput = n;
		}
	}

	m_BASSOpen = 0;
	BASS_RecordFree();

	if(getLockedMetadataFlag(&gMain)) {
		m_Metadata = getLockedMetadata(&gMain);
	}

	m_AutoConnect = gMain.autoconnect;
	UpdateData(FALSE);
#ifdef SHUICASTSTANDALONE
	m_LiveRecCtrl.SetBitmap(HBITMAP(liveRecOn));
	m_RecDevicesCtrl.EnableWindow(TRUE);
	m_RecCardsCtrl.EnableWindow(TRUE);
	m_RecVolumeCtrl.EnableWindow(TRUE);
	startRecording(m_CurrentInputCard);
#endif
	OnLiverec();
	reconnectTimerId = SetTimer(2, 1000, (TIMERPROC) ReconnectTimer);
	if(m_AutoConnect) {
		char	buf[255];
		wsprintf(buf, "AutoConnecting in 5 seconds");
		generalStatusCallback(buf, FILE_LINE);
		autoconnectTimerId = SetTimer(3, 5000, (TIMERPROC) AutoConnectTimer);
	}

	int metadataInterval = atoi(gMain.externalInterval);
	metadataInterval = metadataInterval * 1000;
	metadataTimerId = SetTimer(4, metadataInterval, (TIMERPROC) MetadataTimer);

	SetTimer(5, 1000, (TIMERPROC) MetadataCheckTimer);

	FlexMeters_InitStruct	*fmis = flexmeters.Initialize_Step1();	/* returns a pointer to a struct, */

	/* with default values filled in. */
	fmis->max_x = 512;	/* buffer size. must be at least as big as the meter window */
	fmis->max_y = 512;	/* buffer size. must be at least as big as the meter window */

	fmis->hWndFrame = GetDlgItem(IDC_METER)->m_hWnd;	/* the window to grab coordinates from */

	fmis->border.left = 3;					/* borders. */
	fmis->border.right = 3;
	fmis->border.top = 3;
	fmis->border.bottom = 3;

	fmis->meter_count = 2;					/* number of meters */

	fmis->horizontal_meters = 1;			/* 0 = vertical */

	flexmeters.Initialize_Step2(fmis);		/* news meter info objects. after this, you must set them up. */

	int a = 0;

	for(a = 0; a < fmis->meter_count; a++) {
		CFlexMeters_MeterInfo	*pMeterInfo = flexmeters.GetMeterInfoObject(a);

		pMeterInfo->extra_spacing = 3;

		/* nice gradient */
		pMeterInfo->colour[0].colour = 0x00FF00;
		pMeterInfo->colour[0].at_value = 0;

		pMeterInfo->colour[1].colour = 0xFFFF00;
		pMeterInfo->colour[1].at_value = 55;

		pMeterInfo->colour[2].colour = 0xFF0000;
		pMeterInfo->colour[2].at_value = 58;

		pMeterInfo->colours_used = 3;

		pMeterInfo->value = 0;
		pMeterInfo->peak = 0;
		pMeterInfo->meter_width = 60;

		/*
		 * if(a & 1) pMeterInfo->direction=eMeterDirection_Backwards;
		 */
	}

	flexmeters.Initialize_Step3();			/* finalize init. */

	if(gMain.vuShow) {
		m_VUStatus = VU_ON;
		m_OnOff.ShowWindow(SW_HIDE);
	}
	else {
		m_VUStatus = VU_OFF;
		m_OnOff.ShowWindow(SW_SHOW);
	}

	SetTimer(73, 50, 0);					/* set up timer. 20ms=50hz - probably good. */

	return TRUE;							/* return TRUE unless you set the focus to a control */
	/* EXCEPTION: OCX Property Pages should return FALSE */
}

void CMainWindow::OnDblclkEncoders(NMHDR *pNMHDR, LRESULT *pResult)
{

	OnPopupConfigure();
	*pResult = 0;
}

void CMainWindow::OnRclickEncoders(NMHDR *pNMHDR, LRESULT *pResult)
{

	int iItem = m_Encoders.GetNextItem(-1, LVNI_SELECTED);

	if(iItem >= 0) 
	{

		CMenu	menu;
		VERIFY(menu.LoadMenu(IDR_CONTEXT));

		/* Pop up sub menu 0 */
		CMenu	*popup = menu.GetSubMenu(0);

		if(popup) 
		{
			if(g[iItem]->weareconnected) 
			{
				popup->ModifyMenu(ID_POPUP_CONNECT, MF_BYCOMMAND, ID_POPUP_CONNECT, "Disconnect");
			}
			else 
			{
				if(g[iItem]->forcedDisconnect) 
				{
					popup->ModifyMenu(ID_POPUP_CONNECT, MF_BYCOMMAND, ID_POPUP_CONNECT, "Stop AutoConnect");
				}
				else 
				{
					popup->ModifyMenu(ID_POPUP_CONNECT, MF_BYCOMMAND, ID_POPUP_CONNECT, "Connect");
				}
			}

			POINT	pt;
			GetCursorPos(&pt);
			popup->TrackPopupMenu(TPM_LEFTALIGN, pt.x, pt.y, this);
		}
	}

	*pResult = 0;
}

void CMainWindow::OnPopupConfigure()
{
	int iItem = m_Encoders.GetNextItem(-1, LVNI_SELECTED);

	if(iItem >= 0) 
	{
		configDialog->GlobalsToDialog(g[iItem]);
		configDialog->ShowWindow(SW_SHOW);
	}
}

void CMainWindow::OnPopupConnect()
{
	int iItem = m_Encoders.GetNextItem(-1, LVNI_SELECTED);

	if(iItem >= 0) 
	{
		if(!g[iItem]->weareconnected) 
		{
			if(g[iItem]->forcedDisconnect) 
			{
				g[iItem]->forcedDisconnect = 0;
				outputStatusCallback(iItem + 1, "AutoConnect stopped.", FILE_LINE);
			}
			else 
			{
				m_SpecificEncoder = iItem;
				_beginthreadex(NULL,
							   0,
							   (unsigned(_stdcall *) (void *)) startSpecificshuicastThread,
							   (void *) iItem,
							   0,
							   &shuicastThread);
//Begin patch multiple cpu
HANDLE hProc = GetCurrentProcess();//Gets the current process handle
      DWORD procMask;
      DWORD sysMask;
      HANDLE hDup;
      DuplicateHandle(hProc,
                    hProc,
                    hProc,
                    &hDup,
                    0,
                    FALSE,
                    DUPLICATE_SAME_ACCESS);

      GetProcessAffinityMask(hDup,&procMask,&sysMask);//Gets the current process affinity mask
      DWORD newMask = 2;//new Mask, uses only the first CPU
      BOOL res = SetProcessAffinityMask(hDup,(DWORD_PTR)newMask);//Set the affinity mask for the process
//End patch multiple cpu 
			}
		}
		else 
		{
			disconnectFromServer(g[iItem]);
			setForceStop(g[iItem], 1);
			g[iItem]->forcedDisconnect = false;
		}
	}
}

void CMainWindow::OnLimiter()
{
	UpdateData(TRUE);
}

void CMainWindow::OnStartMinimized()
{
	UpdateData(TRUE);
}

void CMainWindow::OnLiverec()
{
#ifdef SHUICASTSTANDALONE
	return;
#endif

	UpdateData(TRUE);
	if(m_LiveRec) {
		m_LiveRecCtrl.SetBitmap(HBITMAP(liveRecOn));
		m_RecDevicesCtrl.EnableWindow(TRUE);
		m_RecCardsCtrl.EnableWindow(TRUE);
		m_RecVolumeCtrl.EnableWindow(TRUE);
		startRecording(m_CurrentInputCard);
	}
	else {
		m_LiveRecCtrl.SetBitmap(HBITMAP(liveRecOff));
		m_RecCardsCtrl.EnableWindow(FALSE);
		m_RecDevicesCtrl.EnableWindow(FALSE);
		m_RecVolumeCtrl.EnableWindow(FALSE);
		generalStatusCallback((void *) "Recording from DSP", FILE_LINE);
		stopRecording();
	}
}

void CMainWindow::ProcessConfigDone(int enc, CConfig *pConfig)
{
	if(enc > 0) 
	{
		pConfig->DialogToGlobals(g[enc - 1]);
		writeConfigFile(g[enc - 1]);
		shuicast_init(g[enc - 1]);
	}

	SetFocus();
	m_Encoders.SetFocus();
}

void CMainWindow::ProcessEditMetadataDone(CEditMetadata *pConfig)
{
	pConfig->UpdateData(TRUE);

	bool	ok = true;

	if(pConfig->m_ExternalFlag == 0) 
	{
		strcpy(gMain.externalMetadata, "URL");
		ok = false;
	}

	if(pConfig->m_ExternalFlag == 1) 
	{
		strcpy(gMain.externalMetadata, "FILE");
		ok = false;
	}

	if(pConfig->m_ExternalFlag == 2) 
	{
		strcpy(gMain.externalMetadata, "DISABLED");
	}

	strcpy(gMain.externalInterval, LPCSTR(pConfig->m_MetaInterval));
	strcpy(gMain.externalFile, LPCSTR(pConfig->m_MetaFile));
	strcpy(gMain.externalURL, LPCSTR(pConfig->m_MetaURL));

	strcpy(gMain.metadataAppendString, LPCSTR(pConfig->m_AppendString));
	strcpy(gMain.metadataRemoveStringBefore, LPCSTR(pConfig->m_RemoveStringBefore));
	strcpy(gMain.metadataRemoveStringAfter, LPCSTR(pConfig->m_RemoveStringAfter));
	strcpy(gMain.metadataWindowClass, LPCSTR(pConfig->m_WindowClass));

	if(ok) 
	{
		setLockedMetadata(&gMain, (char *) LPCSTR(pConfig->m_Metadata));
		setLockedMetadataFlag(&gMain, editMetadata->m_LockMetadata);
		if(strlen((char *) LPCSTR(pConfig->m_Metadata)) > 0) 
		{
			setMetadata((char *) LPCSTR(pConfig->m_Metadata));
		}
	}

	gMain.metadataWindowClassInd = pConfig->m_WindowTitleGrab ? true : false;
	KillTimer(metadataTimerId);
	int metadataInterval = atoi(gMain.externalInterval);
	metadataInterval = metadataInterval * 1000;
	metadataTimerId = SetTimer(4, metadataInterval, (TIMERPROC) MetadataTimer);

	SetFocus();
}

void CMainWindow::OnPopupDelete()
{
	int i = 0;
	for(i = 0; i < gMain.gNumEncoders; i++) 
	{
		if(g[i]->weareconnected) 
		{
			MessageBox("You need to disconnect all the encoders before deleting one from the list", "Message", MB_OK);
			return;
		}
	}

	int iItem = m_Encoders.GetNextItem(-1, LVNI_SELECTED);

	if(iItem >= 0) 
	{
		int ret = MessageBox("Delete this encoder ?", "Message", MB_YESNO);
		if(ret == IDYES) 
		{
			if(g[iItem]) 
			{
				deleteConfigFile(g[iItem]);
				free(g[iItem]);
			}

			m_Encoders.DeleteAllItems();
			for(i = iItem; i < gMain.gNumEncoders; i++) 
			{
				if(g[i + 1]) 
				{
					g[i] = g[i + 1];
					g[i + 1] = 0;
					deleteConfigFile(g[i]);
					g[i]->encoderNumber--;
					writeConfigFile(g[i]);
				}
			}

			gMain.gNumEncoders--;
			for(i = 0; i < gMain.gNumEncoders; i++) 
			{
				shuicast_init(g[i]);
			}
		}
	}
}

void CMainWindow::CleanUp()
{
	timeKillEvent(timer);
	Sleep(100);
	if(specbmp) 
	{
		DeleteObject(specbmp);
	}

	if(specdc) 
	{
		DeleteDC(specdc);
	}

	if(gLiveRecording)
	{
		stopRecording();
	}
}

void CMainWindow::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	bool	opened = false;

	if(pScrollBar->m_hWnd == m_RecVolumeCtrl.m_hWnd) 
	{
		UpdateData(TRUE);
		if(!m_BASSOpen) {
			int ret = BASS_RecordInit(m_CurrentInputCard);
			m_BASSOpen = 1;
			opened = true;
		}

		BASS_RecordSetInput(m_CurrentInput, BASS_INPUT_ON, (float)((float)m_RecVolume/100));
		if(opened) {
			m_BASSOpen = 0;
			BASS_RecordFree();
		}
	}

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CMainWindow::OnManualeditMetadata()
{
	editMetadata->m_LockMetadata = gMain.gLockSongTitle;
	editMetadata->m_Metadata = gMain.gManualSongTitle;

	if(!strcmp("DISABLED", gMain.externalMetadata)) 
	{
		editMetadata->m_ExternalFlag = 2;
	}
	else if(!strcmp("FILE", gMain.externalMetadata)) 
	{
		editMetadata->m_ExternalFlag = 1;
	}
	else if(!strcmp("URL", gMain.externalMetadata)) 
	{
		editMetadata->m_ExternalFlag = 0;
	}

	editMetadata->m_MetaFile = gMain.externalFile;
	editMetadata->m_MetaURL = gMain.externalURL;
	editMetadata->m_MetaInterval = gMain.externalInterval;

	editMetadata->m_AppendString = gMain.metadataAppendString;
	editMetadata->m_RemoveStringAfter = gMain.metadataRemoveStringAfter;
	editMetadata->m_RemoveStringBefore = gMain.metadataRemoveStringBefore;
	editMetadata->m_WindowClass = gMain.metadataWindowClass;

	editMetadata->m_WindowTitleGrab = gMain.metadataWindowClassInd;

	editMetadata->UpdateRadio();
	editMetadata->UpdateData(FALSE);
	editMetadata->ShowWindow(SW_SHOW);
}

void CMainWindow::OnClose()
{
#ifndef SHUICASTSTANDALONE
	bMinimized_ = true;
	SetupTrayIcon();
	SetupTaskBarButton();
#else
	OnDestroy();
	EndModalLoop(1);
	exit(1);
#endif
}

void CMainWindow::OnDestroy()
{
	RECT	pRect;
	bMinimized_ = false;
	SetupTrayIcon();

	GetWindowRect(&pRect);
	UpdateData(TRUE);
	setLastX(pRect.left);
	setLastY(pRect.top);
	setLiveRecFlag(m_LiveRec);
	setAuto(m_AutoConnect);
	setStartMinimized(m_startMinimized);
	setLimiter(m_Limiter);
	setLimiterVals(m_limitdb, m_limitpre, m_gaindb);
	if(gLiveRecording) 
	{
		stopRecording();
	}
	stopshuicast();
	CleanUp();
	if(configDialog) 
	{
		configDialog->DestroyWindow();
		delete configDialog;
	}
	if(editMetadata) 
	{
		editMetadata->DestroyWindow();
		delete editMetadata;
	}
	if(aboutBox) 
	{
		aboutBox->DestroyWindow();
		delete aboutBox;
	}

	writeMainConfig();

	/*
	 * DestroyWindow();
	 */
	CDialog::OnDestroy();
}

void CMainWindow::OnAboutAbout()
{
	aboutBox->m_Version = _T("Built on : "__DATE__ " "__TIME__);
	aboutBox->m_Configpath.Format(_T("Config: %s"), currentConfigDir);
	aboutBox->UpdateData(FALSE);
	aboutBox->ShowWindow(SW_SHOW);
}

void CMainWindow::OnAboutHelp()
{
	char	loc[2046] = "";
	wsprintf(loc, "%s\\%s", m_currentDir, "shuicast.chm");

	HINSTANCE	ret = ShellExecute(NULL, "open", loc, NULL, NULL, SW_SHOWNORMAL);
}

void CMainWindow::OnPaint()
{
	CPaintDC	dc(this);					/* device context for painting */
}

void CMainWindow::OnTimer(UINT nIDEvent)
{
	if(nIDEvent == 73) 
	{
		int			a = 0;
		static int	oldL = 0;
		static int	oldR = 0;
		static int	oldPeakL = 0;
		static int	oldPeakR = 0;
		static int	oldCounter = 0;

		if(m_VUStatus == VU_OFF) {
			return;
		}

		HWND	hWnd = GetDlgItem(IDC_METER)->m_hWnd;

		HDC		hDC = ::GetDC(hWnd);	/* get the DC for the window. */
		if((m_VUStatus == VU_ON) || (m_VUStatus == VU_SWITCHOFF)) 
		{
			if((flexmeters.GetMeterInfoObject(0)->value == -1) && (flexmeters.GetMeterInfoObject(1)->value == -1)) 
			{
				if(oldCounter > 10) 
				{
					flexmeters.GetMeterInfoObject(0)->value = 0;
					flexmeters.GetMeterInfoObject(1)->value = 0;
					flexmeters.GetMeterInfoObject(0)->peak = 0;
					flexmeters.GetMeterInfoObject(1)->peak = 0;
					oldCounter = 0;
				}
				else 
				{
					flexmeters.GetMeterInfoObject(0)->value = oldL;
					flexmeters.GetMeterInfoObject(1)->value = oldR;
					flexmeters.GetMeterInfoObject(0)->peak = oldPeakL;
					flexmeters.GetMeterInfoObject(1)->peak = oldPeakR;
					oldCounter++;
				}
			}
			else 
			{
				oldCounter = 0;
			}

			if(m_VUStatus == VU_SWITCHOFF) 
			{
				flexmeters.GetMeterInfoObject(0)->value = 0;
				flexmeters.GetMeterInfoObject(1)->value = 0;
				flexmeters.GetMeterInfoObject(0)->peak = 0;
				flexmeters.GetMeterInfoObject(1)->peak = 0;
				m_VUStatus = VU_OFF;
			}

			flexmeters.RenderMeters(hDC);	/* render */
			oldL = flexmeters.GetMeterInfoObject(0)->value;
			oldR = flexmeters.GetMeterInfoObject(1)->value;
			oldPeakL = flexmeters.GetMeterInfoObject(0)->peak;
			oldPeakR = flexmeters.GetMeterInfoObject(1)->peak;

			flexmeters.GetMeterInfoObject(0)->value = -1;
			flexmeters.GetMeterInfoObject(1)->value = -1;
			flexmeters.GetMeterInfoObject(0)->peak = -1;
			flexmeters.GetMeterInfoObject(1)->peak = -1;

		}
		else 
		{
			flexmeters.GetMeterInfoObject(0)->value = 0;
			flexmeters.GetMeterInfoObject(1)->value = 0;
			flexmeters.GetMeterInfoObject(0)->peak = 0;
			flexmeters.GetMeterInfoObject(1)->peak = 0;
			flexmeters.RenderMeters(hDC);	/* render */
		}
		::ReleaseDC(hWnd, hDC);			/* release the DC */
	}

	CDialog::OnTimer(nIDEvent);
}

void CMainWindow::OnMeter()
{
	if(m_VUStatus == VU_ON) 
	{
		m_VUStatus = VU_SWITCHOFF;
		gMain.vuShow = 0;
		m_OnOff.ShowWindow(SW_SHOW);
	}
	else 
	{
		m_VUStatus = VU_ON;
		gMain.vuShow = 1;
		m_OnOff.ShowWindow(SW_HIDE);
	}
}

void CMainWindow::OnSTExit()
{
	OnCancel();
}

void CMainWindow::OnSTRestore()
{
	ShowWindow(SW_RESTORE);
	bMinimized_ = false;
	SetupTrayIcon();
	SetupTaskBarButton();
}

void CMainWindow::SetupTrayIcon()
{
	if(bMinimized_ && (pTrayIcon_ == 0)) 
	{
		pTrayIcon_ = new CSystemTray;
		pTrayIcon_->Create(0, nTrayNotificationMsg_, "ShuiCast Restore", hIcon_, IDR_SYSTRAY);
		pTrayIcon_->SetMenuDefaultItem(IDI_RESTORE, false);
		pTrayIcon_->SetNotifier(this);
	}
	else 
	{
		delete pTrayIcon_;
		pTrayIcon_ = 0;
	}
}

/*
 =======================================================================================================================
    SetupTaskBarButton Show or hide the taskbar button for this app, depending on whether ;
    we're minimized right now or not.
 =======================================================================================================================
 */
void CMainWindow::SetupTaskBarButton()
{
	/* Show or hide this window appropriately */
	if(bMinimized_) 
	{
		ShowWindow(SW_HIDE);
	}
	else 
	{
		ShowWindow(SW_SHOW);
	}
}

void CMainWindow::OnSysCommand(UINT nID, LPARAM lParam)
{
	DoSysCommand(nID, lParam);
}

void CMainWindow::OnSelchangeRecdevices()
{
	char	*name;
	char	selectedDevice[1024] = "";
	bool	opened = false;
	char	msg[2046] = "";

	

	int		index = m_RecDevicesCtrl.GetCurSel();
	memset(selectedDevice, '\000', sizeof(selectedDevice));
	m_RecDevicesCtrl.GetLBText(index, selectedDevice);

	m_RecDevices = selectedDevice;

	if(!m_BASSOpen) {
		int ret = BASS_RecordInit(m_CurrentInputCard);
		m_BASSOpen = 1;
		opened = true;
	}

	for(int n = 0; name = (char *)BASS_RecordGetInputName(n); n++) {
		float vol = 0.0;
		int		s = BASS_RecordGetInput(n, &vol);
		CString description = name;

		if(m_RecDevices == description) {

			BASS_RecordSetInput(n, BASS_INPUT_ON, -1);
			m_CurrentInput = n;
			//m_RecVolume = LOWORD(s);
			m_RecVolume = (int)vol*100;
			wsprintf(msg, "Recording from %s", name);
			pWindow->generalStatusCallback((void *) msg, FILE_LINE);
		}
	}

	if(opened) {
		m_BASSOpen = 0;
		BASS_RecordFree();
	}

	UpdateData(FALSE);
}

void CMainWindow::DoSysCommand(UINT nID, LPARAM lParam) // BASE - slight diff for DSP
{
	/* Decide if minimize state changed */
	bool	bOldMin = bMinimized_;
	if(nID == SC_MINIMIZE)
	{
#ifndef SHUICASTSTANDALONE
		bMinimized_ = false;
#else
		bMinimized_ = true;
		SetupTrayIcon();
		ShowWindow(SW_HIDE);
		return;
#endif
	}
	else if(nID == SC_RESTORE) 
	{
		bMinimized_ = false;
		if(bOldMin != bMinimized_) 
		{
			/*
			 * Minimize state changed. Create the systray icon and do ;
			 * custom taskbar button handling.
			 */
			SetupTrayIcon();
			SetupTaskBarButton();
		}
	}

	CDialog::OnSysCommand(nID, lParam);
}

void CMainWindow::OnKeydownEncoders(NMHDR *pNMHDR, LRESULT *pResult)
{
	LV_KEYDOWN	*pLVKeyDow = (LV_KEYDOWN *) pNMHDR;

	if (pLVKeyDow->wVKey == 32) 
	{
		OnPopupConfigure();
	}
	else if (pLVKeyDow->wVKey == 46) 
	{
		OnPopupDelete();
	}
	else if (pLVKeyDow->wVKey == 93) 
	{
		int iItem = m_Encoders.GetNextItem(-1, LVNI_SELECTED);

		CMenu	menu;
		VERIFY(menu.LoadMenu(IDR_CONTEXT));

		/* Pop up sub menu 0 */
		CMenu	*popup = menu.GetSubMenu(0);

		if(popup) 
		{
			if(g[iItem]->weareconnected) 
			{
				popup->ModifyMenu(ID_POPUP_CONNECT, MF_BYCOMMAND, ID_POPUP_CONNECT, "Disconnect");
			}
			else 
			{
				if(g[iItem]->forcedDisconnect) 
				{
					popup->ModifyMenu(ID_POPUP_CONNECT, MF_BYCOMMAND, ID_POPUP_CONNECT, "Stop AutoConnect");
				}
				else 
				{
					popup->ModifyMenu(ID_POPUP_CONNECT, MF_BYCOMMAND, ID_POPUP_CONNECT, "Connect");
				}
			}

			RECT	pt;
			WINDOWPLACEMENT wp;
			WINDOWPLACEMENT wp2;
			GetWindowPlacement(&wp);
			m_Encoders.GetWindowPlacement(&wp2);

			//GetCursorPos(&pt);
			pt.bottom = wp.rcNormalPosition.bottom + wp2.rcNormalPosition.bottom;
			pt.left = wp.rcNormalPosition.left + wp2.rcNormalPosition.left;
			pt.right = wp.rcNormalPosition.right + wp2.rcNormalPosition.right;
			pt.top = wp.rcNormalPosition.top + wp2.rcNormalPosition.top;

			popup->TrackPopupMenu(TPM_LEFTALIGN, pt.left, pt.top, m_Encoders.GetActiveWindow());
		}
	}

	*pResult = 0;
}

void CMainWindow::OnButton1()
{
	OnPopupConfigure();
}

void CMainWindow::OnCancel()
{
	;
}

void CMainWindow::OnSetfocusEncoders(NMHDR *pNMHDR, LRESULT *pResult)
{
	int mark = m_Encoders.GetSelectionMark();

	if(mark == -1) 
	{
		if(m_Encoders.GetItemCount() > 0) 
		{
			m_Encoders.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED | LVIS_FOCUSED);
			m_Encoders.EnsureVisible(0, FALSE);
		}
	}
	else 
	{
		m_Encoders.SetCheck(m_Encoders.GetSelectionMark(), true);
	}

	*pResult = 0;
}

void CMainWindow::OnSelchangeReccards() 
{
	char	*name;
	char	selectedCard[1024] = "";
	bool	opened = false;
	int		index = m_RecCardsCtrl.GetCurSel();
	memset(selectedCard, '\000', sizeof(selectedCard));
	m_RecCardsCtrl.GetLBText(index, selectedCard);

	m_RecCards = selectedCard;

	setWindowsRecordingDevice(&gMain, selectedCard);


	BASS_DEVICEINFO info;

	for (int a=0; BASS_RecordGetDeviceInfo(a, &info); a++) {
		if (info.flags&BASS_DEVICE_ENABLED) {
			if (!strcmp(selectedCard, info.name)) {
				BASS_RecordSetDevice(a);
				m_CurrentInputCard = a;
			}
		}
		
	}


	if(m_BASSOpen) {
		m_BASSOpen = 0;
		BASS_RecordFree();
	}

	if(!m_BASSOpen) {
		int ret = BASS_RecordInit(m_CurrentInputCard);
		m_BASSOpen = 1;
		opened = true;
	}

	m_RecDevicesCtrl.ResetContent();

	for(int n = 0; name = (char *)BASS_RecordGetInputName(n); n++) {
		float vol = 0.0;
		int s = BASS_RecordGetInput(n, &vol);
		m_RecDevicesCtrl.AddString(name);
		if(s & BASS_INPUT_OFF) {
			;
		}
		else {
			m_RecDevices = name;
//			m_RecVolume = LOWORD(s);
			m_RecVolume = (int)vol*100;
			m_CurrentInput = n;
		}
	}

	if(m_BASSOpen) {
		m_BASSOpen = 0;
		BASS_RecordFree();
	}
	startRecording(m_CurrentInputCard);

	UpdateData(FALSE);

	
}

