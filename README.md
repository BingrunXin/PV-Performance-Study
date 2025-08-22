# An Empirical Study on the Performance of a Photovoltaic Module Under Real-World Environmental Conditions

**Author:** [Your Name]  
**Project Duration:** [Start Date] – [End Date], 2025  
**Location:** [Your City, Country]

---

## Abstract
This project presents the design, implementation, and analysis of a comprehensive data acquisition system built to characterize the performance of a polycrystalline silicon photovoltaic (PV) module under dynamic, real-world environmental conditions.  

The study was conducted in two distinct phases:  

- **Phase I:** A 10-day continuous monitoring period with a fixed 150 Ω load to establish a baseline performance model correlating power output with solar irradiance and ambient temperature.  
- **Phase II:** A rapid load-scanning methodology, utilizing resistors from 20 Ω to 200 Ω, to empirically derive the module's Current–Voltage (I-V) and Power–Voltage (P-V) characteristic curves.  

Key findings:  
- Strong linear dependence of power output on irradiance.  
- Negative thermal coefficient reducing conversion efficiency.  
- Identification of the **Maximum Power Point (MPP)** under different conditions.  
- Quantification of a significant **“mismatch loss” (~25–30%)** between fixed-load operation and the true MPP, validating the importance of **Maximum Power Point Tracking (MPPT)**.  
- Development of fault-tolerant firmware for reliable long-term operation.  

---

## 1. Introduction
Photovoltaics (PV) are central to the global renewable energy transition. However, real-world PV performance deviates significantly from **Standard Test Conditions (STC)** due to environmental variability.  

This project set out to **bridge the gap between theory and field performance**, with four primary objectives:  

1. Design and deploy a robust multi-sensor data acquisition system.  
2. Quantify the relationship between irradiance, temperature, and module power under a fixed load.  
3. Empirically measure I-V and P-V curves to identify the MPP.  
4. Quantify energy “mismatch loss” to highlight the role of MPPT technology.  

---

## 2. System Design & Methodology

### 2.1 Hardware Platform
- **Microcontroller:** TTGO T8 (ESP32)  
- **PV Module:** 10 W Polycrystalline Silicon Panel (Voc: 21 V)  
- **Power Measurement:** INA226 DC current & power sensor  
- **Irradiance Sensor:** MAX44009 ambient light sensor  
- **Temperature Sensors:** 2 × DS18B20 probes (rear surface of panel)  
- **Ambient Sensor:** BMP280 (temperature & pressure)  
- **Storage:** MicroSD card module  
- **User Interface:** SSD1306 OLED display  
- **I²C Multiplexing:** TCA9548A multiplexer  

📌 *Insert schematic diagram here:*  
`/images/system_architecture.png`  

### 2.2 Software & Data Acquisition
- Developed in **C++ (Arduino framework)**.  
- **Fault tolerance:** Infinite retries for critical components (SD, RTC); self-healing re-initialization for failed sensors.  
- **Data logging:** `.csv` format with timestamp (1 s accuracy).  
- **Sampling interval:** ~30 s.  

### 2.3 Experimental Procedure
- **Phase I (Fixed Load):** 150 Ω resistor, continuous 10-day deployment.  
- **Phase II (Load Scanning):** Resistors from 20 Ω to 200 Ω swapped rapidly under stable irradiance (~1 min per step).  

### 2.4 Data Analysis Workflow
Analysis performed in **Python (Jupyter Notebook)**.  
Libraries: Pandas, Matplotlib, Seaborn.  

📌 *Notebook:* `/Analysis/Solar_PV_Analysis.ipynb`  

---

## 3. Results & Analysis

### 3.1 Long-Term Performance (150 Ω Fixed Load)
- Power output follows irradiance closely.  
- Module runs significantly hotter than ambient.  
- Efficiency decreases with temperature.  

📌 *Insert plot:* Time series of **irradiance, module temperature, and power**  
`/images/phase1_timeseries.png`  

📌 *Insert plot:* Scatter of **temperature vs. efficiency metric**  
`/images/efficiency_vs_temp.png`  

---

### 3.2 PV Characteristic Curves & Maximum Power Point
- I-V and P-V curves plotted for each sweep.  
- MPP identified as the peak of the P-V curve.  
- Higher irradiance → higher Pmax; Vmp decreases slightly with temperature.  

📌 *Insert plots:*  
- `/images/IV_curve_peak.png`  
- `/images/PV_curve_peak.png`  
- `/images/mpp_comparison.png`  

---

### 3.3 Quantifying the Mismatch Loss
- Theoretical Pmax estimated for Phase I using sweep-derived curves.  
- Fixed 150 Ω load underperformed by **~27.8%** on a clear day.  

📌 *Insert plot:* Comparison of **theoretical Pmax vs actual power**  
`/images/mismatch_loss.png`  

---

## 4. Discussion
- Confirmed irradiance as the primary driver of PV power.  
- Demonstrated negative thermal coefficient of voltage.  
- Characterized the dynamic nature of the MPP.  
- Provided empirical proof of **~25–30% energy loss** without MPPT.  

*Limitation:* Manual resistor swapping introduces small delays.  
*Future improvement:* Programmable electronic load for fully automated sweeps.  

---

## 5. Conclusion & Future Work
This project:  
- Designed and validated a robust PV data acquisition system.  
- Empirically characterized I-V and P-V curves.  
- Quantified mismatch losses from fixed load vs. true MPP.  

**Future directions:**  
- Implement a simple MPPT controller (e.g., Perturb & Observe).  
- Compare mono- vs poly-crystalline panels under identical conditions.  
- Study long-term soiling and degradation effects.  

---

## 6. Repository Structure
