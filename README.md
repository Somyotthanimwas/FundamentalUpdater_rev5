# FundamentalUpdater_rev5

ระบบอัปเดตข้อมูลหุ้นไทยอัตโนมัติสำหรับ SET, AmiBroker, Excel และ LINE

## ภาพรวม

`FundamentalUpdater_rev5` เป็นรุ่นแพ็กเกจ V5 ที่รวม workflow เดิมที่ทดสอบแล้วของ Price V4, Fundamental V4 และ Excel V4 โดยมีหน้าที่หลักดังนี้

1. ดึงข้อมูลราคาหุ้นจาก SET
2. สร้าง `set-price.csv`
3. แปลงข้อมูลราคาเป็น `set-price-amibroker.csv`
4. นำเข้าข้อมูลราคาเข้า AmiBroker แบบ background/hidden
5. ดึงข้อมูล Fundamental จาก SET ผ่าน Chrome DevTools Protocol (CDP)
6. บันทึก Financial / Trading / Historical / Current JSON
7. สร้าง `fundamental_v4.csv`
8. เรียก Excel V4 Macro 2 เพื่ออัปเดตข้อมูล Fundamental
9. เรียก Excel V4 Macro 1 เพื่อส่งข้อมูลเข้า AmiBroker
10. ส่งสถานะการทำงานผ่าน LINE

> V5 เป็นชื่อแพ็กเกจและ installer ส่วน logic หลักยังคงใช้ Price V4, Fundamental V4 และ Excel V4 ที่ผ่านการทดสอบแล้ว

## ความสามารถสำคัญ

- รองรับหุ้นประมาณ 869 symbols ตาม `symbols.txt`
- Price workflow ประมวลผลข้อมูลราคาจาก SET
- Fundamental workflow เก็บข้อมูล JSON จาก SET
- ใช้ `fundamental_v4.csv` เป็นไฟล์ Fundamental หลักสำหรับ Excel Macro 2
- AmiBroker import ทำงานแบบ hidden/background เพื่อลดการรบกวนผู้ใช้
- รองรับการ build เป็น Windows executable แบบ x64 ด้วย MinGW
- Installer สร้างด้วย Inno Setup

## โครงสร้างโปรเจกต์

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

ไฟล์ข้อมูลที่สร้างระหว่าง runtime เช่น CSV, JSON, log และ executable จะไม่ถูกเก็บใน Git repository ตาม `.gitignore`

## ไฟล์ข้อมูลสำคัญ

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

`fundamental_v4.csv` เป็นไฟล์หลักที่ใช้โดย Excel Macro 2

## Excel

Workbook หลัก:

```text
Set_Amibroker_ok.xlsm
```

Macro 2 อัปเดตค่าหลักจาก `fundamental_v4.csv` ได้แก่:

- D/E Ratio
- EPS
- ROA%
- ROE%
- NPM%
- P/E
- P/BV
- Dividend Yield%

จากนั้น Macro 1 ใช้ข้อมูลใน Excel เพื่ออัปเดต AmiBroker

## AmiBroker

ระบบใช้ AmiBroker 32-bit โดยค่า default database path คือ:

```text
C:\Program Files (x86)\AmiBroker\Data
```

การ import ราคาจะใช้ COM automation ผ่าน `Broker.Application` และตั้ง `AB.Visible = False` เพื่อทำงานแบบ background

## การติดตั้ง

ดาวน์โหลด installer จาก GitHub Releases หรือจากไฟล์ installer ที่สร้างจาก Inno Setup:

```text
FundamentalUpdater_rev5_Setup.exe
```

ค่า default installation directory:

```text
C:\Program Files\FundamentalUpdater_rev5
```

หลังติดตั้ง โปรแกรมจะสร้าง shortcut ใน Start Menu และ Desktop

## การ Build บน WSL Ubuntu

ต้องมี MinGW-w64, CMake และ vcpkg พร้อม dependencies ที่โปรเจกต์ต้องใช้

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

Executable ที่ได้:

```text
build-win-fixed/FundamentalUpdater_rev5.exe
build-win-fixed/FundamentalCapture.exe
build-win-fixed/FundamentalCsvConverter.exe
```

## สร้าง Installer

ติดตั้ง Inno Setup 6 บน Windows แล้วเรียกจาก WSL ได้ เช่น:

```bash
cd ~/FundamentalUpdater_rev5

/mnt/c/Program\ Files\ \(x86\)/Inno\ Setup\ 6/ISCC.exe \
  FundamentalUpdater_rev5.iss
```

ผลลัพธ์:

```text
FundamentalUpdater_rev5_Setup.exe
```

## การทำงานของ Price V4

Price V4 มีการตรวจสอบวันที่ตลาดจากข้อมูลจริงของ SET เพื่อไม่ใช้วันที่เครื่องเป็นวันที่ตลาดในกรณีตลาดหยุดทำการ

หลักการสำคัญคือ:

> วันที่ในข้อมูลราคาต้องอ้างอิงจากวันที่ตลาดมีข้อมูลจริงล่าสุด ไม่ใช่วันที่ปัจจุบันของเครื่องเมื่อไม่มีการซื้อขาย

ส่วนนี้เป็น baseline สำคัญและไม่ควรเปลี่ยน logic โดยไม่ทดสอบ Price workflow ทั้งชุด

## Fundamental Capture

การเข้าถึง SET API ใช้ Chrome/CDP ในกรณีที่การเรียก API โดยตรงถูกป้องกันด้วยระบบของเว็บไซต์

ข้อมูลที่เก็บต่อ symbol มี 4 กลุ่มหลัก:

```text
*_financial_data.json
*_trading_stat.json
*_historical_trading.json
*_related_product.json
```

ตัวอย่างผลการทดสอบที่ผ่าน:

```text
Success : 869
Failed  : 0
```

## ข้อกำหนดสำคัญ

- ห้ามเปลี่ยน Price V4 logic โดยไม่จำเป็น
- Excel Macro 2 ต้องใช้ `fundamental_v4.csv`
- `fundamental_869.csv` ไม่ใช่ไฟล์หลักสำหรับ Macro 2
- `Data/` เป็นข้อมูล runtime และไม่ควร commit เข้า repository
- Backup/source backup files ไม่ควรถูกนำเข้า release build

## Git

Repository:

```text
Somyotthanimwas/FundamentalUpdater_rev5
```

Branch หลัก:

```text
main
```

## Release baseline

Release baseline ของ repository นี้คือชุดที่ผ่านการทดสอบ end-to-end ได้แก่:

```text
Price V4
  ↓
AmiBroker Price Import
  ↓
Fundamental V4 Capture
  ↓
FundamentalCsvConverter
  ↓
Excel V4 Macro 2
  ↓
Excel V4 Macro 1
  ↓
AmiBroker
  ↓
LINE notification
```

## License

โปรเจกต์นี้จัดทำเพื่อใช้งานภายใน Plaifa Engineering เว้นแต่จะมีการระบุเงื่อนไขการเผยแพร่เพิ่มเติมในภายหลัง.
