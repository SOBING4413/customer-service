# CS Input — PRIMAPER Customer Manager

Aplikasi desktop Windows untuk input dan pengelolaan data customer PRIMAPER,
ditulis dalam bahasa **C** (Win32 API murni) dengan penyimpanan data lokal
berformat **JSON**.

## Cara membuka & build (Visual Studio)

1. Buka file **`alif.sln`** dengan Visual Studio 2022 (atau versi lain yang
   mendukung toolset v143 / C11). Jika Visual Studio Anda memakai toolset
   lain, klik kanan project → *Retarget Projects*.
2. Pilih konfigurasi **Debug** atau **Release**, platform **x64** disarankan.
3. Tekan **F5** (Run) atau **Ctrl+Shift+B** (Build).
4. Hasil build ada di `bin\x64\Debug\CS_Input.exe` (atau `Release`), lengkap
   dengan folder `data\` yang otomatis disalin oleh post-build event.

Tidak ada dependensi eksternal yang perlu diinstal — satu-satunya pustaka
pihak ketiga adalah **cJSON** (MIT License, `src/cJSON.c` & `src/cJSON.h`,
sudah disertakan dalam project).

## Login

Aplikasi memakai sistem login lokal berbasis `data/users.json`.

| Username     | Password           |
|--------------|---------------------|
| `admin`  | `admin123`  |

Anda dapat menambah/mengubah akun dengan mengedit `data/users.json`
(format sederhana: `username`, `password`, `nama`, `role`).

> Catatan: penyimpanan password di sini masih plain text di dalam file lokal —
> cukup untuk kebutuhan alat internal single-user. Untuk kebutuhan produksi
> multi-user, sebaiknya tambahkan hashing password.

## Struktur project

```
CS_Input/
├─ alif.sln                  # Solution Visual Studio
├─ CS_Input.vcxproj          # Project C (Win32)
├─ CS_Input.vcxproj.filters
├─ src/
│  ├─ main.c                 # Entry point (WinMain) + alur login/logout
│  ├─ app_data.h / .c        # Model data customer, load/save JSON, CRUD, statistik
│  ├─ theme.h / .c           # Palet warna, font, komponen UI flat modern
│  ├─ ui_login.c             # Layar login
│  ├─ ui_main.c              # Shell utama: sidebar, dashboard, daftar customer, analisis, about
│  ├─ ui_forms.c             # Form tambah/ubah, detail, konfirmasi
│  ├─ resource.h             # ID kontrol
│  ├─ app.rc                 # Resource (ikon + manifest)
│  ├─ cJSON.c / cJSON.h      # Pustaka JSON pihak ketiga (MIT License)
├─ res/
│  ├─ app.ico
│  └─ app.manifest           # Common Controls v6 + DPI aware
└─ data/
   ├─ customers.json         # Data awal (hasil import dari PRIMAPER_LIST_CUSTOMER.xlsx)
   └─ users.json             # Akun login
```

## Fitur utama

- **Login lokal** berbasis JSON.
- **Dashboard** ringkas: total customer, customer per tahun, pertumbuhan,
  tren bulanan, top wilayah cabang, top sales.
- **Data Customer**: tabel (ListView) dengan pencarian/filter real-time di
  semua kolom, tambah, ubah, hapus (dengan konfirmasi), dan lihat detail
  (klik ganda pada baris atau tombol Detail).
- **Analisis & Tren**: grafik batang tren bulanan Jun 2025 – Sep 2026,
  ringkasan statistik, dan proyeksi kasar untuk periode berikutnya.
- **Penyimpanan otomatis**: setiap tambah/ubah/hapus langsung ditulis ke
  `data/customers.json` (dengan pola *write-to-temp-then-replace* agar file
  tidak korup jika aplikasi ditutup paksa).
- **UI modern flat 2026-style**: sidebar navy + aksen indigo, kartu
  (card) putih dengan sudut membulat, tipografi Segoe UI.

## Field data customer

Mengikuti struktur data asli (`PRIMAPER_LIST_CUSTOMER.xlsx`):

- Wilayah Customer
- Wilayah Cabang
- Sales / Kepala Operasional
- Tanggal (format `YYYY-MM-DD`)
- Nomor Customer (nomor HP)
- Catatan (field tambahan, opsional, untuk kebutuhan mendatang)

## Import data awal

`data/customers.json` sudah berisi 90 data customer hasil impor dan
normalisasi dari `PRIMAPER_LIST_CUSTOMER.xlsx` (sheet 2025 & 2026), dengan
tanggal yang sudah dikonversi ke format standar `YYYY-MM-DD`.
