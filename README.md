# FundamentalUpdater_rev5

**ระบบอัปเดตข้อมูลหุ้นไทยอัตโนมัติสำหรับ SET, AmiBroker, Excel และ FundamentalWeb**

`FundamentalUpdater_rev5` คือแพ็กเกจ Windows สำหรับ workflow ข้อมูลหุ้นของ Plaifa Engineering โดยรวม workflow ที่ผ่านการทดสอบของ **Price V4, Fundamental V4 และ Excel V4** ไว้ในแพ็กเกจ V5

> **สำคัญ:** V5 เป็นชื่อแพ็กเกจ/Installer ส่วน logic หลักที่ใช้งานจริงยังคงเป็น Price V4, Fundamental V4 และ Excel V4 ที่ผ่านการทดสอบแล้ว

## 🔗 Related Web Dashboard

ข้อมูล Fundamental จากระบบนี้ถูกนำไปใช้ต่อในโปรเจกต์ **FundamentalWeb**

- Repository: https://github.com/Somyotthanimwas/FundamentalWeb
- Live Dashboard: https://fundamentalweb-frontend.onrender.com/

ข้อมูลที่ Web ใช้เป็นหลักคือ `fundamental_v4.csv`

---

## 🏗️ System Architecture

```text
                         SET
                          │
             ┌────────────┴────────────┐
             │                         │
          Price V4                Fundamental V4
             │                         │
             ▼                         ▼
      set-price.csv             SET JSON / CDP
             │                         │
             ▼                         ▼
 set-price-amibroker.csv      Fundamental JSON
             │                         │
             ▼                         ▼
        AmiBroker          fundamental_v4.csv
                                       │
                                       ├──────────────► Excel V4 Macro 2
                                       │                         │
                                       │                         ▼
                                       │                  Excel Stocks
                                       │                         │
                                       │                         ▼
                                       │                  Excel V4 Macro 1
                                       │                         │
                                       │                         ▼
                                       │                     AmiBroker
                                       │
                                       ▼
                                  FundamentalWeb
                                       │
                                       ▼
                                  Web Dashboard
```

---

## 📋 Main Workflow

1. ดึงข้อมูลราคาหุ้นจาก SET ด้วย Price V4
2. สร้าง `set-price.csv`
3. แปลงข้อมูลราคาเป็น `set-price-amibroker.csv`
4. นำเข้าข้อมูลราคาเข้า AmiBroker แบบ background/hidden
5. ดึงข้อมูล Fundamental จาก SET ผ่าน Chrome DevTools Protocol (CDP)
6. บันทึก Financial / Trading / Historical / Current / Related Product JSON ตาม workflow
7. สร้าง `fundamental_v4.csv`
8. เรียก Excel V4 Macro 2 เพื่ออัปเดตข้อมูล Fundamental
9. เรียก Excel V4 Macro 1 เพื่อส่งข้อมูลเข้า AmiBroker
10. ส่งสถานะการทำงานผ่าน LINE
11. `fundamental_v4.csv` สามารถนำไปใช้ต่อกับ FundamentalWeb

---

## 📊 Data Flow ที่สำคัญ

```text
FundamentalUpdater_rev5
        │
        ▼
Fundamental V4
        │
        ▼
fundamental_v4.csv
        │
        ├────────► Excel V4 Macro 2
        │
        └────────► FundamentalWeb
```

### Source of Truth

ไฟล์ Fundamental หลักคือ:

```text
Data/Fundamental/fundamental_v4.csv
```

ไฟล์ต้นทางบน Windows ที่ใช้ใน workflow หลัก:

```text
C:\Program Files\FundamentalUpdater_rev4\Data\Fundamental\fundamental_v4.csv
```

> **กฎสำคัญ:** `fundamental_v4.csv` เป็นไฟล์หลักสำหรับ Fundamental workflow และ Excel Macro 2

> `fundamental_869.csv` **ไม่ใช่ไฟล์หลักสำหรับ Macro 2** และไม่ควรนำมาแทน `fundamental_v4.csv`

---

## 🚀 ความสามารถสำคัญ

- รองรับหุ้นประมาณ **869 symbols** ตาม `symbols.txt`
- Price workflow สำหรับข้อมูลราคาจาก SET
- Fundamental workflow สำหรับ Financial / Trading / Historical / Related Product data
- ใช้ Chrome/CDP สำหรับการเข้าถึงข้อมูล SET ในกรณีที่ direct API ถูกป้องกัน
- สร้าง `fundamental_v4.csv` สำหรับ Excel V4
- AmiBroker import ทำงานแบบ hidden/background
- รองรับ Windows x64
- Build ด้วย CMake + MinGW
- Installer สร้างด้วย Inno Setup
- สามารถส่งข้อมูล Fundamental ต่อไปยัง FundamentalWeb

---

## 📁 Project Structure

```text
FundamentalUpdater_rev5/
├── CMakeLists.txt
├── FundamentalUpdater_rev5.iss
├── config.ini
├── symbols.txt
├── Set_Amibroker_ok.xlsm
├── include/
├── src/
├── tools/
├── cmake/
├── Data/
│   └── Fundamental/
└── build-win-fixed/
```

ไฟล์ runtime เช่น CSV, JSON, log และ executable ที่เกิดจากการ build ไม่ควร commit เข้า Git repository ตาม `.gitignore`

---

## 📦 Important Output Files

### Price

```text
Data/set-price.csv
Data/set-price-amibroker.csv
```

### Fundamental

```text
Data/Fundamental/fundamental_v4.csv
Data/Fundamental/JSON/*_financial_data.json
Data/Fundamental/JSON/*_trading_stat.json
Data/Fundamental/JSON/*_historical_trading.json
Data/Fundamental/JSON/*_related_product.json
```

---

## 📈 Excel V4

Workbook หลัก:

```text
Set_Amibroker_ok.xlsm
```

### Macro 2

Macro 2 ใช้ **`fundamental_v4.csv` เท่านั้น** เพื่ออัปเดตค่าหลัก เช่น:

- D/E Ratio
- EPS
- ROA%
- ROE%
- NPM%
- P/E
- P/BV
- Dividend Yield%

### Macro 1

หลังจาก Macro 2 อัปเดต Stocks แล้ว Macro 1 จะนำข้อมูลเข้าสู่ AmiBroker

```text
fundamental_v4.csv
        ↓
Macro 2
        ↓
Stocks
        ↓
Macro 1
        ↓
AmiBroker
```

---

## 📊 AmiBroker

ระบบใช้ AmiBroker 32-bit โดยค่า default database path คือ:

```text
C:\Program Files (x86)\AmiBroker\Data
```

Price import ใช้ COM automation ผ่าน `Broker.Application` และตั้ง AmiBroker ให้ทำงานแบบ background/hidden ตาม workflow ของระบบ

---

## 📅 Price V4 — กฎเรื่องวันที่ตลาด

ส่วนนี้เป็น **baseline สำคัญ** ของระบบและไม่ควรแก้โดยไม่ทดสอบ Price workflow ทั้งชุด

เมื่อวันที่ปัจจุบันของเครื่องตรงกับวันที่ตลาดหยุดทำการ ระบบต้องไม่ใช้วันที่เครื่องเป็นวันที่ตลาดโดยอัตโนมัติ

หลักการคือ:

> **วันที่ข้อมูลราคาต้องอ้างอิงจากวันที่ตลาดมีข้อมูลจริงล่าสุด ไม่ใช่วันที่ปัจจุบันของเครื่องเมื่อไม่มีการซื้อขาย**

การแก้ Web หรือ Fundamental workflow ต้องไม่ทำให้ logic ของ Price V4 ที่ทำงานอยู่แล้วเสียหาย

---

## 🌐 FundamentalWeb Integration

`FundamentalWeb` เป็น Web Dashboard ที่อ่านข้อมูล Fundamental จาก `fundamental_v4.csv`

```text
Fundamental V4
      ↓
fundamental_v4.csv
      ↓
FundamentalWeb
      ↓
React Dashboard
```

Web Repository:

https://github.com/Somyotthanimwas/FundamentalWeb

Live Dashboard:

https://fundamentalweb-frontend.onrender.com/

> Web เป็นส่วนต่อยอดจากข้อมูล Fundamental และไม่ควรเปลี่ยน logic ของ Price V4 เพียงเพื่อแก้ปัญหา Web

---

## 🧩 Fundamental Capture

การเข้าถึงข้อมูล SET ใช้ Chrome/CDP ในกรณีที่ direct API ถูกป้องกัน

ข้อมูลต่อ symbol มีไฟล์หลักในรูปแบบ:

```text
*_financial_data.json
*_trading_stat.json
*_historical_trading.json
*_related_product.json
```

ผลการทดสอบที่บันทึกไว้ใน workflow เดิม:

```text
Success : 869
Failed  : 0
```

---

## 🛠️ Build on WSL Ubuntu

ต้องมี MinGW-w64, CMake, vcpkg และ dependencies ของโปรเจกต์

### Configure

```bash
cd ~/FundamentalUpdater_rev5

cmake -S . -B build-win-fixed \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-windows-fixed.cmake
```

### Build

```bash
cmake --build build-win-fixed --target FundamentalUpdater_rev5 -j1
cmake --build build-win-fixed --target FundamentalCapture -j1
cmake --build build-win-fixed --target FundamentalCsvConverter -j1
```

Executable:

```text
build-win-fixed/FundamentalUpdater_rev5.exe
build-win-fixed/FundamentalCapture.exe
build-win-fixed/FundamentalCsvConverter.exe
```

---

## 📦 Build Installer

ติดตั้ง Inno Setup 6 บน Windows แล้วสามารถเรียกจาก WSL ได้ เช่น:

```bash
cd ~/FundamentalUpdater_rev5

/mnt/c/Program\ Files\ \(x86\)/Inno\ Setup\ 6/ISCC.exe \
  FundamentalUpdater_rev5.iss
```

ผลลัพธ์:

```text
FundamentalUpdater_rev5_Setup.exe
```

Default installation directory:

```text
C:\Program Files\FundamentalUpdater_rev5
```

---

## 🔧 Installation

Installer หลัก:

```text
FundamentalUpdater_rev5_Setup.exe
```

หลังติดตั้ง ระบบสามารถสร้าง shortcut สำหรับเรียกใช้งานจาก Windows Start Menu / Desktop ตามการตั้งค่าใน Inno Setup

---

## 🔍 Monitoring / Troubleshooting

ตรวจสอบไฟล์ Fundamental:

```bash
ls -lh Data/Fundamental/fundamental_v4.csv
```

ตรวจสอบ JSON:

```bash
find Data/Fundamental/JSON -type f | wc -l
```

ตรวจสอบจำนวน symbol:

```bash
wc -l symbols.txt
```

เมื่อ Web ไม่อัปเดต ให้ตรวจตามลำดับ:

```text
1. FundamentalUpdater สร้าง fundamental_v4.csv แล้วหรือยัง
2. fundamental_v4.csv มีข้อมูล/เวลาแก้ไขล่าสุดหรือไม่
3. CSV ถูก Sync ไป FundamentalWeb/GitHub หรือไม่
4. Backend ของ FundamentalWeb โหลด CSV ใหม่หรือไม่
5. /api/stocks ตอบข้อมูลใหม่หรือไม่
6. Frontend แสดงข้อมูลใหม่หรือไม่
```

---

## ⚠️ Important Rules / Baseline

1. **Price V4 ที่ทำงานอยู่แล้วต้องไม่ถูกแก้โดยไม่จำเป็น**
2. **`fundamental_v4.csv` คือ Fundamental CSV หลัก**
3. **Excel Macro 2 ต้องใช้ `fundamental_v4.csv`**
4. `fundamental_869.csv` ไม่ใช่ไฟล์หลักสำหรับ Macro 2
5. วันที่ Price ต้องอ้างอิงตลาดจริงเมื่อไม่มีการซื้อขาย
6. Web Dashboard ต้องอ่านข้อมูลจาก Fundamental V4
7. การแก้ Web ต้องไม่ทำให้ Price V4 เสีย
8. `Data/` เป็น runtime data และไม่ควร commit ข้อมูลส่วนตัว/ข้อมูล runtime ที่ไม่จำเป็น
9. Backup/source backup files ไม่ควรถูกนำเข้า release build

---

## 🧪 End-to-End Release Baseline

ชุด workflow ที่ใช้เป็น baseline:

```text
Price V4
   ↓
AmiBroker Price Import
   ↓
Fundamental V4 Capture
   ↓
FundamentalCsvConverter
   ↓
fundamental_v4.csv
   ↓
Excel V4 Macro 2
   ↓
Excel V4 Macro 1
   ↓
AmiBroker
   ↓
LINE notification
   │
   └────────► FundamentalWeb
```

---

## 📌 Current System

```text
Package          FundamentalUpdater_rev5
Price Logic      Price V4
Fundamental      Fundamental V4
Excel             Excel V4
Main CSV          fundamental_v4.csv
Symbols           ~869
Platform          Windows x64
Build             CMake + MinGW
Installer         Inno Setup
AmiBroker         32-bit
Web               FundamentalWeb
Web Hosting       Render
```

---

## 🔗 Repository

https://github.com/Somyotthanimwas/FundamentalUpdater_rev5

Main branch:

```text
main
```

---

## 📜 License

โปรเจกต์นี้จัดทำเพื่อใช้งานภายใน **Plaifa Engineering** เว้นแต่จะมีการระบุเงื่อนไขการเผยแพร่เพิ่มเติมในภายหลัง
