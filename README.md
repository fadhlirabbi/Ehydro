# 🌱 Ehydro: Sistem Hidroponik Otomatis Berbasis ESP32 dengan Kontrol Lokal & Cloud

[![License](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/Platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Protocol](https://img.shields.io/badge/Protocol-MQTT%20%7C%20WebSocket-orange.svg)]()

**Ehydro** adalah solusi perangkat lunak lengkap (firmware) untuk sistem hidroponik otomatis berbasis **ESP32**. Proyek ini menggabungkan logika kontrol dosis nutrisi yang cerdas dengan kemampuan monitoring real-time melalui *Dashboard Web Internal* (Akses Lokal) dan integrasi *Cloud* melalui protokol **MQTT** (Akses Jarak Jauh).

***

## ✨ Fitur Utama

Ehydro dirancang untuk keandalan dan kemudahan penggunaan, memungkinkan Anda mengelola siklus tanam secara presisi.

| Kategori | Fitur | Detail Teknis |
| :--- | :--- | :--- |
| **Kontrol Dosis Otomatis** | **Penyeimbangan Nutrisi** | Secara otomatis mengontrol pompa untuk menyesuaikan kadar **pH** dan **PPM (TDS)** air berdasarkan batas ambang yang telah ditetapkan (min/max). |
| | **Kontrol Pompa Multi-Channel** | Mendukung 4 saluran relay yang terpisah untuk dosis pH Up, pH Down, Nutrisi A, dan Nutrisi B (Pin 15, 16, 17, 18). |
| | **Interval Dosis** | Menetapkan jeda (*mixing interval*) 60 detik setelah dosis untuk memastikan pencampuran air dan pembacaan sensor yang akurat. |
| **Monitoring & Kontrol** | **Dashboard Web Lokal** | Antarmuka web yang dilindungi kata sandi untuk monitoring dan kontrol perangkat via *browser* di jaringan lokal (menggunakan `AsyncWebServer` dan `WebSocket`). |
| | **Integrasi Cloud (MQTT)** | Mengirim data sensor secara berkala ke broker **MQTT** (`eec79f8e.ala.asia-southeast1.emqxsl.com:8883`) menggunakan koneksi aman (TLS/SSL). |
| **Keandalan Sistem** | **Sensor Warm-up Logic** | Mengimplementasikan periode jeda pemanasan sensor (30 detik) saat *cycle* baru dimulai untuk mendapatkan hasil pembacaan yang stabil. |
| | **TDS Buffer Fix (v7)** | Perbaikan untuk mencegah laporan error TDS palsu (`<=0`) hingga buffer sensor TDS terisi penuh. |
| | **WiFi Exception Handling** | Secara otomatis menghapus kredensial WiFi yang gagal dan masuk ke AP Mode jika koneksi buruk. |

***

## 🛠️ Persyaratan Hardware

Proyek ini membutuhkan mikrokontroler **ESP32** dan beberapa sensor/modul:

* **Mikrokontroler:** ESP32 Dev Module atau sejenisnya.
* **Sensor pH:** Modul sensor pH analog (terhubung ke pin **GPIO 4**).
* **Sensor TDS:** Modul sensor TDS analog (terhubung ke pin **GPIO 5**).
* **Sensor Suhu Air:** Sensor DS18B20 (terhubung ke pin **GPIO 20**).
* **Modul Relay 4 Channel:** Digunakan untuk mengontrol pompa, terhubung ke pin **GPIO 15, 16, 17, 18**.
    * Relay CH1: Pompa pH Up (GPIO 15)
    * Relay CH2: Pompa pH Down (GPIO 16)
    * Relay CH3: Pompa Nutrisi A (GPIO 17)
    * Relay CH4: Pompa Nutrisi B (GPIO 18)
* **Pompa Dosis Mini:** 4 unit.

***

## ⚙️ Instalasi dan Setup

### 1. Kebutuhan Library

Pastikan Anda telah menginstal semua library berikut di Arduino IDE Anda:

* `WiFi`, `WiFiClientSecure`, `time.h` (Built-in ESP32)
* `Preferences` (Built-in ESP32, untuk NVS)
* `PubSubClient`
* `ArduinoJson`
* `WiFiManager`
* `OneWire`, `DallasTemperature`
* `ESPAsyncWebServer`, `AsyncTCP`

### 2. Konfigurasi Awal

Anda dapat menyesuaikan beberapa parameter secara langsung di awal file `hidro.ino`:

| Variabel | Deskripsi | Nilai Default |
| :--- | :--- | :--- |
| `mqtt_server` | Alamat broker MQTT Anda. | `"eec79f8e.ala.asia-southeast1.emqxsl.com"` |
| `mqtt_port` | Port MQTT (8883 untuk TLS/SSL). | `8883` |
| `web_password` | Kata sandi untuk mengakses dashboard web lokal. | `"admin"` |
| `DURASI_PULSE_POMPA` | Durasi pulsa dosis (dalam milidetik). | `500` |
| `sensorWarmUpTime` | Jeda pemanasan sensor awal (dalam milidetik). | `30000` (30 detik) |

### 3. Upload dan Koneksi WiFi

1.  Upload kode `hidro.ino` ke board ESP32 Anda.
2.  Setelah restart, jika belum ada kredensial WiFi tersimpan, perangkat akan masuk ke **AP Mode (Mode Konfigurasi)**.
3.  Hubungkan ponsel/laptop Anda ke *Access Point* baru dengan nama: `Setup-Hidroponik-XXXXXX` (di mana XXXXXX adalah 6 digit terakhir dari Device ID).
4.  Ikuti petunjuk di portal yang muncul untuk memasukkan kredensial WiFi jaringan rumah Anda.

***

## 🖥️ Panduan Penggunaan

### 1. Dashboard Web Lokal

Setelah terhubung ke WiFi, akses Dashboard dengan mengetikkan alamat IP ESP32 Anda (terlihat di Serial Monitor) di browser.

* **Akses:** `http://[IP_Perangkat_Anda]`
* **Login:** Masukkan `web_password` yang telah dikonfigurasi (`admin` secara default).
* **Aksi:** Dari dashboard, Anda dapat **Mulai/Hentikan Cycle**, mengatur batas **Threshold**, atau bahkan **mengganti WiFi**.

### 2. Kalibrasi Sensor (Via Serial Monitor)

Kalibrasi harus dilakukan untuk akurasi sensor pH dan TDS. Hubungkan ESP32 Anda ke komputer dan gunakan Serial Monitor (Baud Rate 115200) dengan perintah-perintah berikut:

| Perintah Serial | Fungsi | Detail |
| :--- | :--- | :--- |
| `ph4` | Kalibrasi pH 4.0 | Ukur dan simpan nilai voltase untuk titik pH 4.0. |
| `ph7` | Kalibrasi pH 7.0 | Ukur dan simpan nilai voltase untuk titik pH 7.0. |
| `ph10` | Kalibrasi pH 10.0 | Ukur dan simpan nilai voltase untuk titik pH 10.0. |
| `tdsXXXX` | Kalibrasi TDS | Kalibrasi dengan larutan standar (misalnya `tds1382` untuk larutan 1382 ppm). |
| `status` | Cek Status | Menampilkan semua nilai kalibrasi dan batas ambang yang tersimpan. |
| `baca` | Baca Sensor | Memaksa pembacaan sensor dan menampilkannya di Serial Monitor. |

Semua nilai kalibrasi akan disimpan secara persisten di memori **NVS** ESP32.
