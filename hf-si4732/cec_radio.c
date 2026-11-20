#include "driver/cec_si4735.h"
#include "driver/patch_ssb_compressed.h"    // Compressed SSB patch version (saving almost 1KB)

#include "ceccommon.h"
#include "ui/helper.h"
#include "ui/status.h"
#include "driver/eeprom.h"
#include "helper/battery.h"
#include "settings.h"

#include "bsp/dp32g030/pwmplus.h"
#include "bsp/dp32g030/portcon.h"
const uint16_t size_content = sizeof ssb_patch_content; // See ssb_patch_content.h
const uint16_t cmd_0x15_size = sizeof cmd_0x15;         // Array of lines where the 0x15 command occurs in the patch content.


#define FM_FUNCTION 0
#define AM_FUNCTION 1
#define RESET_PIN 12
// #define RESET_PIN 9


#define LSB 1
#define USB 2
#define MAX_BANDWIDTH_COUNT 6
#define DEFAULT_BANDWIDTH_INDEX 2
#define DEFAULT_BANDWIDTH_INDEX_CW 5

int currentBFO = 0;
uint8_t currentBFOStep = 10;


/*
SSB Audio Bandwidth:
0 = 1.2 kHz low-pass filter*1. (default)
1 = 2.2 kHz low-pass filter*1.
2 = 3.0 kHz low-pass filter.
3 = 4.0 kHz low-pass filter.
4 = 500 Hz band-pass filter for receiving CW signal, i.e. [250 Hz, 750 Hz]
with center frequency at 500 Hz when USB is selected or [-250 Hz, -750
Hz] with center frequency at -500Hz when LSB is selected*1.
5 = 1 kHz band-pass filter for receiving CW signal, i.e. [500 Hz, 1500 Hz]
with center frequency at 1 kHz when USB is selected or [-500 Hz, -1500
Hz] with center frequency at -1kHz when LSB is selected*1.
Other values = reserved.
Note:
1. If audio bandwidth selected is about 2 kHz or below, it is recommended to set SBCUTFLT[3:0] to 0 to enable the band pass filter for better high cut performance on the wanted side band. Otherwise, set it to 1.
*/

//SSB 14.074
//uint16_t currentRadioFreq = 14074;
uint8_t bandwidthIdx = DEFAULT_BANDWIDTH_INDEX;
uint8_t restoreBandwidthIdx;
uint8_t isAutoSave = 0;

//const char *bandwitdth[] = {"6", "4", "3", "2", "1", "1.8", "2.5"};
static const char bandwitdth[][4] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};

uint8_t currentAGCAtt = 0;


//============ SCREEN DESIGN (Version 0.1 ~ 0.2 )==================
// MODE Switch Change AM ->  LSB -> USB
// USB    7.075 KHz
// AGC ON      STEP:1   <-- Change when STEP Switch 
// BFO: 0Hz   St : 25
// BW : 3.0Khz  30  S:2 0%

//Switch BAND+, BAND-
//STEP , MODE, BW : 0.5, 1.0, 1.2, 2.2  3.0, 4.0 
//===============================================
const char modeName [5][4] = {"FM", "AM", "LSB", "USB", "CW"};	//CW IS USB AND SHIFT 700hz
#define MODE_FM  0
#define MODE_AM  1
#define MODE_LSB 2
#define MODE_USB 3
#define MODE_CW  4

uint8_t NowMode = 0;	//0:FM, 1:AM, 2:LSB, 3:USB, 4: CW (USB - 1KHz)
uint16_t CurrentFMFreq = 9310;
uint16_t CurrentAMFreq = 14074;
//uint16_t CurrentSSBFreq = 14074;
//uint8_t CurrentBandIndex = 0;
// step index per mode class: [0] FM (10kHz), [1] AM (10kHz), [2] LSB/USB/CW (1kHz)
static uint8_t freqStepIdx[3] = {1, 2, 0};
#define FMFreqStep	freqStepIdx[0]
#define AMFreqStep	freqStepIdx[1]
#define SSBFreqStep	freqStepIdx[2]
#define NowStepIdx	freqStepIdx[NowMode < 2 ? NowMode : 2]
bool disableAgc = true;



#define BAND_BCAST 0
#define BAND_HAM   1
/*
#define MOD_AM  0
#define MOD_LSB 1
#define MOD_USB 2
#define MOD_CW 3
*/

typedef struct {
  uint8_t   BandType;
  uint8_t   currentMode;  //0 : AM, 1:LSB, 2:USB
  char bandName[5];
} Band;

//23 Band (0.3HF Version)
/*
Band swband[] = {
  {BAND_HAM,    150,   471,   200,  1, MOD_LSB, 0, "L.F"},
  {BAND_HAM,    472,   479,   475,  1, MOD_LSB, 0, "630M"},
  {BAND_BCAST,  480,  1799,  1000,  1, MOD_AM,  0, "AMMF"},
  {BAND_HAM,   1800,  2000,  1820,  1, MOD_LSB, 0, "160M"},
  {BAND_BCAST, 2001,  3499,  2500,  1, MOD_AM,  0, "120M"},
  {BAND_HAM,   3500,  4000,  3800,  1, MOD_LSB, 0, "80M"},
  {BAND_BCAST, 4001,  5200,  4700,  1, MOD_AM,  0, "60M"},
  {BAND_BCAST, 5201,  6999,  5700,  1, MOD_AM,  0, "49M"},
  {BAND_HAM,   7000,  7300,  7070,  1, MOD_LSB, 0, "40M"},
  {BAND_BCAST, 7301,  9999,  7500,  1, MOD_AM,  0, "35M"},
  {BAND_HAM,  10000, 10500, 10299,  1, MOD_LSB, 0, "30M"},
  {BAND_BCAST,10501, 12999, 12499,  1, MOD_AM,  0, "25M"},
  {BAND_BCAST,13000, 13999, 13500,  1, MOD_AM,  0, "22M"},
  {BAND_HAM,  14000, 14499, 14050,  1, MOD_USB, 0, "20M"},
  {BAND_BCAST,14500, 16999, 15000,  1, MOD_AM,  0, "19M"},
  {BAND_BCAST,17000, 17999, 17500,  1, MOD_AM,  0, "18M"},
  {BAND_HAM,  18000, 18199, 18050,  1, MOD_USB, 0, "17M"},
  {BAND_BCAST,18200, 20999, 18500,  1, MOD_AM,  0, "16M"},
  {BAND_HAM,  21000, 21499, 21050,  1, MOD_USB, 0, "15M"},
  {BAND_BCAST,21500, 24799, 22500,  1, MOD_AM,  0, "13M"},
  {BAND_HAM,  24800, 25000, 24900,  1, MOD_USB, 0, "12M"},
  {BAND_HAM,  25001, 27999, 27125,  1, MOD_AM,  0, "11M"},
  {BAND_HAM,  28000, 30000, 28200,  1, MOD_USB, 0, "10M"},
};
*/

/*
//reduce 23 -> 20 for using FM Radio Frequency EEPROM
Band swband[] = {
  {BAND_HAM,    150,   471,   200,  1, MOD_LSB, 0, "L.F"},
  {BAND_HAM,    472,   479,   475,  1, MOD_LSB, 0, "630M"},
  {BAND_BCAST,  480,  1799,  1000,  1, MOD_AM,  0, "AMMF"},
  {BAND_HAM,   1800,  2000,  1820,  1, MOD_LSB, 0, "160M"},
  {BAND_BCAST, 2001,  3499,  2500,  1, MOD_AM,  0, "120M"},
  {BAND_HAM,   3500,  4000,  3800,  1, MOD_LSB, 0, "80M"},
//  {BAND_BCAST, 4001,  6999,  4700,  1, MOD_AM,  0, "60M"},
  {BAND_BCAST, 4001,  6999,  5500,  1, MOD_AM,  0, "60M"},
//  {BAND_BCAST, 5201,  6999,  5700,  1, MOD_AM,  0, "49M"},
  {BAND_HAM,   7000,  7300,  7070,  1, MOD_LSB, 0, "40M"},
  {BAND_BCAST, 7301,  9999,  7500,  1, MOD_AM,  0, "35M"},
  {BAND_HAM,  10000, 10500, 10299,  1, MOD_LSB, 0, "30M"},
  {BAND_BCAST,10501, 13999, 12499,  1, MOD_AM,  0, "25M"},
//  {BAND_BCAST,13000, 13999, 13500,  1, MOD_AM,  0, "22M"},
  {BAND_HAM,  14000, 14499, 14050,  1, MOD_USB, 0, "20M"},
  {BAND_BCAST,14500, 17999, 15000,  1, MOD_AM,  0, "19M"},
//  {BAND_BCAST,17000, 17999, 17500,  1, MOD_AM,  0, "18M"},
  {BAND_HAM,  18000, 18199, 18050,  1, MOD_USB, 0, "17M"},
  {BAND_BCAST,18200, 20999, 18500,  1, MOD_AM,  0, "16M"},
  {BAND_HAM,  21000, 21499, 21050,  1, MOD_USB, 0, "15M"},
  {BAND_BCAST,21500, 24799, 22500,  1, MOD_AM,  0, "13M"},
  {BAND_HAM,  24800, 25000, 24900,  1, MOD_USB, 0, "12M"},
  {BAND_HAM,  25001, 27999, 27125,  1, MOD_AM,  0, "11M"},
  {BAND_HAM,  28000, 30000, 28200,  1, MOD_USB, 0, "10M"},
};
*/

//reduce 23 -> 20 for using FM Radio Frequency EEPROM, Split Current Freq
// The 20 bands are contiguous, so only the lower edges are stored:
// band i covers bandEdge[i] .. bandEdge[i + 1] - 1 (150 .. 30000 kHz in total).
static const uint16_t bandEdge[21] = {
    150,   472,   480,  1800,  2001,  3500,  4001,  7000,  7301, 10000,
  10501, 14000, 14500, 18000, 18200, 21000, 21500, 24800, 25001, 28000,
  30001
};

const Band swband[] = {
  {BAND_HAM, MODE_LSB, "L.F"},		//   150 -   471
  {BAND_HAM, MODE_LSB, "630M"},		//   472 -   479
  {BAND_BCAST, MODE_AM, "AMMF"},	//   480 -  1799
  {BAND_HAM, MODE_LSB, "160M"},		//  1800 -  2000
  {BAND_BCAST, MODE_AM, "120M"},	//  2001 -  3499
  {BAND_HAM, MODE_LSB, "80M"},		//  3500 -  4000
//  {BAND_BCAST, 4001,  6999,  4700,  1, MOD_AM,  0, "60M"},
  {BAND_BCAST, MODE_AM, "60M"},		//  4001 -  6999
//  {BAND_BCAST, 5201,  6999,  5700,  1, MOD_AM,  0, "49M"},
  {BAND_HAM, MODE_LSB, "40M"},		//  7000 -  7300
  {BAND_BCAST, MODE_AM, "35M"},		//  7301 -  9999
  {BAND_HAM, MODE_LSB, "30M"},		// 10000 - 10500
  {BAND_BCAST, MODE_AM, "25M"},		// 10501 - 13999
//  {BAND_BCAST,13000, 13999, 13500,  1, MOD_AM,  0, "22M"},
  {BAND_HAM, MODE_USB, "20M"},		// 14000 - 14499
  {BAND_BCAST, MODE_AM, "19M"},		// 14500 - 17999
//  {BAND_BCAST,17000, 17999, 17500,  1, MOD_AM,  0, "18M"},
  {BAND_HAM, MODE_USB, "17M"},		// 18000 - 18199
  {BAND_BCAST, MODE_AM, "16M"},		// 18200 - 20999
  {BAND_HAM, MODE_USB, "15M"},		// 21000 - 21499
  {BAND_BCAST, MODE_AM, "13M"},		// 21500 - 24799
  {BAND_HAM, MODE_USB, "12M"},		// 24800 - 25000
  {BAND_HAM, MODE_AM, "11M"},		// 25001 - 27999
  {BAND_HAM, MODE_USB, "10M"},		// 28000 - 30000
};

uint16_t CurrentFreqs[20];

#define SI4735_RADIO_EEPROM 0x0F20
void LoadSaveConfig(bool isLoad);

int8_t CEC_FindBandIndex(uint16_t srcFreq)
{
    for (int i = 0; i < 20; i++)
    {
		if (srcFreq >= bandEdge[i] && srcFreq < bandEdge[i + 1])
			return i;
    }

	return -1;
}

void CEC_Radio_DrawRSSI(bool _isDraw)
{
	uint8_t tmpBuff[20];
	SI4735_getCurrentReceivedSignalQuality(0);
	memset(gFrameBuffer[3], 0, 128 * 2);		//2Line
/*
	uint8_t RSSI;    //!<  RESP4 - Contains the current receive signal strength (0–127 dBμV).
	uint8_t SNR;     //!<  RESP5 - Contains the current SNR metric (0–127 dB).
*/
	uint8_t displayRSSI = dBuVToSignalLength15(SI4735_getCurrentRSSI()) ;	//MAX 20

	//tmpBuff[0] = 'S';
	memset(tmpBuff, 0, sizeof(tmpBuff));
	sprintf(tmpBuff, "S%1u%c", displayRSSI < 10 ? displayRSSI : 9, displayRSSI < 10 ? ' ' : '+');
	for (int i = 0; i < 15; i++)
	{
		tmpBuff[i + 3] = displayRSSI > i ? '}' : '{';
	}

	UI_PrintStringSmallNormal(tmpBuff, 0, 0, 3);

	sprintf(tmpBuff, "RSSI %2ddBuV", SI4735_getCurrentRSSI());
	//sprintf(tmpBuff, "RSSI TEST TEST %d", 0);
	CEC_DisplaySmallest(tmpBuff, 1,  33, false, true);

	sprintf(tmpBuff, "SNR %2ddB", SI4735_getCurrentSNR());
	CEC_DisplaySmallest(tmpBuff, 95,  33, false, true);

	if (_isDraw)
	{
    	ST7565_BlitFullScreen();
	}
}

void CEC_Radio_DrawScreen(void)
{
	char tmpBuff[32];
//    uint16_t tmpFreq = 14074;
    uint16_t freqScaleDiv =  1000;
	uint8_t dispStep =  0;

	uint16_t tmpFreq = SI4735_getFrequency();
	gAskToSave	= false;
	if (NowMode == 0)	//FMSI47
	{
		CurrentFMFreq = tmpFreq;
		freqScaleDiv = 100;
	}
	else	//AM, LSB, USB, CW
		CurrentAMFreq = tmpFreq;
	dispStep = NowStepIdx;
	/*
	else if (NowMode == 1)	//AM
	{
		CurrentAMFreq = tmpFreq;
		dispStep = AMFreqStep;
	}
	else if (NowMode == 2) //LSB
	{
		CurrentAMFreq = tmpFreq;
		dispStep = SSBFreqStep;
	}
	else if (NowMode == 3) //USB
	{
		CurrentAMFreq = tmpFreq;
		dispStep = SSBFreqStep;
	}
	*/



	memset(gStatusLine, 0, sizeof(gStatusLine));
	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));	

    uint8_t     *line = gStatusLine;

	//sprintf(tmpBuff, "%u", tmpFreq);
	//sprintf(tmpBuff, NowMode == MODE_FM ? "%3u.%02u" : "%3u.%03u %u %u", tmpFreq / freqScaleDiv, (tmpFreq) % freqScaleDiv, tmpFreq, freqScaleDiv);
	//currentStep
	int nowBandIndex = CEC_FindBandIndex(tmpFreq);
	const char * bandName;
	const char * bandTypeName;
	if (NowMode == MODE_FM)
	{
		nowBandIndex = -1;
		bandName = "FM";
		bandTypeName = "BROAD";
	}
	//else if (nowBandIndex == -1)
	//{
	//	bandName = "";
	//	bandTypeName = "";
	//}
	else
	{
		bandName = swband[nowBandIndex].bandName;
		bandTypeName = swband[nowBandIndex].BandType == BAND_BCAST ? "BROAD" : "HAM";
	}

	uint8_t tmpLnaStr[10];
	if (!disableAgc)
	{
		sprintf(tmpLnaStr, "AGC ON");
	}
	else
	{
		sprintf(tmpLnaStr, "ATT %2d", currentAGCAtt);
	}

	sprintf(tmpBuff, "%-5s %4uKhz %s %3sK",bandTypeName,  NowMode == MODE_FM ? currentStep * 10 : currentStep, tmpLnaStr, bandwitdth[bandwidthIdx] );
    CEC_DisplaySmallest(tmpBuff, 3, 1, true, true);
    CEC_DisplaySmallest("HENE", 107, 1, true, true);
    CEC_ReverseScreen(gStatusLine, sizeof(gStatusLine));
    ST7565_BlitStatusLine();


	int startBandIndex = nowBandIndex - 3;
	if (nowBandIndex != -1)
	{
		if (startBandIndex < 0)
			startBandIndex = 0;
		else if (startBandIndex > 13)
			startBandIndex = 13;

		for (int i = 0; i < 7; i++)
		{
			CEC_DisplaySmallest(swband[startBandIndex + i].bandName, 2 + i * 18, 1, false, true);
		}


		uint8_t bandIndex = nowBandIndex - startBandIndex;
		uint8_t drawStartX = (18 * bandIndex);
		UI_DrawRectangleBuffer(gFrameBuffer, 0 + drawStartX , 6, 17 + drawStartX, 6, true);

	}

	sprintf(tmpBuff, NowMode == MODE_FM ? "%4u.%02u" : "%3u.%03u", tmpFreq / freqScaleDiv, (tmpFreq) % freqScaleDiv);
    UI_DisplayFrequency(tmpBuff, 20, 1, false);
	UI_PrintStringSmallNormal("Mhz", 105, 0, 2);
	UI_PrintStringSmallNormal(modeName[NowMode], 1, 0, 2);

	uint8_t drawStepUnderLineX = 46;
	/*
	if (dispStep == 0)
		drawStepUnderLineX = 88;
	if (dispStep == 1)
		drawStepUnderLineX = 74;
	else if (dispStep == 2)
		drawStepUnderLineX = 61;
		*/

	// x of the digit underline for step index 0..6 (1, 5, 10, 50, 100, 500, 1000)
	static const uint8_t stepUnderLineX[] = {88, 81, 74, 67, 61, 53, 46};
	if (dispStep < sizeof(stepUnderLineX))
		drawStepUnderLineX = stepUnderLineX[dispStep];


	//else
	//	drawStepUnderLineX = 30;

    UI_DrawRectangleBuffer(gFrameBuffer, drawStepUnderLineX , 22, drawStepUnderLineX + 13, 23, true);

	//With 13
	//dispStep

	if (NowMode >= MODE_LSB)
	{
		sprintf(tmpBuff, "%+04d", currentBFO);
		CEC_DisplaySmallest(tmpBuff, 105, 9, false, true);
	}

	CEC_Radio_DrawRSSI(0);
	//UI_PrintStringSmallNormal("S5}}}}}}{{{{{{{{{{", 0, 0, 3);
	//CEC_DisplaySmallest("RSSI 35dBuV", 3,  33, false, true);
	//CEC_DisplaySmallest("SNR 35", 95,  33, false, true);


    //UI_DrawRectangleBuffer(gFrameBuffer, 46 , 23, 59, 23, true);

 	//UI_PrintString("Mhz", 100, 0, 1, 7);


/*
	CEC_DisplaySmallest("MODE", 2,  36, false, true);
	CEC_DisplaySmallest("STEP+", 2, 42, false, true);
	CEC_DisplaySmallest("STEP-", 2, 48, false, true);
    UI_DrawRectangleBuffer(gFrameBuffer, 0 , 34, 23, 55, true);
*/
	// Key hints instead of the old BND/STP/LNA/BW/BFO function bar
	CEC_DisplaySmallest(NowMode >= MODE_LSB ? "UP/DN:TUNE SIDE:FINE F:MODE" : "UP/DN:TUNE SIDE:STEP F:MODE", 2, 42, false, true);
	CEC_DisplaySmallest("0-9:FREQ MENU:SETUP *:STEP", 2, 50, false, true);

    ST7565_BlitFullScreen();

	if (isAutoSave == 1)
	{
		LoadSaveConfig(false);	//SAVE
		//BackLightBlink(2);
		isAutoSave = 0;
	}

}

/*
//STEP
void AppllyStep(uint8_t _nowStep)
{
	currentStep = 1;
	for (int i = 0; i < _nowStep; i++)
		currentStep *= 10;
}
*/

void SetSI4735ChangeStep(uint8_t isIncrease)
{
	uint8_t nowStep = NowStepIdx;


	if (isIncrease == 1)	//Increase
	{
		if (++nowStep > 6)
			nowStep = 0;
	}
	else if (isIncrease == 0)	//Decrease
	{
		if (nowStep == 0)
			nowStep = 6;
		else 
			nowStep--;
	}

	//other skip
	static const uint16_t lstStep[] = {1, 5, 10, 50, 100, 500, 1000};
	//SI4735_setFrequencyStep(nowStep);
	//AppllyStep(nowStep);
	//currentStep = 1;
	//for (int i = 0; i < nowStep; i++)
	//	currentStep *= 10;
	currentStep = lstStep[nowStep];

	uint16_t tmpFreq = SI4735_getFrequency();

	NowStepIdx = nowStep;
	if (NowMode == 0)
		CurrentFMFreq = tmpFreq;
	else	//AM, SSB
		CurrentAMFreq = tmpFreq;


	//if (NowMode)
	/*
	uint16_t tmpFreq = SI4735_getFrequency();
	tmpFreq = (tmpFreq / nowStep) * nowStep;

	setFrequency(tmpFreq);
	*/

}

#define AMBAND_FREQ_LOW  150
#define AMBAND_FREQ_HIGH 30000



void SendMixSignal()
{

	return;
  	gCurrentVfo->pTX->Frequency = 2510000;

	RADIO_PrepareTX();
  	BK4819_PrepareTransmit();

	BK4819_WriteRegister(0x40, 0);
	BK4819_SetupPowerAmplifier(0, 0);
	BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);

	const uint8_t gain   = (3u << 3) | (1u << 0);
	const uint8_t enable = 1;

	uint32_t startFreq = gRxVfo->pRX->Frequency;
	BK4819_WriteRegister(BK4819_REG_36, (100 << 8) | (enable << 7) | (gain << 0));

	GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BK1080);
	//while(1);
	BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
	AUDIO_AudioPathOn();

	//BK4819_SetFrequency(startFreq);
	//잘된다.
	/*
	BK4819_WriteRegister(BK4819_REG_38, (startFreq >>  0) & 0xFFFF);
	BK4819_WriteRegister(BK4819_REG_39, (startFreq >> 16) & 0xFFFF);

	uint16_t tmp1 = 0xC1FE;
	tmp1 &= ~(1 << 15);	//VCO Calibration Enable  (이것만 해줘도 주파수 변경됨) - 0xC1FE로 다시 전송한경우 주파수 변경됨
	//tmp1 &= ~(0xF << 4);	//드드드 소리큼
	BK4819_WriteRegister(BK4819_REG_30, tmp1);
	BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
	*/
}

void WaitMomentDisplay()
{
	memset(gFrameBuffer[1] + 40, 0, 50);
	memset(gFrameBuffer[2] + 40, 0, 50);
				
	UI_PrintString("Wait", 50, 0, 1, 8);
	CEC_ReverseScreen(gFrameBuffer[1] + 40, 50);
	CEC_ReverseScreen(gFrameBuffer[2] + 40, 50);
	ST7565_BlitFullScreen();
}


// Sends only the BFO offset (LSB/USB/CW); used after fine tuning and frequency entry
static void SetSSBBfo(void)
{
	if (NowMode >= 2)
		SI4735_setSSBBfo2(-currentBFO + (NowMode == 4 ? (CW_Tone * -10) : 0) );
}

void SetSSBConfig()
{
	if (NowMode != MODE_FM)		// AM/SSB property; the FM firmware rejects it
		SI4735_setAvcAmMaxGain(90); // Sets the maximum gain for automatic volume control on AM/SSB mode (from 12 to 90dB)
	SI4735_setAutomaticGainControl(disableAgc, currentAGCAtt);	//AGC / ATT in every mode, not only SSB
	if (NowMode < 2)
		return;
	SetSSBBfo();
	// The antenna capacitor (ANTCAPL = 1) is already set at power up and every
	// SI4735_setFrequency sends it, so no extra re-tune is needed here.

	//bandwidthIdx
	//1. If audio bandwidth selected is about 2 kHz or below, it is recommended to set SBCUTFLT[3:0] to 0 to enable the band pass filter for better high cut performance on the wanted side band. Otherwise, set it to 1.
	// AUDIOBW and SBCUTFLT go out in one SSB_MODE write (was two)
	currentSSBMode.param.AUDIOBW = bandwidthIdx;
	currentSSBMode.param.SBCUTFLT = (bandwidthIdx == 0 || bandwidthIdx == 4 || bandwidthIdx == 5) ? 0 : 1;
	sendSSBModeProperty();
}

void SetSI4735Mode(uint8_t newMode)
{
	//SendMixSignal();

	if (newMode == 0)	//FM
		SI4735_setFM(7000, 10800,  CurrentFMFreq, FMFreqStep);
	else if (newMode == 1)	//AM
		SI4735_setAM(AMBAND_FREQ_LOW, AMBAND_FREQ_HIGH,  CurrentAMFreq, AMFreqStep);
	else if (newMode == 2 || newMode == 3 || newMode == 4)	//LSB, USB, CW
	{
		if (NowMode < 2)
		{
			//2024.04.06
			WaitMomentDisplay();
			//end of 2024.04.06

			//XOSCEN_CRYSTAL
			//setRefClock(32570);
			//setRefClockPrescaler(1, 0);
			SI4735_setup2(0, AM_FUNCTION);
			//SI4735_setup1(0, -1, AM_FUNCTION, SI473X_ANALOG_AUDIO, XOSCEN_RCLK, 0); // XOSCEN_RCLK means: external clock source setup
			//SI4735_setup1(resetPin, 0, defaultFunction, SI473X_ANALOG_AUDIO, XOSCEN_CRYSTAL, 0);

			delay(10);
			SI4735_loadCompressedPatch(ssb_patch_content, size_content, cmd_0x15, cmd_0x15_size, 1);
			delay(10);

		}

		//setRefClock(32570);
		//setRefClockPrescaler(1, 0);
		SI4735_setSSB(AMBAND_FREQ_LOW, AMBAND_FREQ_HIGH, CurrentAMFreq, SSBFreqStep, newMode == 3 ? 2 : 1);	//USB, LSB(CW)

		//if ()
		//currentRadioFreq = SI4735_getFrequency();
	}

/*
SI4735_setAM2
        //SI4735_setAvcAmMaxGain(currentAvcAmMaxGain); // Set AM Automatic Volume Gain (default value is DEFAULT_CURRENT_AVC_AM_MAX_GAIN)
        //SI4735_setVolume(volume);                    // Set to previus configured volume
*/

	if (NowMode != MODE_CW && newMode == MODE_CW)		//OTHER MODE TO CW (Save Bandwidth)
	{
		restoreBandwidthIdx = bandwidthIdx;
		bandwidthIdx = DEFAULT_BANDWIDTH_INDEX_CW;
	}
	else if (NowMode == MODE_CW && newMode != MODE_CW)	//CW TO OTHER MODE (Restore Bandwidth)
	{
		bandwidthIdx = restoreBandwidthIdx;
	}
	//AppllyStep(currentStep);	//Change currnetStep
	NowMode = newMode;
	//SI4735_setVolume(63);
	currentSSBMode.param.AVCEN = 0;	// AVC off; sent with the SSB_MODE write in SetSSBConfig (SSB modes only)

	SetSSBConfig();
	SetSI4735ChangeStep(2);
	SI4735_setVolume(63);
}

KEY_Code_t GetKeyValue()
{
	KEY_Code_t rst = KEYBOARD_Poll();
	if (rst == KEY_INVALID)
		return KEY_INVALID;

	delay(250);
	return rst;
}


uint8_t GetKeyPressTime(uint8_t maxDelayTime)
{
	uint8_t i = 0;
	for (i = 0; i < maxDelayTime; i++)
	{
		SYSTEM_DelayMs(10);
		if (KEYBOARD_Poll() == KEY_INVALID)
			return i;
	}

	return i;
}


void WaitKeyUp()
{
	while (KEYBOARD_Poll() != KEY_INVALID)
	{
		delay(10);
	}
}

static uint8_t hfSavedMode = 1;	//MODE_AM, mode to start in after the SI4732 init

void LoadSaveConfig(bool isLoad)
{
	//STEP
	struct
	{
		uint16_t fmFreq;
		uint16_t amFreq;
		uint8_t  agcInfo;	//0 : agc on/off,  7~1 : AGC Value
		uint8_t  bandwidth;	//0~10
		uint8_t amStep;
		uint8_t ssbStep;
	} __attribute__((packed)) SI4735_ROM1;

	struct
	{
		uint16_t xtalAdjVal;	//0, 1
		uint8_t temp1_1;			//2
		uint8_t temp1_2;			//3
		uint8_t  temp2;			//4
		uint8_t  temp3;			//5
		uint8_t temp4;			//6
		uint8_t temp5;			//7
	} __attribute__((packed)) SI4735_ROM2;

	if (isLoad)
	{
		//Load Stored Frequency 
        EEPROM_ReadBuffer(0x0E40, CurrentFreqs, sizeof(CurrentFreqs));
        for (int i = 0; i < 20; i++)
        {
            if (CurrentFreqs[i] >= bandEdge[i + 1] || CurrentFreqs[i] < bandEdge[i])
                CurrentFreqs[i] = bandEdge[i] + ((bandEdge[i + 1] - 1 - bandEdge[i]) / 2);
        }

		//Load Setting values
		EEPROM_ReadBuffer(SI4735_RADIO_EEPROM, &SI4735_ROM1, 8);
		//AMBAND_FREQ_LOW, AMBAND_FREQ_HIGH
		//gEeprom.FM_LowerLimit = 700;
		//gEeprom.FM_UpperLimit = 1080;
		if (SI4735_ROM1.fmFreq < 7000 || SI4735_ROM1.fmFreq > 10800)
			CurrentFMFreq = 9700;
		else
			CurrentFMFreq = SI4735_ROM1.fmFreq;

		if (SI4735_ROM1.amFreq < AMBAND_FREQ_LOW || SI4735_ROM1.amFreq > AMBAND_FREQ_HIGH)
			CurrentAMFreq = 21000;
		else
			CurrentAMFreq = SI4735_ROM1.amFreq;

		//bandwidth byte : low nibble SSB bandwidth, high nibble last mode + 1 (0 on older images = AM)
		bandwidthIdx =   (SI4735_ROM1.bandwidth & 0x0F) < MAX_BANDWIDTH_COUNT ? (SI4735_ROM1.bandwidth & 0x0F) : DEFAULT_BANDWIDTH_INDEX;
		hfSavedMode = (SI4735_ROM1.bandwidth >> 4) - 1;
		if (hfSavedMode > MODE_CW)
			hfSavedMode = MODE_AM;

		disableAgc = SI4735_ROM1.agcInfo & 0x01;
		currentAGCAtt = (SI4735_ROM1.agcInfo >> 1) & 0x7F;
		if (currentAGCAtt > 30)
		{
			currentAGCAtt = 0;
			disableAgc = 1;
		}

		AMFreqStep = SI4735_ROM1.amStep < 7 ? SI4735_ROM1.amStep : 2;		//10Khz
		SSBFreqStep = SI4735_ROM1.ssbStep < 7 ? SI4735_ROM1.ssbStep : 0;	//1Khz
	}
	else
	{
		//AMBAND_FREQ_LOW, AMBAND_FREQ_HIGH
		//gEeprom.FM_LowerLimit = 700;
		//gEeprom.FM_UpperLimit = 1080;
        //Save Freq
        for (int i = 0; i < 5; i++)
            EEPROM_WriteBuffer(0x0E40 + (i * 8), &CurrentFreqs[i * 4]);     

		SI4735_ROM1.fmFreq = CurrentFMFreq;
		SI4735_ROM1.amFreq = CurrentAMFreq;
		SI4735_ROM1.bandwidth = (NowMode == MODE_CW ? restoreBandwidthIdx : bandwidthIdx) | ((NowMode + 1) << 4);
		SI4735_ROM1.agcInfo = (currentAGCAtt << 1) | (disableAgc & 0x01);
		SI4735_ROM1.amStep = AMFreqStep;
		SI4735_ROM1.ssbStep = SSBFreqStep;

		EEPROM_WriteBuffer(SI4735_RADIO_EEPROM, &SI4735_ROM1);
	}
}

//===================== MENU DRIVEN HF UI =====================
// Main screen : 0-9 frequency entry, UP/DOWN tune, SIDE fine tune (SSB) or step (AM/FM),
//               F mode, * step, MENU setup, EXIT back to VHF/UHF
// Setup menu  : UP/DOWN select, MENU edit (UP/DOWN or SIDE change the value live),
//               MENU/EXIT done, 1-9 jump to item, EXIT close
enum {
	HFM_MODE = 0,
	HFM_BAND,
	HFM_STEP,
	HFM_AGC,
	HFM_BW,
	HFM_FINE,
	HFM_BFO,
	HFM_BOOT,
	HFM_EXIT,
	HFM_COUNT
};

static const char hfMenuName[HFM_COUNT][8] = {"Mode", "Band", "Step", "AGC", "SSB BW", "FineStp", "BFO", "Boot HF", "Exit HF"};
static const uint8_t agcAttList[] = {0, 1, 5, 15, 26};	//index 0 : AGC ON, 1~ : AGC OFF + ATT
static const uint8_t fineStepList[] = {10, 25, 50, 100};
static uint8_t hfMenuCursor = 0;

static uint8_t HF_Cycle(uint8_t value, uint8_t count, int8_t dir)
{
	return (value + count + dir) % count;
}

static uint8_t HF_GetAgcIndex(void)
{
	if (!disableAgc)
		return 0;

	for (uint8_t i = 0; i < sizeof(agcAttList); i++)
		if (agcAttList[i] == currentAGCAtt)
			return i + 1;

	return 1;
}

static uint8_t HF_GetFineIndex(void)
{
	for (uint8_t i = 0; i < sizeof(fineStepList); i++)
		if (fineStepList[i] == currentBFOStep)
			return i;

	return 0;
}

// SSB/CW fine tune with the BFO, folded into the dial frequency every kHz
static void HF_FineTune(int8_t dir)
{
	currentBFO += dir * currentBFOStep;
	if (currentBFO > 500 && currentWorkFrequency < AMBAND_FREQ_HIGH)
	{
		currentBFO -= 1000;
		SI4735_setFrequency(currentWorkFrequency + 1);
	}
	else if (currentBFO < -500 && currentWorkFrequency > AMBAND_FREQ_LOW)
	{
		currentBFO += 1000;
		SI4735_setFrequency(currentWorkFrequency - 1);
	}
}

static void HF_ChangeBand(int8_t dir)
{
	int8_t bandIndex = NowMode == MODE_FM ? -1 : CEC_FindBandIndex(currentWorkFrequency);

	if (bandIndex < 0)
		bandIndex = 0;
	else
	{
		CurrentFreqs[bandIndex] = currentWorkFrequency;
		bandIndex = HF_Cycle(bandIndex, 20, dir);
	}

	CurrentAMFreq = CurrentFreqs[bandIndex];
	if (NowMode == MODE_FM)	//FM can not tune HF, use the band mode now
		SetSI4735Mode(swband[bandIndex].currentMode);

	SI4735_setFrequency(CurrentAMFreq);
	currentBFO = 0;
	isAutoSave = 1;
}

// Applied when band editing is done, so scrolling bands does not reload the SSB patch every step
static void HF_ApplyBandMode(void)
{
	int8_t bandIndex = CEC_FindBandIndex(currentWorkFrequency);

	CurrentAMFreq = currentWorkFrequency;
	if (NowMode != MODE_FM && bandIndex >= 0 && swband[bandIndex].currentMode != NowMode)
		SetSI4735Mode(swband[bandIndex].currentMode);
}

static void HF_MenuValue(uint8_t item, char *buf)
{
	buf[0] = 0;
	switch (item)
	{
		case HFM_MODE:
			strcpy(buf, modeName[NowMode]);
			break;
		case HFM_BAND:
			strcpy(buf, NowMode == MODE_FM ? "FM" : swband[CEC_FindBandIndex(currentWorkFrequency)].bandName);
			break;
		case HFM_STEP:
			sprintf(buf, "%uK", NowMode == MODE_FM ? currentStep * 10 : currentStep);
			break;
		case HFM_AGC:
			if (disableAgc)
				sprintf(buf, "ATT %u", currentAGCAtt);
			else
				strcpy(buf, "AUTO");
			break;
		case HFM_BW:
			sprintf(buf, "%sK", bandwitdth[bandwidthIdx]);
			break;
		case HFM_FINE:
			sprintf(buf, "%uHz", currentBFOStep);
			break;
		case HFM_BFO:
			sprintf(buf, "%+dHz", currentBFO);
			break;
		case HFM_BOOT:
			strcpy(buf, HF_BootMode ? "ON" : "OFF");
			break;
	}
}

static void HF_MenuChange(uint8_t item, int8_t dir)
{
	uint8_t agcIndex;

	switch (item)
	{
		case HFM_MODE:
			if (NowMode != MODE_FM)
				CurrentAMFreq = currentWorkFrequency;	//keep the fine tune fold
			SetSI4735Mode(HF_Cycle(NowMode, 5, dir));
			break;
		case HFM_BAND:
			HF_ChangeBand(dir);
			break;
		case HFM_STEP:
			SetSI4735ChangeStep(dir > 0);
			break;
		case HFM_AGC:
			agcIndex = HF_Cycle(HF_GetAgcIndex(), sizeof(agcAttList) + 1, dir);
			disableAgc = agcIndex != 0;
			currentAGCAtt = agcIndex ? agcAttList[agcIndex - 1] : 0;
			break;
		case HFM_BW:
			bandwidthIdx = HF_Cycle(bandwidthIdx, MAX_BANDWIDTH_COUNT, dir);
			break;
		case HFM_FINE:
			currentBFOStep = fineStepList[HF_Cycle(HF_GetFineIndex(), sizeof(fineStepList), dir)];
			break;
		case HFM_BFO:
			HF_FineTune(dir);
			break;
		case HFM_BOOT:
			HF_BootMode = !HF_BootMode;
			break;
	}

	SetSSBConfig();
}

static void HF_DrawMenu(bool editing)
{
	char buf[12];
	uint8_t topItem = hfMenuCursor < 7 ? 0 : hfMenuCursor - 6;

	memset(gStatusLine, 0, sizeof(gStatusLine));
	memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

	CEC_DisplaySmallest(editing ? "HF SETUP  UP/DN:CHANGE  MENU:OK" : "HF SETUP  MENU:EDIT  EXIT:CLOSE", 2, 1, true, true);
	CEC_ReverseScreen(gStatusLine, sizeof(gStatusLine));

	for (uint8_t i = 0; i < 7 && topItem + i < HFM_COUNT; i++)
	{
		uint8_t item = topItem + i;

		sprintf(buf, "%u %s", item + 1, hfMenuName[item]);
		UI_PrintStringSmallNormal(buf, 0, 0, i);

		HF_MenuValue(item, buf);
		UI_PrintStringSmallNormal(buf, 127 - strlen(buf) * 7, 0, i);

		if (item == hfMenuCursor)
		{
			if (editing)
				CEC_ReverseScreen(gFrameBuffer[i] + 72, 56);
			else
				CEC_ReverseScreen(gFrameBuffer[i], 128);
		}
	}

	ST7565_BlitStatusLine();
	ST7565_BlitFullScreen();
}

// Returns true when "Exit HF" was selected
static bool HF_SetupMenu(void)
{
	bool editing = false;
	bool bandChanged = false;
	bool needKeyUp = true;
	uint8_t oldBootMode = HF_BootMode;
	bool exitHF = false;

	while (1)
	{
		HF_DrawMenu(editing);

		if (needKeyUp)
			WaitKeyUp();

		KEY_Code_t key;
		while ((key = GetKeyValue()) == KEY_INVALID)
			delay(10);

		BACKLIGHT_TurnOn();
		needKeyUp = true;

		if (key == KEY_UP || key == KEY_DOWN || key == KEY_SIDE1 || key == KEY_SIDE2)
		{
			int8_t dir = (key == KEY_UP || key == KEY_SIDE1) ? 1 : -1;

			needKeyUp = false;	//hold to repeat
			if (editing)
			{
				HF_MenuChange(hfMenuCursor, dir);
				bandChanged |= hfMenuCursor == HFM_BAND;
			}
			else
				hfMenuCursor = HF_Cycle(hfMenuCursor, HFM_COUNT, -dir);
		}
		else if (key == KEY_MENU || key == KEY_EXIT)
		{
			if (key == KEY_MENU && hfMenuCursor == HFM_EXIT)
			{
				exitHF = true;
				break;
			}

			if (!editing && key == KEY_EXIT)
				break;

			if (bandChanged)
				HF_ApplyBandMode();
			bandChanged = false;
			editing = !editing;
		}
		else if (!editing && key >= KEY_1 && key <= KEY_9)
			hfMenuCursor = key - KEY_1;
	}

	if (oldBootMode != HF_BootMode)
		SETTINGS_SaveSettings();

	isAutoSave = 1;
	return exitHF;
}

// Direct frequency entry, started with the first digit already counted.
// Type kHz (7074) or MHz with * as the decimal point (7*074). Short numbers are MHz (7, 14, 93).
// MENU accepts, EXIT deletes the last digit (cancel when empty). FM broadcast frequencies switch to FM.
static void HF_FrequencyEntry(KEY_Code_t key)
{
	char input[8];
	uint8_t len = 0;
	int8_t dotPos = -1;
	uint32_t lastKeyTime = millis10();

	while (1)
	{
		if (key != KEY_INVALID)
		{
			if (key <= KEY_9 && len < 7)
				input[len++] = '0' + key;
			else if (key == KEY_STAR && dotPos < 0 && len < 7)
			{
				dotPos = len;
				input[len++] = '.';
			}
			else if (key == KEY_EXIT)
			{
				if (--len == dotPos)
					dotPos = -1;
				if (len == 0)
					return;
			}
			else if (key == KEY_MENU)
				break;

			input[len] = 0;

			//accept when no more digits are possible
			if (dotPos < 0 ? len >= 5 : len - dotPos > 3)
				break;

			memset(gFrameBuffer[1], 0, 104);
			memset(gFrameBuffer[2], 0, 104);
			memset(gFrameBuffer[5], 0, 128 * 2);
			UI_DisplayFrequency(input, 20, 1, false);
			CEC_DisplaySmallest("MENU:OK  EXIT:DEL  *:DOT", 2, 42, false, true);
			CEC_DisplaySmallest("KHZ 7074 OR MHZ 7*074", 2, 50, false, true);
			ST7565_BlitFullScreen();

			WaitKeyUp();
			lastKeyTime = millis10();
		}

		if (millis10() - lastKeyTime > 1000)	//10 sec without key : cancel
			return;

		delay(20);
		key = KEYBOARD_Poll();
	}

	uint32_t freq = 0;	//kHz
	uint8_t fracDigits = 0;
	for (uint8_t i = 0; i < len; i++)
	{
		if (input[i] == '.')
			continue;
		freq = freq * 10 + input[i] - '0';
		if (dotPos >= 0 && i > dotPos)
			fracDigits++;
	}

	if (dotPos >= 0)
	{
		while (fracDigits++ < 3)
			freq *= 10;
	}
	else if (freq <= 30 || (freq >= 64 && freq <= 108))
		freq *= 1000;

	if (freq >= 70000 && freq <= 108000)
	{
		CurrentFMFreq = freq / 10;
		if (NowMode != MODE_FM)
			SetSI4735Mode(MODE_FM);
		SI4735_setFrequency(CurrentFMFreq);
	}
	else if (freq >= AMBAND_FREQ_LOW && freq <= AMBAND_FREQ_HIGH)
	{
		CurrentAMFreq = freq;
		if (NowMode == MODE_FM)
			SetSI4735Mode(swband[CEC_FindBandIndex(freq)].currentMode);
		SI4735_setFrequency(freq);
		currentBFO = 0;
	}

	isAutoSave = 1;
}

bool isNeed4732Init = true;
void CEC_Radio_Work(void)
{
    //SI4735Test_SSB();
    //DRAW MAIN SCREEN 
	uint16_t refreshCount = 0;

	WaitMomentDisplay();

	RADIO_SetupRegisters(true);
	GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BK1080);
	delay(100);	

	if (isNeed4732Init)
	{
		//Generat32Khz();
		LoadSaveConfig(true);
		gMonitor = false;
		//SI4735_setDeviceI2CAddress(0);
		int16_t si4735Addr = 0x11;

		//for External 32Khz X-Tal  (Default)
		//
		//setRefClock(32500);
		//setRefClockPrescaler(1, 0);
		SI4735_setup2(0, FM_FUNCTION);

/*
		//Belore Test Working with Signal Generator amp 1volt, bias 1.5 volt
		//* @param refclk The allowed REFCLK frequency range is between 31130 and 34406 Hz (32768 ±5%), or 0 (to disable AFC). : 32500 = OK
		setRefClock(32500);          // Ref = 32.5kHz

		//@param prescale  Prescaler for Reference Clock value; Between 1 and 4095 in 1 unit steps. Default is 1. : 800  = OK
		//@param rclk_sel  0 = RCLK pin is clock source (default); 1 = DCLK pin is clock source : 0 : Connect clock with RCLK pin  = OK
		setRefClockPrescaler(800, 0);   // prescaler = 800 ==> 32500 x 800 = 26000000 (26MHz)
		//rx.setup(RESET_PIN, 0, POWER_UP_AM, SI473X_ANALOG_AUDIO, XOSCEN_RCLK);
		//NOT USE RESET PIN, INT 0, 

		SI4735_setup1(0, 0, FM_FUNCTION, SI473X_ANALOG_AUDIO, XOSCEN_RCLK, 0);
*/

/*
	//USING PWM
		setRefClock(32120);          // Ref = 32.5kHz
		setRefClockPrescaler(0, 0);   // prescaler = 800 ==> 32500 x 800 = 26000000 (26MHz)
		SI4735_setup1(0, 0, FM_FUNCTION, SI473X_ANALOG_AUDIO, XOSCEN_RCLK, 0);
*/

/*
		//delay(1000);
		delay(100);
		//SI4735_setVolume(0);
		SetSI4735Mode(MODE_FM);
		delay(100);
		SetSI4735Mode(MODE_AM);
		delay(70);
*/		
	}

	if ((! HF_DualWatch) || isNeed4732Init)
	{
		delay(100);
		//SI4735_setVolume(0);
		SetSI4735Mode(MODE_FM);
		delay(300);
		SetSI4735Mode(MODE_AM);
		delay(100);
		if (hfSavedMode != MODE_AM)	//back to the mode used last time
			SetSI4735Mode(hfSavedMode);
	}


	//SI4735_setVolume(63);
	//setSSBAudioBandwidth(bandwidthIdx);
	AUDIO_AudioPathOn();
	SI4735_setVolume(63);
	
	isNeed4732Init = true;	//using 2 way
    gAskToSave = true;  //use need redrawScreen
	bool isNeedKeyUpCheck = true;
    while (1)
    {
        if (gAskToSave)
            CEC_Radio_DrawScreen();

		if (isNeed4732Init)
		{
			isNeed4732Init = false;
		}

		if (isNeedKeyUpCheck)
		{
			isNeedKeyUpCheck = false;
			WaitKeyUp();
		}

        //Check Buttons
		KEY_Code_t chkKey = GetKeyValue();
		if (chkKey != KEY_INVALID)
		{
			BACKLIGHT_TurnOn();
			isNeedKeyUpCheck = true;

			if (chkKey <= KEY_9)
				HF_FrequencyEntry(chkKey);
			else if (chkKey == KEY_MENU)
			{
				if (HF_SetupMenu())
					chkKey = KEY_EXIT;
			}
			else if (chkKey == KEY_F)	//AM -> LSB -> USB -> CW -> AM, FM -> AM
				SetSI4735Mode(NowMode == MODE_FM || NowMode == MODE_CW ? MODE_AM : NowMode + 1);
			else if (chkKey == KEY_STAR)
				SetSI4735ChangeStep(1);
			else if (chkKey == KEY_UP || chkKey == KEY_DOWN || chkKey == KEY_SIDE1 || chkKey == KEY_SIDE2)
			{
				int8_t dir = (chkKey == KEY_UP || chkKey == KEY_SIDE1) ? 1 : -1;

				isNeedKeyUpCheck = false;	//hold to repeat
				if (chkKey == KEY_UP)
					SI4735_frequencyUp();
				else if (chkKey == KEY_DOWN)
					SI4735_frequencyDown();
				else if (NowMode >= MODE_LSB)
					HF_FineTune(dir);
				else
				{
					SetSI4735ChangeStep(dir > 0);
					isNeedKeyUpCheck = true;
				}
			}

			if (chkKey == KEY_EXIT)
			{
				SI4735_setVolume(HF_DualWatch ? HF_DualVol : 0);

				if (! HF_DualWatch)
				{
					GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BK1080);	// holds the SI4732 in reset
					AUDIO_AudioPathOff();
					// The chip loses its power-up state and SSB patch while held in reset, so the
					// next entry must initialise it again even if HF dual watch is enabled meanwhile.
					// This also keeps radio.c/audio.h/menu.c from treating it as running.
					isNeed4732Init = true;
				}
				LoadSaveConfig(false);	//SAVE
				WaitKeyUp();
				gRequestDisplayScreen = DISPLAY_MAIN;	//redraw the normal radio screen
				gUpdateStatus = true;
				return;
			}

			// Mode and menu changes configure the chip themselves; only fine tuning
			// and frequency entry (BFO reset) leave a new BFO offset to send
			if (chkKey <= KEY_9 || chkKey == KEY_SIDE1 || chkKey == KEY_SIDE2)
				SetSSBBfo();
			refreshCount = 0;
			gAskToSave = true;	//Redraw
		}	//end of key invalid check

		if (++refreshCount > 3000)
		{
			//BACKLIGHT_TurnOff();
			refreshCount = 0;
			gAskToSave = true;	//Redraw
		}

		/*
		if ((refreshCount % 100) == 0)
		{
			CEC_Radio_DrawRSSI(1);
		}
		*/
		if ((refreshCount % 50) == 0)
		{
			if (gBacklightCountdown_500ms > 0 && --gBacklightCountdown_500ms == 0)
			{
				BACKLIGHT_TurnOff();
			}
		}
		delay(10);
    }
}