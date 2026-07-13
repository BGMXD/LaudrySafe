#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// --- Konfigurasi Pin ---
#define VIB_PIN 15       // Pin Sensor Getaran SW420 terhubung ke D15
#define DHTPIN 4         // Pin Sensor Suhu DHT22 terhubung ke D4 
#define DHTTYPE DHT22    // Tipe Sensor DHT (DHT22)

// --- Konfigurasi OLED ---
#define SCREEN_WIDTH 128 // Lebar OLED dalam pixel
#define SCREEN_HEIGHT 32 // Tinggi OLED 0.91 inch dalam pixel
#define OLED_RESET    -1 // Reset pin (tidak digunakan pada modul I2C standar)

// Inisialisasi objek OLED dan DHT
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);

// --- Variabel Baru untuk Logika Debounce 2 Arah ---
int hitungAnomali = 0;        // Penghitung berapa kali sensor mendeteksi getaran (1)
int hitungAman = 0;           // Penghitung berapa kali sensor mendeteksi diam (0)
bool statusAnomali = false;   // Status akhir yang akan ditampilkan di layar

void setup() {
  Serial.begin(115200);

  // Inisialisasi Pin Sensor Getaran
  pinMode(VIB_PIN, INPUT);

  // Inisialisasi Sensor DHT
  dht.begin();

  // Inisialisasi Komunikasi I2C untuk OLED (Pin D21 SDA, D22 SCL)
  Wire.begin();

  // Inisialisasi Layar OLED (Alamat I2C umumnya 0x3C)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Gagal inisialisasi OLED!"));
    for(;;); // Menghentikan program jika OLED tidak terdeteksi
  }

  // Tampilan Awal OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print("Sistem Memulai...");
  display.display();
  delay(2000); // Jeda 2 detik
}

void loop() {
  // 1. Membaca Data Suhu dari DHT22
  float suhu = dht.readTemperature();
  
  // 2. Membaca Status Getaran dari SW420
  // (Asumsi standar: HIGH/1 = Ada Getaran, LOW/0 = Diam/Aman)
  int statusGetaran = digitalRead(VIB_PIN);

  // --- LOGIKA FILTER GETARAN (DEBOUNCE 2 ARAH) ---
  if (statusGetaran == HIGH) {
    hitungAnomali++;     // Tambah terus poin anomali
    hitungAman = 0;      // Reset poin aman menjadi 0

    if (hitungAnomali >= 5) {
      statusAnomali = true; // Ubah layar ke Anomali setelah 5x beruntun
      hitungAnomali = 5;    // Tahan angka di 5 agar tidak membebani memori (overflow)
    }
  } 
  else { 
    // Jika sensor mendeteksi LOW (0)
    hitungAman++;        // Tambah terus poin aman
    hitungAnomali = 0;   // Reset poin anomali menjadi 0

    if (hitungAman >= 7) {
      statusAnomali = false; // Ubah layar ke Aman setelah 7x beruntun
      hitungAman = 7;        // Tahan angka di 7 agar tidak membebani memori
    }
  }

  // Validasi pembacaan DHT22 (mencegah error jika sensor terputus)
  if (isnan(suhu)) {
    Serial.println("Gagal membaca sensor DHT!");
    suhu = 0.0; // Mengubah nilai menjadi 0 jika error
  }

  // 3. Menampilkan Data ke Serial Monitor
  Serial.print("Suhu: "); 
  Serial.print(suhu); 
  Serial.print(" *C | Getaran: ");
  
  // Menampilkan ke Serial berdasarkan variabel filter
  if (statusAnomali == true) {
    Serial.println("TERDETEKSI! (Anomali)");
  } else {
    Serial.println("Normal (Aman)");
  }

  // 4. Menampilkan Data ke Layar OLED
  display.clearDisplay();
  
  // Baris 1: Menampilkan Suhu
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Suhu Mesin: ");
  display.print(suhu, 1); 
  display.print(" C");

  // Baris 2: Menampilkan Status Getaran
  display.setCursor(0, 16);
  display.print("Getaran   : ");
  
  // Menampilkan ke OLED berdasarkan variabel filter
  if (statusAnomali == true) {
    display.print("ANOMALI!!!!");
  } else {
    display.print("Aman");
  }

  display.display(); 

  // Jeda 100ms sebelum sistem melakukan pembacaan ulang
  delay(100); 
}