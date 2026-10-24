#include <WiFi.h>
#include <WebSocketMCP.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//======================================================
// CONFIGURAÇÃO DO DISPLAY OLED (I2C nos pinos SDA=8, SCL=9)
//======================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variáveis para controle do piscar dos olhos
unsigned long ultimoPiscar = 0;
int intervaloPiscar = 3000;
bool piscando = false;
unsigned long tempoInicioPiscar = 0;
int duracaoPiscar = 150; 
int tipoOlho = 0; 

//======================================================
// SERVOS (Pinos adaptados para o ESP32-C3)
//======================================================

#define SERVO_HEAD        0
#define SERVO_LEFT_ARM    7
#define SERVO_RIGHT_ARM   3

#define PERNA_ESQUERDA    6  // Movido para 10 para liberar o GPIO 8 do OLED
#define PERNA_DIREITA     5   // Movido para 3 (ou use outro pino livre se preferir)
#define SERVO_PERna       0

Servo servoPerna;
Servo servoHead;
Servo servoLeftArm;
Servo servoRightArm;

//======================================================
// SENSOR DE SOM KY-038
//======================================================

#define PINO_MICROFONE 2  // Ajustado para um pino livre (GPIO 2)

//======================================================
// WIFI
//======================================================

const char* ssid = "RAMOS";
const char* password = "012027AENJ";

//======================================================
// MCP
//======================================================

const char* mcpEndpoint = "wss://api.xiaozhi.me/mcp/?token=eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VySWQiOjk3OTAzNiwiYWdlbnRJZCI6MjE1Nzg0MywiZW5kcG9pbnRJZCI6ImFnZW50XzIxNTc4NDMiLCJwdXJwb3NlIjoibWNwLWVuZHBvaW50IiwiaWF0IjoxNzg0ODQyNzQ4LCJleHAiOjE4MTY0MDAzNDh9.xxiIArsnsBTpaUbbVn92RxcgZqsbhuS0VBOaqKqNMzekeWQQ2pZW-wuP4GPIi_S20gKZOp5Rh013_AC2eN623g";

WebSocketMCP mcpClient;

unsigned long ultimoCheckWiFi = 0;
bool mcpIniciado = false;

//======================================================
// CONTROLE DOS MOVIMENTOS
//======================================================

bool movimentoAtivo = false;
unsigned long ultimoMovimento = 0;
unsigned long ultimoSom = 0;
int intervaloMovimento = 1750;

//======================================================
// CONTROLE DAS PERNAS
//======================================================

bool andando = false;
bool passoEsquerdo = true;
unsigned long ultimoPasso = 0;
bool esperando = false;

const int TEMPO_PASSO = 400;

//======================================================
// DESENHO DOS OLHOS NO OLED
//======================================================

void desenharOlhos() {
    display.clearDisplay();

    if (piscando) {
        // Olhos fechados (linha horizontal)
        display.fillRect(26, 32, 24, 4, SSD1306_WHITE);
        display.fillRect(78, 32, 24, 4, SSD1306_WHITE);
    } else {
        switch (tipoOlho) {
            case 0: // Normal (Retângulos arredondados limpos)
                display.fillRoundRect(26, 18, 24, 28, 6, SSD1306_WHITE);
                display.fillRoundRect(78, 18, 24, 28, 6, SSD1306_WHITE);
                break;

            case 1: // Feliz (Arcos limpos)
                display.fillTriangle(26, 36, 38, 20, 50, 36, SSD1306_WHITE);
                display.fillRect(26, 34, 24, 4, SSD1306_BLACK);
                
                display.fillTriangle(78, 36, 90, 20, 102, 36, SSD1306_WHITE);
                display.fillRect(78, 34, 24, 4, SSD1306_BLACK);
                break;

            case 2: // Triste
                display.fillTriangle(26, 20, 50, 26, 50, 38, SSD1306_WHITE);
                display.fillTriangle(78, 26, 102, 20, 78, 38, SSD1306_WHITE);
                break;

            case 3: // Bravo
                display.fillRoundRect(26, 22, 24, 24, 4, SSD1306_WHITE);
                display.fillTriangle(20, 16, 54, 26, 20, 26, SSD1306_BLACK);

                display.fillRoundRect(78, 22, 24, 24, 4, SSD1306_WHITE);
                display.fillTriangle(108, 16, 74, 26, 108, 26, SSD1306_BLACK);
                break;

            case 4: // Surpreso (Círculos limpos)
                display.fillCircle(38, 32, 12, SSD1306_WHITE);
                display.fillCircle(90, 32, 12, SSD1306_WHITE);
                break;
        }
    }
    display.display();
}

void atualizarPiscar() {
    unsigned long agora = millis();

    if (!piscando && (agora - ultimoPiscar > intervaloPiscar)) {
        piscando = true;
        tempoInicioPiscar = agora;
        intervaloPiscar = random(2000, 5000);
    }

    if (piscando && (agora - tempoInicioPiscar > duracaoPiscar)) {
        piscando = false;
        ultimoPiscar = agora;
    }

    desenharOlhos();
}

//======================================================
// POSIÇÕES DOS SERVOS
//======================================================

void posicaoNeutra()
{
    servoHead.write(90);
    servoLeftArm.write(90);
    servoRightArm.write(90);
    tipoOlho = 0;
}

//======================================================
// DETECTA QUANDO O XIAOZHI ESTÁ FALANDO
//======================================================

void detectarFala()
{
    int som = analogRead(PINO_MICROFONE);

    if (som > 300)
    {
        movimentoAtivo = true;
        ultimoSom = millis();
    }

    if (millis() - ultimoSom > 800)
    {
        movimentoAtivo = false;
        servoHead.write(90);
        servoLeftArm.write(90);
        servoRightArm.write(90);
    }
}

//======================================================
// MOVIMENTOS ENQUANTO FALA
//======================================================

void moverServosAleatoriamente()
{
    if (!movimentoAtivo)
        return;

    if (millis() - ultimoMovimento < intervaloMovimento)
        return;

    ultimoMovimento = millis();
    intervaloMovimento = random(1000, 1800);

    servoHead.write(random(70, 110));
    servoLeftArm.write(random(55, 95));
    servoRightArm.write(random(95, 135));
}

//======================================================
// MOVIMENTO DAS PERNAS
//======================================================

void iniciarAndar()
{
    andando = true;
    esperando = false;
    passoEsquerdo = true;
    ultimoPasso = millis();
}

void pararAndar()
{
    andando = false;
    digitalWrite(PERNA_ESQUERDA, LOW);
    digitalWrite(PERNA_DIREITA, LOW);
}

void atualizarAndar()
{
    if (!andando)
        return;

    unsigned long agora = millis();

    if (agora - ultimoPasso < TEMPO_PASSO)
        return;

    ultimoPasso = agora;

    if (passoEsquerdo)
    {
        digitalWrite(PERNA_ESQUERDA, HIGH);
        digitalWrite(PERNA_DIREITA, LOW);
        servoPerna.write(120);
    }
    else
    {
        digitalWrite(PERNA_ESQUERDA, LOW);
        digitalWrite(PERNA_DIREITA, HIGH);
        servoPerna.write(40);
    }

    passoEsquerdo = !passoEsquerdo;
}

//======================================================
// EMOÇÕES
//======================================================

void emocaoFeliz()
{
    movimentoAtivo = false;
    tipoOlho = 1;
    servoHead.write(90);

    for (int i = 0; i < 3; i++)
    {
        servoLeftArm.write(45);
        servoRightArm.write(135);
        delay(250);

        servoHead.write(80);
        delay(80);

        servoLeftArm.write(70);
        servoRightArm.write(110);
        delay(250);

        servoHead.write(100);
        delay(80);
    }

    posicaoNeutra();
}

void emocaoTriste()
{
    movimentoAtivo = false;
    tipoOlho = 2;
    servoHead.write(75);
    servoLeftArm.write(110);
    servoRightArm.write(70);
}

void emocaoBravo()
{
    movimentoAtivo = false;
    tipoOlho = 3;
    servoHead.write(95);
    servoLeftArm.write(60);
    servoRightArm.write(120);
}

void emocaoSurpreso()
{
    movimentoAtivo = false;
    tipoOlho = 4;
    servoHead.write(100);
    servoLeftArm.write(20);
    servoRightArm.write(160);
}

void dancar()
{
    pararAndar();
    tipoOlho = 1;

    for (int i = 0; i < 6; i++)
    {
        servoHead.write(75);
        servoLeftArm.write(45);
        servoRightArm.write(135);

        digitalWrite(PERNA_ESQUERDA, HIGH);
        digitalWrite(PERNA_DIREITA, LOW);
        delay(350);

        digitalWrite(PERNA_ESQUERDA, LOW);
        delay(150);

        servoHead.write(105);
        servoLeftArm.write(70);
        servoRightArm.write(110);

        digitalWrite(PERNA_DIREITA, HIGH);
        delay(350);

        digitalWrite(PERNA_DIREITA, LOW);
        delay(150);
    }

    posicaoNeutra();
    digitalWrite(PERNA_ESQUERDA, LOW);
    digitalWrite(PERNA_DIREITA, LOW);
}

//======================================================
// REGISTRO DAS TOOLS MCP
//======================================================

void registerMcpTools()
{
    mcpClient.registerTool("robot_andar", "Faz o robô começar a andar.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        iniciarAndar();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });

    mcpClient.registerTool("robot_parar", "Faz o robô parar.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        pararAndar();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });

    mcpClient.registerTool("robot_dancar", "Faz o robô dançar.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        dancar();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });

    mcpClient.registerTool("robot_servos_feliz", "Mostra que o robô está feliz.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        emocaoFeliz();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });

    mcpClient.registerTool("robot_servos_triste", "Mostra que o robô está triste.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        emocaoTriste();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });

    mcpClient.registerTool("robot_servos_bravo", "Mostra que o robô está bravo.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        emocaoBravo();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });

    mcpClient.registerTool("robot_servos_surpreso", "Mostra surpresa.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        emocaoSurpreso();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });

    mcpClient.registerTool("robot_servos_neutro", "Volta para posição neutra.", "{\"type\":\"object\",\"properties\":{}}", [](const String& args) {
        posicaoNeutra();
        return WebSocketMCP::ToolResponse("{\"success\":true}");
    });
}

//======================================================
// STATUS DA CONEXÃO
//======================================================

void onConnectionStatus(bool connected)
{
    if (connected)
    {
        Serial.println("\nMCP CONECTADO");
        registerMcpTools();
    }
    else
    {
        Serial.println("\nMCP DESCONECTADO");
    }
}

//======================================================
// SETUP
//======================================================

void setup()
{
    Serial.begin(115200);

    // Inicializa o I2C nos pinos solicitados: SDA=GPIO 8, SCL=GPIO 9
    Wire.begin(8, 9);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("Falha ao encontrar o Display OLED SSD1306!"));
        for(;;);
    }
    display.clearDisplay();
    display.display();

    servoHead.attach(SERVO_HEAD);
    servoLeftArm.attach(SERVO_LEFT_ARM);
    servoRightArm.attach(SERVO_RIGHT_ARM);
    
    pinMode(PERNA_ESQUERDA, OUTPUT);
    pinMode(PERNA_DIREITA, OUTPUT);
    digitalWrite(PERNA_ESQUERDA, LOW);
    digitalWrite(PERNA_DIREITA, LOW);

    servoPerna.attach(SERVO_PERna);
    servoPerna.write(40);
    
    posicaoNeutra();

    pinMode(PINO_MICROFONE, INPUT);
    randomSeed(micros());

    WiFi.begin(ssid, password);
}

//======================================================
// CONTROLE DA EMOÇÃO ATUAL
//======================================================

bool emocaoAtiva = false;
unsigned long tempoEmocao = 0;
const unsigned long DURACAO_EMOCAO = 3000;

void atualizarEmocao()
{
    if (!emocaoAtiva)
        return;

    if (millis() - tempoEmocao >= DURACAO_EMOCAO)
    {
        emocaoAtiva = false;
        posicaoNeutra();
    }
}

//======================================================
// LOOP
//======================================================

void loop()
{
    unsigned long agora = millis();

    if (agora - ultimoCheckWiFi >= 3000) 
    {
        ultimoCheckWiFi = agora;
        
        if (WiFi.status() == WL_CONNECTED) 
        {
            if (!mcpIniciado) 
            {
                mcpClient.begin(mcpEndpoint, onConnectionStatus);
                mcpIniciado = true;
            }
        } 
        else 
        {
            if (mcpIniciado) {
                mcpIniciado = false;
            }
        }
    }

    if (mcpIniciado) 
    {
        mcpClient.loop();
    }

    atualizarAndar();

    if (!emocaoAtiva)
    {
        detectarFala(); 
        moverServosAleatoriamente();
    }

    atualizarEmocao();
    atualizarPiscar();

    delay(10);
}