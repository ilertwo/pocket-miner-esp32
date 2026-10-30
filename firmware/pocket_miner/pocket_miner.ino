#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdlib.h>

// Налаштування екрану (128x64)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Ініціалізація дисплея
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Піни
const int JOY_X = 4;
const int JOY_Y = 5;
const int BUZZER_PIN = 7;
const int BTN_JUMP = 15;
const int BTN_DIG = 16;

// I2C піни для ESP32-S3
const int I2C_SDA = 8;
const int I2C_SCL = 9;

//**************************************************************
// БІТМАПИ
//**************************************************************

const unsigned char PROGMEM dirt [] = { 0x9b, 0xff, 0xef, 0xd9, 0xfd, 0xaf, 0xa5, 0xff };
const unsigned char PROGMEM stone [] = { 0xff, 0xf3, 0xcf, 0xb9, 0xe7, 0x9d, 0xf3, 0xff };
const unsigned char PROGMEM ore [] = { 0xff, 0xc9, 0xcd, 0xd7, 0x81, 0xb5, 0x99, 0xff };
const unsigned char PROGMEM bedrock [] = { 0x5a, 0xff, 0x5a, 0xe7, 0xe7, 0x5a, 0xff, 0x5a };
const unsigned char PROGMEM heart [] = { 0x00, 0x3c, 0x42, 0x4e, 0x4e, 0x2c, 0x18, 0x00 };

const unsigned char PROGMEM sprite_player [] = { 0x00, 0x18, 0x12, 0x1b, 0x7d, 0x58, 0x18, 0x14 };
const unsigned char PROGMEM sprite_player_frame_0[] = { 0x00, 0x08, 0x10, 0x18, 0x38, 0x70, 0x30, 0x50 };
const unsigned char PROGMEM sprite_player_frame_1[] = { 0x00, 0x08, 0x10, 0x18, 0x38, 0x70, 0x30, 0x28 };

//**************************************************************
// СВІТ
//**************************************************************

const int MAP_WIDTH = 16;
const int MAP_HEIGHT = 8;
byte world[MAP_HEIGHT][MAP_WIDTH] = {0};

void createWorld() {
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      byte tile = 0;
      if (y == 0 || y == 1) {
        tile = 0;
      } else if (y == 7) {
        tile = 4;
      } else {
        int random_value = rand() % 100; 
        if (y == 2 || y == 3 || y == 4) {
          if(random_value < 50)      tile = 1;
          else if(random_value < 70) tile = 0;
          else                       tile = 2;
        } else {
          if(random_value < 70)      tile = 2;
          else if(random_value < 80) tile = 1;
          else                       tile = 3;
        }
      }
      world[y][x] = tile; 
    }
  }
}

void drawWorld() {
  for (int y = 0; y < MAP_HEIGHT; y++) {
    for (int x = 0; x < MAP_WIDTH; x++) {
      int tileID = world[y][x];
      switch(tileID) {
        case 1: display.drawBitmap(x * 8, y * 8, dirt, 8, 8, WHITE); break;
        case 2: display.drawBitmap(x * 8, y * 8, stone, 8, 8, WHITE); break;
        case 3: display.drawBitmap(x * 8, y * 8, ore, 8, 8, WHITE); break;
        case 4: display.drawBitmap(x * 8, y * 8, bedrock, 8, 8, WHITE); break;
      }
    }
  }
}

//**************************************************************
// ФІЗИКА ТА ГРАВЕЦЬ
//**************************************************************

float playerX = 64.0;
float playerY = 10.0;
float velocityY = 0.0;
float gravity = 0.5;
float playerSpeed = 2.0; 

void applyPhysics() {
  velocityY += gravity;
  float nextY = playerY + velocityY;

  if (velocityY > 0) {
    int leftGridX = (int)(playerX) / 8;
    int rightGridX = (int)(playerX + 7) / 8;
    
    int bottomGridY = (int)(nextY + 8) / 8; 

    if (bottomGridY < MAP_HEIGHT) {
      if (world[bottomGridY][leftGridX] != 0 || world[bottomGridY][rightGridX] != 0) {
        nextY = (bottomGridY * 8) - 8; 
        velocityY = 0.0;      }
    }
  }

  playerY = nextY;

  if (playerY >= SCREEN_HEIGHT - 8) {
    playerY = SCREEN_HEIGHT - 8;
    velocityY = 0.0;
  }
}

void jump() {
  if (velocityY == 0) {
    velocityY = -5.0;
  }
}

void drawPlayer() {
  if (abs(playerX - 64.0) > 0.1) {
    int frame = (int)(playerX / 4) % 2; 
    if (frame == 0) {
      display.drawBitmap(playerX, playerY, sprite_player_frame_0, 8, 8, WHITE);
    } else {
      display.drawBitmap(playerX, playerY, sprite_player_frame_1, 8, 8, WHITE);
    }
  } else {
    display.drawBitmap(playerX, playerY, sprite_player, 8, 8, WHITE);
  }
}

//**************************************************************
// УПРАВЛІННЯ ТА КОПАННЯ
//**************************************************************

void dig(int dirX, int dirY) {
  int currentGridX = (playerX + 4) / 8;
  int currentGridY = (playerY + 4) / 8;
  int targetX = currentGridX + dirX;
  int targetY = currentGridY + dirY;

  if (targetX >= 0 && targetX < MAP_WIDTH && targetY >= 0 && targetY < MAP_HEIGHT) {
    if (world[targetY][targetX] != 0 && world[targetY][targetX] != 4) { 
      world[targetY][targetX] = 0; 
      tone(BUZZER_PIN, 800, 50); 
    }
  }
}

void handleInput() {
  int xValue = analogRead(JOY_X);
  int yValue = analogRead(JOY_Y);
  
  float dx = 0;
  
  if (xValue < 1500) dx = -playerSpeed; 
  else if (xValue > 2500) dx = playerSpeed; 

  // ГОРИЗОНТАЛЬНА КОЛІЗІЯ
  if (dx != 0) {
    float nextX = playerX + dx;

    int topGridY = (int)(playerY + 1) / 8;
    int bottomGridY = (int)(playerY + 7) / 8;
    
    bool hitWall = false;

    if (dx < 0) { 
      int leftGridX = (int)nextX / 8;
      if (leftGridX >= 0) {
        if (world[topGridY][leftGridX] != 0 || world[bottomGridY][leftGridX] != 0) {
          playerX = (leftGridX * 8) + 8;
          hitWall = true;
        }
      }
    } 
    else if (dx > 0) { 
      int rightGridX = (int)(nextX + 7) / 8;
      if (rightGridX < MAP_WIDTH) {
        if (world[topGridY][rightGridX] != 0 || world[bottomGridY][rightGridX] != 0) {
          playerX = (rightGridX * 8) - 8;
          hitWall = true;
        }
      }
    }

    if (nextX < 0) { playerX = 0; hitWall = true; }
    if (nextX > SCREEN_WIDTH - 8) { playerX = SCREEN_WIDTH - 8; hitWall = true; }

    if (!hitWall) {
      playerX = nextX;
    }
  }

  // Стрибок
  if (digitalRead(BTN_JUMP) == LOW) {
    jump();
  }

  // Копання
  if (digitalRead(BTN_DIG) == LOW) {
    if (yValue > 2500) dig(0, 1);        // Вниз
    else if (yValue < 1500) dig(0, -1);  // Вгору
    else if (xValue < 1500) dig(-1, 0);  // Вліво
    else if (xValue > 2500) dig(1, 0);   // Вправо
    delay(150);
  }
}

//**************************************************************
// ГОЛОВНИЙ ЦИКЛ ПРОГРАМИ ESP32
//**************************************************************

void setup() {
  Serial.begin(115200);

  pinMode(BTN_JUMP, INPUT_PULLUP);
  pinMode(BTN_DIG, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(I2C_SDA, I2C_SCL);

  // Ініціалізація дисплея
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("Помилка SSD1306"));
    for(;;);
  }
  display.setTextColor(WHITE);
  display.clearDisplay();

  createWorld();
}

void loop() {
  // 1. Очистка екрану
  display.clearDisplay();

  // 2. Обробка логіки
  handleInput();
  applyPhysics();

  // 3. Відмальовка
  drawWorld();
  drawPlayer();

  // 4. Відправка буфера на екран
  display.display();

  // 5. Контроль частоти кадрів
  delay(30);
}