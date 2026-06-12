#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "heartRate.h"

// Configuración de la Pantalla OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Inicialización del Sensor GY-MAX30102
MAX30105 particleSensor;

// Configuración del Buzzer
#define BUZZER_PIN 5  // Cambia este número si soldaste o conectaste el buzzer a otro GPIO

// Variables optimizadas para estabilizar el cálculo de BPM
const byte RATE_SIZE = 6; // Mayor tamaño de búfer para suavizar la lectura y evitar saltos locos
byte rates[RATE_SIZE]; 
byte rateSpot = 0;
long lastBeat = 0; 
float beatsPerMinute;
int beatAvg = 0;

// Variables de control para el buzzer (No bloqueante)
unsigned long tiempoBuzzerEncendido = 0;
bool buzzerActivo = false;

// Definición de los Estados Emocionales de Lumina
enum EstadoLumina { DESCONECTADO, CALMO, AGITADO };
EstadoLumina estadoActual = DESCONECTADO;

unsigned long ultimoCambioTexto = 0;
int alternarAnimacion = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Inicializando a Lumina con Audio...");

  // Configurar pin del Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // 1. Inicializar Pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("Error: No se encontró la pantalla OLED"));
    for(;;); 
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // Pantalla de carga con sonido de encendido
  display.setTextSize(2);
  display.setCursor(20, 15);
  display.println("LUMINA v3");
  display.setTextSize(1);
  display.setCursor(20, 45);
  display.println("Buzzer Activo...");
  display.display();
  
  // Pequeña melodía feliz de inicio (Beep-Boop)
  tone(BUZZER_PIN, 1000, 100); delay(150);
  tone(BUZZER_PIN, 1500, 150); delay(150);

  // 2. Inicializar Sensor MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Error: No se encontró el sensor GY-MAX30102");
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Error: Sensor NO det.");
    display.display();
    for(;;);
  }

  particleSensor.setup(); 
  particleSensor.setPulseAmplitudeRed(0x0A); 
  particleSensor.setPulseAmplitudeIR(0x1F);  
}

void loop() {
  long irValue = particleSensor.getIR();

  // Si no hay un dedo detectado sobre el sensor
  if (irValue < 50000) {
    estadoActual = DESCONECTADO;
    beatAvg = 0;
  } else {
    // Si se detecta físicamente un latido real
    if (checkForBeat(irValue) == true) {
      long delta = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);

      // FILTRO DE SEGURIDAD: Solo promedia lecturas lógicas humanas para evitar saltos raros
      if (beatsPerMinute >= 45 && beatsPerMinute <= 145) {
        rates[rateSpot++] = (byte)beatsPerMinute; 
        rateSpot %= RATE_SIZE; 

        long total = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++) total += rates[x];
        beatAvg = total / RATE_SIZE; // Promedio estable
        
        // ACTIVAR BUZZER: Emitir sonido inmediato sin congelar el código
        buzzerActivo = true;
        tiempoBuzzerEncendido = millis();
        if (beatAvg > 100) {
          tone(BUZZER_PIN, 1800); // Tono agudo de alerta si está agitado
        } else {
          tone(BUZZER_PIN, 1200); // Tono normal y armónico si está calmado
        }
      }
    }

    // Determinar estados emocionales según el promedio verificado
    if (beatAvg == 0) {
      estadoActual = DESCONECTADO; 
    } else if (beatAvg > 100) {
      estadoActual = AGITADO;      
    } else {
      estadoActual = CALMO;        
    }
  }

  // Apagar el buzzer de forma automática pasados 40 milisegundos (Sonido rápido estilo radar)
  if (buzzerActivo && (millis() - tiempoBuzzerEncendido > 40)) {
    noTone(BUZZER_PIN);
    buzzerActivo = false;
  }

  // Control de velocidad de parpadeo de ojos
  if (millis() - ultimoCambioTexto > 500) {
    alternarAnimacion = !alternarAnimacion;
    ultimoCambioTexto = millis();
  }

  actualizarPantalla();
}

void actualizarPantalla() {
  display.clearDisplay();

  // ==========================================
  // SECCIÓN INFERIOR: Telemetría en TAMAÑO GRANDE
  // ==========================================
  display.setTextSize(2);
  display.setCursor(0, 48); 
  display.print("BPM:");
  
  if (beatAvg > 0) {
    display.setCursor(55, 48);
    display.print(beatAvg);
  } else if (estadoActual == DESCONECTADO && particleSensor.getIR() > 50000) {
    display.setTextSize(1);   
    display.setCursor(50, 52);
    display.print("...leyendo");
  } else {
    display.setCursor(55, 48);
    display.print("--");
  }

  // ==========================================
  // SECCIÓN SUPERIOR: Rostro Animado de Lumina
  // ==========================================
  switch (estadoActual) {
    case DESCONECTADO:
      display.setTextSize(1);
      display.setCursor(15, 0);
      display.println("Modo Standby");
      
      display.setTextSize(2);
      if (alternarAnimacion) {
        display.setCursor(45, 16); display.print("u");
        display.setCursor(75, 16); display.print("u");
      } else {
        display.setCursor(45, 18); display.print("-");
        display.setCursor(75, 18); display.print("-");
      }
      display.setCursor(61, 28); display.print("_");
      break;

    case CALMO:
      display.setTextSize(1);
      display.setCursor(15, 0);
      display.println("Lumina: Calmado");
      
      display.setTextSize(2);
      if (alternarAnimacion) {
        display.setCursor(45, 16); display.print("^");
        display.setCursor(75, 16); display.print("^");
      } else {
        display.setCursor(45, 16); display.print("o");
        display.setCursor(75, 16); display.print("o");
      }
      display.setCursor(61, 26); display.print(".");
      break;

    case AGITADO:
      display.setTextSize(1);
      display.setCursor(15, 0);
      display.println("!Respira profundo!");
      
      display.setTextSize(2);
      if (alternarAnimacion) {
        display.setCursor(45, 14); display.print("O");
        display.setCursor(75, 14); display.print("O");
        display.setCursor(61, 25); display.print("o");
      } else {
        display.setCursor(45, 16); display.print("0");
        display.setCursor(75, 16); display.print("0");
        display.setCursor(61, 27); display.print("_");
      }
      break;
  }

  display.display();
}
