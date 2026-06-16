# AtmoBadge

**Sistem Peringatan Dini Prediktif dan Pemantau Mikroklimat Ruangan Berbasis ESP32-C3 Mini**

AtmoBadge adalah perangkat *embedded system* berbasis ESP32-C3 Mini yang memantau kualitas udara dan kondisi mikroklimat ruangan secara *real-time*. Alat ini mampu memprediksi kenaikan kadar gas polutan menggunakan Regresi Linear Sederhana, mengirim notifikasi ke Telegram, dan menampilkan ekspresi wajah piksel art di layar OLED sebagai indikator visual kondisi udara.

---

## Fitur Utama

- **Pemantauan Real-Time**: Membaca kadar gas polutan (PPM) via sensor MQ135 dan suhu/kelembaban via DHT11 setiap 2 detik
- **Prediksi Tren Gas (5 Menit ke Depan)**: Implementasi algoritma Regresi Linear Sederhana berbasis 60 data historis terakhir untuk memprojeksikan `predictedPPM`
- **Peringatan Dini Otomatis**: Alarm buzzer dan notifikasi Telegram Bot API aktif secara otomatis ketika PPM ≥ 700 (kategori AQI Sedang)
- **Ekspresi Wajah Piksel Art**: Layar OLED menampilkan karakter animasi `FACE_SMILE`, `FACE_HOT`, atau `FACE_MASK` sesuai kondisi udara
- **IoT & Cloud Integration**: Sinkronisasi data cuaca luar ruangan via OpenWeatherMap API dan pengiriman laporan telemetri via Telegram Bot API (SSL/TLS)
- **Auto Sleep Mode**: Layar beralih ke tampilan AOD (Always-On Display) minimalis setelah 25 detik tanpa interaksi untuk menghemat daya
- **Kalibrasi Sensor Mandiri**: Rutinitas `runAutoCalibration()` dapat dipicu via 5x ketukan tombol; nilai R₀ tersimpan permanen di flash memory menggunakan `Preferences`
- **Minigame Flappy Bird**: Mode interaktif yang juga berfungsi sebagai *stress test* sistem pada OLED 128x64

---

## Hardware

| Komponen | Spesifikasi | Pin ESP32-C3 |
|---|---|---|
| Mikrokontroler | ESP32-C3 Mini (RISC-V 32-bit) | — |
| Sensor Gas | MQ135 (Analog) | GPIO 2 (ADC) |
| Sensor Suhu & Kelembaban | DHT11 (Single-Wire) | GPIO 3 |
| Layar | OLED 1.3" SH1106 128x64 (I2C) | SDA: GPIO 5, SCL: GPIO 4 |
| Buzzer | Buzzer Pasif (LEDC PWM) | GPIO 10 |
| Tombol | Push Button (Internal Pull-Up) | GPIO 1 |
| Baterai | Li-Po 3.7V | — |
| Charger | TP4056 | — |
| Step-Up | MT3608 (3.7V → 5V) | — |

>  **Catatan Hardware:** Pin GPIO 12–17 pada ESP32-C3 Mini digunakan secara internal untuk bus SPI Flash dan **tidak boleh digunakan** untuk periferal eksternal, karena dapat menyebabkan kegagalan booting total.

---

## Arsitektur Perangkat Lunak

Sistem menggunakan model komputasi **hibrida** yang menggabungkan FSM, FreeRTOS, dan Event-Driven Programming.

```
┌─────────────────────────────────────────────────┐
│                  Arduino loop()                 │
│  FSM: BOOTING → IDLE_SENSING → ALERT_MODE       │
│       ↕ GAME_MODE (4x klik)                     │
│  - Sampling MQ135 & DHT11                       │
│  - Render OLED & animasi wajah                  │
│  - Game Flappy Bird                             │
└──────────────────┬──────────────────────────────┘
                   │ FreeRTOS
        ┌──────────┴───────────┐
        │    networkTask       │  ← Permanent background task
        │  - WiFi reconnect    │
        │  - NTP sync          │
        │  - OpenWeatherMap    │
        │  - Telegram Bot API  │
        └──────────────────────┘
        ┌──────────────────────┐
        │    marioTask         │  ← Transient (booting only)
        │  - Melodi Super Mario│
        │  - vTaskDelete(NULL) │
        └──────────────────────┘
```

### Finite State Machine (FSM)

| State | Deskripsi |
|---|---|
| `BOOTING` | Inisialisasi hardware, WiFi, NTP, dan melodi pembuka |
| `IDLE_SENSING` | Operasi normal: sampling sensor, tampil animasi wajah |
| `ALERT_MODE` | PPM ≥ 700: alarm buzzer aktif, wajah masker, kirim Telegram |
| `GAME_MODE` | Minigame Flappy Bird (dipicu 4x klik tombol) |
| `CHECK_MODE` | *Reserved* untuk fitur diagnostik mendatang |

---

## Cara Kerja Prediksi Gas

Sistem mengumpulkan 60 data historis ke dalam `ppmBuffer` dan `timeBuffer`, lalu menghitung gradien tren (m) dan intersep (c) menggunakan rumus **Regresi Linear Sederhana**:

```
predictedPPM = m × (currentTime + 300) + c
```

Nilai `predictedPPM` (proyeksi 5 menit ke depan) ditampilkan langsung di halaman data layar OLED sebagai peringatan preventif sebelum udara ruangan benar-benar berbahaya.

---

## Interaksi Tombol Fisik

| Aksi | Fungsi |
|---|---|
| Single click | Ganti halaman tampilan OLED |
| Double click | Kirim laporan telemetri instan ke Telegram |
| 4x klik | Masuk ke GAME_MODE (Flappy Bird) |
| 5x klik | Jalankan rutinitas kalibrasi sensor (`runAutoCalibration()`) |
| Long press | Kembali ke IDLE_SENSING dari mode apapun |

---

## Dependencies

Tambahkan library berikut melalui Arduino IDE Library Manager:

- `U8g2`: Rendering grafis OLED SH1106
- `DHT sensor library`: Baca sensor DHT11
- `OneButton`: Manajemen event ketukan tombol
- `ArduinoJson`: Parse response JSON API
- `Preferences` *(bawaan ESP32 core)*: Penyimpanan data kalibrasi ke flash
- `WiFiClientSecure` *(bawaan ESP32 core)*: Koneksi HTTPS SSL/TLS
- `FreeRTOS` *(bawaan ESP32 core)*: Multitasking

---

## Konfigurasi

Sebelum upload, sesuaikan konstanta berikut di file kode utama:

```cpp
// Kredensial WiFi
const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// Telegram Bot
const char* BOT_TOKEN  = "YOUR_BOT_TOKEN";
const char* CHAT_ID    = "YOUR_CHAT_ID";

// OpenWeatherMap
const char* OWM_API_KEY = "YOUR_OWM_KEY";
const char* OWM_CITY    = "Malang";        // Nama kota lokasi perangkat
```

---

## Upload ke ESP32-C3 Mini

1. Install **Arduino IDE** dan tambahkan board ESP32 via Board Manager
2. Pilih board: `ESP32C3 Dev Module`
3. Set **USB CDC On Boot: Enabled** (untuk Serial Monitor)
4. Install semua dependency di atas
5. Isi konfigurasi WiFi, Telegram, dan OWM
6. Upload dan buka Serial Monitor (115200 baud)

---

## Kronologi Pengembangan

| Fase | Deskripsi |
|---|---|
| **Fase 1** | Prototipe pada breadboard dengan ESP32 DevKit + sensor AHT20xBMP280 |
| **Fase 2** | Migrasi ke PCB bolong, integrasi modul TP4056 + MT3608, kemas ke enclosure |
| **Fase 3** | Troubleshooting: ganti sensor ke DHT11, ganti MCU ke ESP32-C3 Mini, ganti display ke OLED SH1106 |

---

## Kode & Dokumentasi

- 📄 [Kode Program AtmoBadge](https://drive.google.com/your-link-here)
- 🎬 [Video Perkembangan AtmoBadge](https://drive.google.com/your-link-here)

---

## Tim Pengembang

| Nama | NIM |
|---|---|
| Purnama Ramadhansyah | 245150301111018 |
| Stefanus Evan Anggawardhana | 245150307111039 |
| Muhammad Dzakwan Ikram | 245150301111019 |
| Lutfi Maulana | 245150301111021 |
| Akhtar Kael | 245150300111036 |
