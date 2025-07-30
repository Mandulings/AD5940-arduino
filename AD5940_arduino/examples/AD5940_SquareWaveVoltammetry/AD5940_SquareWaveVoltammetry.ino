#define CHIPSEL_594X      /**< AD5940 or AD5941 */

#include <SPI.h>
#include "AD5940_arduino.h"
#include <ArduinoBLE.h>

#define APPBUFF_SIZE 1024 /* 4 kB SRAM for FIFO */
uint32_t AppBuff[APPBUFF_SIZE]; /* Array to store ADC current data */
float RampDACArray[APPBUFF_SIZE]; /* Array to store DAC voltage data */

uint32_t DataCount; /* # of measured FIFO data*/

float LFOSCFreq;   /* Measured LFOSC frequency */

int INT0_PIN; /* PIN for MCU to get interrupt from AD5940 */

uint32_t StepNumber;
float RampFrequency;

/* BLE packets */
const int PACKET_SIZE = 5;  // 5 data per packet
byte packet_voltage[4 * PACKET_SIZE];    // data = 4 byte, packet = 20 byte
byte packet_current[4 * PACKET_SIZE];
byte packet_initial[4 * PACKET_SIZE] = {0};
byte RampFrequencyBytes[4];

/* BLE */
BLEDevice central;
BLEService SWVService("ecce822d-9357-480a-9c74-4e5e8322f3c7");
/* Transmit 4 characteristics: DAC ramp voltage, ADC responsive current, Ramp frequency, and Endflag*/
BLECharacteristic VoltageChar("bcdef012-1234-5678-1234-56789abcdef0", BLERead | BLENotify, sizeof(packet_voltage));
BLECharacteristic CurrentChar("f968b639-9ec8-4d21-830a-1d0eaf49e76c", BLERead | BLENotify, sizeof(packet_current));
BLECharacteristic RampFrequencyChar("8c5eee30-4cf2-46e8-a614-3eccc19ef130", BLERead | BLENotify, sizeof(RampFrequencyBytes));
BLECharacteristic EndFlagChar("dbe10000-1234-5678-1234-56789abcdef0", BLERead | BLENotify, 1);

void setup() {
  INT0_PIN = 2;
  CS_PIN = 10;
  RESET_PIN = 9;
  SPI_CLOCK = 1600000;
  
  Serial.begin(115200);
  delay(1000);

  /* PIN */
  pinMode(INT0_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INT0_PIN), INT0_ISR, FALLING);

  pinMode(CS_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(RESET_PIN, HIGH);


  AD5940_HWReset(); /* Reset before SPI begin */
  
  SPI.begin();
  SPI.beginTransaction(SPISettings(SPI_CLOCK, MSBFIRST, SPI_MODE0));

  AD5940_Initialize(); /* This function is used to put AD5940 to correct state */
  AD5940PlatformCfg(); /* The general configuration to AD5940 like FIFO/Sequencer/Clock. */
  AD5940FrequencyMeasure(); /* Code sometimes stop at this function. Reset the cable connection */

  AD5940RampStructInit(); /* User interface for ramp structure setting */

  /* Get ramp frequency and store it in the data packet */
  RampFrequency = AppSWVGetRampFrequency();
  memcpy(RampFrequencyBytes, &RampFrequency, sizeof(RampFrequency));

  AppSWVInit(AppBuff, APPBUFF_SIZE);    /* Initialize RAMP application. Provide a buffer, which is used to store sequencer commands */	

  StepNumber = AppSWVGetStepNumber();
  GenerateRampDacCodeArray(RampDACArray, StepNumber); /* Calculate and store DAC voltage values */

  AD5940_Delay10us(100000); /* Delay to stabilize after initialization */


  /* BLE */
  if (!BLE.begin()) {
    Serial.println("failed to initialize BLE!");
    while (1);
  }

  BLE.setDeviceName("SWVReader");
  BLE.setLocalName("SWVReader");
  BLE.setConnectionInterval(0x0006, 0x0c80);
  BLE.setAdvertisedService(SWVService);
  SWVService.addCharacteristic(VoltageChar);
  SWVService.addCharacteristic(CurrentChar);
  SWVService.addCharacteristic(RampFrequencyChar); /* Sends ramp sample delay for plotting */
  SWVService.addCharacteristic(EndFlagChar); /* Sends flag when the transimission is done */
  BLE.addService(SWVService);

  VoltageChar.writeValue(packet_initial, sizeof(packet_initial));
  CurrentChar.writeValue(packet_initial, sizeof(packet_initial));
  RampFrequencyChar.writeValue(RampFrequencyBytes, sizeof(RampFrequencyBytes));
  EndFlagChar.writeValue((byte)0);

  BLE.advertise();
  Serial.println("Bluetooth device active, waiting for connections...");

  while(1){
    central = BLE.central();
    if(central){
       Serial.println("connected to the device... ");
       Serial.println(central.address());
       break;
    }
    delay(5); 
  }

  /* DO NOT use delay() function below. Use BLEdelay() instead. */

  AppSWVCtrl(APPCTRL_START, 0);          /* Control SWV measurement to start. Second parameter has no meaning with this command. */
}

void loop() {
  BLE.poll();

  if (!central) {
    central = BLE.central(); 
  }

  /* ISR */
  while(1){
    BLE.poll();
    if(ucInterrupted) {
      AD5940_ClrMCUIntFlag();
      // DataCount = APPBUFF_SIZE;
      AppSWVISR(AppBuff, &DataCount); /* Interrupt service routine: Stores ADC values when the sequencer ends */
      if (DataCount > 0) {  /* Break when data is stored */
        break;
      }     
    }
  }

  Serial.println("Waiting 2 seconds before sending...");
  BLEdelay(2000);  /* Delay to stabilize BLE connection. DO NOT use delay() function */

  /* Sending the data packets via BLE */
  for (int i = 0; i < DataCount; i += PACKET_SIZE) {

    for (int j = 0; j < PACKET_SIZE; j++) {
      float value_voltage;
      float value_current;
      
      if ((i+j) < DataCount) {
        value_voltage = ((float*)RampDACArray)[i+j];
        value_current = ((float*)AppBuff)[i+j];
      } else {
        value_voltage = NAN;
        value_current = NAN;
      }
      memcpy(&packet_voltage[4 * j], &value_voltage, 4);
      memcpy(&packet_current[4 * j], &value_current, 4);
    }

    BLEdelay(100);

    if (central && central.connected()) {
      VoltageChar.writeValue(packet_voltage, sizeof(packet_voltage));
      }

    BLEdelay(100);

    if (central && central.connected()) {
      CurrentChar.writeValue(packet_current, sizeof(packet_current));
    }
  }
    
  BLEdelay(100);

  /* Send end flag */
  byte doneFlag = 1;
  if (central && central.connected()) {
      EndFlagChar.writeValue(doneFlag);
      Serial.println("End flag sent");
  }

  while(1) { BLE.poll(); }
}


/* FUNCTIONS */
void INT0_ISR() {
  ucInterrupted = 1;
}


void BLEdelay(unsigned long time){ // ms
  unsigned long start = millis();
  while (millis() - start < time) {
    BLE.poll();
  }
}

int32_t AD5940FrequencyMeasure(void){
  /* Measure LFOSC frequency */
  /**@note Calibrate LFOSC using system clock. The system clock accuracy decides measurement accuracy. Use XTAL to get better result. */

  LFOSCMeasure_Type LfoscMeasure;
  LfoscMeasure.CalDuration = 1000.0;  /* 1000ms used for calibration. */
  LfoscMeasure.CalSeqAddr = 0;        /* Put sequence commands from start address of SRAM */
  LfoscMeasure.SystemClkFreq = 16000000.0f; /* 16MHz in this firmware. */
  AD5940_LFOSCMeasure(&LfoscMeasure, &LFOSCFreq);
  Serial.print("LFOSC Freq:");
  Serial.println(LFOSCFreq);
  // AD5940_SleepKeyCtrlS(SLPKEY_UNLOCK);         /*  */
  
  return 0;
}


/**
 * @brief The interface for user to change application paramters.
 * @return return 0.
*/
void AD5940RampStructInit(void)
{
  AppSWVCfg_Type *pRampCfg;
  
  AppSWVGetCfg(&pRampCfg);
  /* Step1: configure general parmaters */
  pRampCfg->SeqStartAddr = 0x10;                /* leave 16 commands for LFOSC calibration.  */
  pRampCfg->MaxSeqLen = 512-0x10;              /* 4kB/4 = 1024  */
  pRampCfg->RcalVal = 10000.0;                   /* 10kOhm RCAL */
  pRampCfg->ADCRefVolt = 1.820f;               /* The real ADC reference voltage. Measure it from capacitor C12 with DMM. */
  pRampCfg->FifoThresh = 1023;              /* Maximum value is 4kB/4-1 = 1024-1. Set it to higher value to save power. */
  pRampCfg->SysClkFreq = 16000000.0f;           /* System clock is 16MHz by default */
  pRampCfg->LFOSCClkFreq = LFOSCFreq;           /* LFOSC frequency */
	pRampCfg->AdcPgaGain = ADCPGA_1P5;
	pRampCfg->ADCSinc3Osr = ADCSINC3OSR_4; 
  
	/* Step 2:Configure square wave signal parameters */
  pRampCfg->RampStartVolt = -400.0f;     /* Measurement starts at 0V*/
  pRampCfg->RampPeakVolt = 0.0f;     		 /* Measurement finishes at -0.4V */
  pRampCfg->VzeroStart = 1300.0f;           /* Vzero is voltage on SE0 pin: 1.3V */
  pRampCfg->VzeroPeak = 1300.0f;          /* Vzero is voltage on SE0 pin: 1.3V */
  pRampCfg->Frequency = 75;                 /* Frequency of square wave in Hz */
  pRampCfg->SqrWvAmplitude = 150;       /* Amplitude of square wave in mV */
  pRampCfg->SqrWvRampIncrement = 5; /* Increment in mV*/
  pRampCfg->SampleDelay = 2.0f;             /* Time between update DAC and ADC sample. Unit is ms and must be < (1/Frequency)/2 */
  pRampCfg->LPTIARtiaSel = LPTIARTIA_1K;      /* Maximum current decides RTIA value */
	pRampCfg->bRampOneDir = bFALSE; //bTRUE; 			/* Only measure ramp in one direction */
}