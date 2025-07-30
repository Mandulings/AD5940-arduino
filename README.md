# AD5940-arduino

<aside>

This document is a guide for performing **Square Wave Voltammetry** (SWV) using the **EVAL-AD5940ELCZ** board controlled by an **Arduino Nano 33 BLE.**

</aside>


# How to run the measurement

## 1 Hardware Setup

1. Connect the pins of the Arduino Nano 33 BLE and the EVAL-AD5940ELCZ as shown below.
    
    
    | **Role** | **Arduino Nano 33 BLE** | **EVAL-AD5940ELCZ** |
    | --- | --- | --- |
    | Vin(3.3V) | 3.3V | 3.3V_Shield |
    | GND | GND | GND_Shield |
    | RST | D9 | RST_Shield |
    | CS | D10 | D10 |
    | MOSI | D11 | D11 |
    | MISO | D12 | D12 |
    | SCLK | D13 | D13 |
    | Interrupt | D2 | D2 |
    - Pinout Schematics
        - EVAL-AD5940ELCZ

        <img width="680" height="467" alt="스크린샷 2025-07-29 140542" src="https://github.com/user-attachments/assets/ba8ca212-f9d8-4c67-be90-654d8acc4563" />

        - Arduino Nano BLE 33
        
        <img width="1634" height="976" alt="스크린샷 2025-07-29 135816" src="https://github.com/user-attachments/assets/f1e6f655-c813-4dda-835a-8990fda294f3" />

    
2. Make sure that the jumpers on the following connectors are in the correct position.
    
    
    | **Connector** | **Position** | **Description** |
    | --- | --- | --- |
    | J6 | B | Set to Network B to connect the 1kΩ || 3kΩ between RE0 and SE0 |
    | J10, J11 | C (PIN 5-6) | RE0, SE0 connected to USB port |
    - Jumper Description
        
        <img width="1235" height="1332" alt="스크린샷 2025-07-29 134824" src="https://github.com/user-attachments/assets/f8b34986-abef-411a-80b2-33548cef64bf" />

        

1. Connect each electrode to the corresponding USB cable.
    
    
    | **Cable Color** | **Description** | **Schematic Reference** |
    | --- | --- | --- |
    | Red | Counter Electrode | USB_1 |
    | White | Reference Electrode | USB_2 |
    | Green | Working Electrode | USB_3 |
    | Black | DE0 input | USB_4 |


## 2️ Software Configuration

1. Copy the folder `AD5940_arduino` into your Arduino libraries folder (Documents > Arduino > libraries). Once installed, you will be able to open the example code `AD5940_SqaureWaveVoltammetry.ino` from the Arduino IDE.
2. The `AD5940RampStructInit()` function configures the main application parameters for generating the excitation signal and acquiring the measured data. You can modify the following parameters as needed to adjust the ramp characteristics.

```cpp
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
```

The table below explains the meaning of each parameter:

<img width="1423" height="1245" alt="스크린샷 2025-07-29 144208" src="https://github.com/user-attachments/assets/47d41512-9afe-43b1-8208-c2cc087379b7" />


## 3️⃣ SWV Measurement & Transmission

1. Connect the Arduino to your computer and upload the code.
* *Note: Sometimes, the code may pause at the `AD5940FrequencyMeasure();` function inside the `setup()` routine. If this happens, try unplugging and reconnecting the USB cable between the Arduino and the desktop.*
2. Once the Serial Monitor displays `"Bluetooth device active, waiting for connections...”`, run the `SWVReader.m` in MATLAB to begin BLE data transmission.
3. After the data transimission is complete, the data will automatically be saved as `.xlsx` file in the current folder. Additionally, two plots will appear:
    - DAC ramp voltage vs. time
    - ADC current vs. DAC ramp voltage
    
    ** Note: Data loss may occur during BLE transmission. If the amount of data saved in MATLAB is smaller than the `FIFO Count` displayed in the Serial Monitor, try increasing the communication interval and rerun the test.*
    

---

# 📎 Advanced Explanation

## 🔍 Code Workflow

<img width="552" height="711" alt="Schematic_Code" src="https://github.com/user-attachments/assets/a892ffe2-8284-4508-b4bf-f04862cffe61" />


### void setup()

During `setup()`, the MCU initializes communication with the AD5940, configures the DAC, sequencer, and ADC, and calculates the DAC ramp voltage data. Once BLE connection is established with a host device, the sequencer is enabled to begin measurement.

- **Key functions**
    - `AD5940RampStructInit()`
        
        This function allows users to configure the ramp voltage parameters.
        
    - `AppSWVInit()`
        
        This function initializes the AD5940's sequencer and data FIFO, and writes the sampling sequence information.
        
        | Function Name | Sequence Role | Sequence ID |
        | --- | --- | --- |
        | `AppSWVSeqInitGen()`  | Initializes LPDAC, LPAMP, and ADC components | SEQID_3 |
        | `AppSWVSeqADCCtrlGen()` | Controls ADC conversion and FIFO storage | SEQID_2 |
        | `AppSWVSeqDACCtrlGen()` | Update DAC code step by step. | SEQID_0 |
        
        Once the sequences are configured, `SEQID_3` is triggered to initialize the system for measurement.
        
    - `GenerateRampDacCodeArray()`
        
        This function calculates an array of DAC ramp voltages over time. The computation is performed entirely on the MCU, and the results are later transmitted to the PC via BLE. This calculation does not affect DAC updates within the AD5940 itself.
        
    - `AppSWVCtrl(APPCTRL_START, 0)`
        
        This command starts the wakeup timer, which in turn triggers the sequencer and begins the measurement.
        The sequences configured in `AppSWVInit()` are assigned as follows:
        
        | Wakeup Timer | SeqA | SeqB | SeqC | SeqD |
        | --- | --- | --- | --- | --- |
        | Sequence | SEQID_0 | SEQID_2 | SEQID_1 | SEQID_2 |
    

### void loop()

Inside `loop()`, the AD5940 autonomously executes DAC updates and ADC sampling based on the preloaded sequencer steps. At the end of each DAC block, a **custom interrupt (INT0)** is triggered, prompting the MCU to update the sequencer via an interrupt service routine (ISR). When the entire sequence is completed, an **end interrupt** is triggered, and the MCU reads the current data from FIFO memory. The collected data is grouped into packets of 5 samples, transmitted via BLE, and saved on the host device.

- **Key functions**
    - `AppSWVISR()`
        
        This function is called when the MCU receives an interrupt signal from the AD5940 via digital pin D2.
        
        | **Interrupt** | **Trigger Condition** | **MCU Action** |
        | --- | --- | --- |
        | INT0 | Triggered at the end of a sequence block. | Calls the function `AppSWVSeqDACCtrlGen()` to write the next DAC update sequence to SRAM. |
        | END | Triggered when the measurement is complete. | Reads data from the FIFO. |
        | FIFO Threshold | Triggered when the FIFO reaches its threshold (4kB). | Reads data from the FIFO. |
    - `BLE.poll()`
        
        After advertising, `BLE.poll()` must be continuously called to maintain data transmission. Instead of using the `delay()` function, it is recommended to use a custom function like `BLEdelay()` to ensure the BLE connection remains active.
        

## Library Modification

The **`AD5940_arduino.h`** library merges **`AD5940.h`** and **`SquareWaveVoltammetry.h`**, along with selected portions from **`ADICUP3029Port.c`** and **`AD5940Main.c`**, and has been adapted for the Arduino environment.

**Key Modifications:**

- **`GenerateRampDacCodeArray()` Function**
    - A new function added to calculate the voltage steps for the DAC ramp and store them in an array
- **SPI Communication**
    - Modified `AD5940_SPIWriteReg()` and `AD5940_SPIReadReg()` for compatibility with Arduino
    - Implemented according to the SPI communication section in the AD5940 datasheet
- **Arduino Compatibility Fixes**
    - Replaced or removed C functions not supported by Arduino, such as `printf()`

# References

### Datasheets & Schematics

- [AD5940 Userguide](https://wiki.analog.com/resources/eval/user-guides/ad5940)
- [AD5940 Datasheet](https://www.analog.com/en/products/ad5940.html)
- [EVAL-AD5940ELCZ Hardware Info & Schematics](https://wiki.analog.com/resources/eval/user-guides/eval-ad5940/hardware/eval-ad5940elcz)
- [EVAL-AD5940ELCZ SWV guide](https://wiki.analog.com/resources/eval/user-guides/eval-ad5940/software_examples/ad5940_swv)

### Original Codes

- [AD5940.h](https://github.com/analogdevicesinc/ad5940lib)
- [SquareWaveVoltammetry.h & AD5940Main.c](https://github.com/analogdevicesinc/ad5940-examples/tree/master/examples/AD5940_SqrWaveVoltammetry)
