
#include <WiFiManager.h>
#include <Preferences.h>
#include <HTTPClient.h>

#include <ArduinoWebsockets.h>

#include <DHT.h>

#define DHTPIN 15
#define DHTTYPE DHT11
#define MQ_DIGITAL 13

DHT dht(DHTPIN, DHTTYPE);

Preferences preferences;

String deviceToken = "";

WiFiManager wifiManager;

const char* websockets_connection_string = "wss://websockets-gerenciamento-residuos.onrender.com/ws";
using namespace websockets;
WebsocketsClient client;

const char ssl_ca_cert[] = \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIICnzCCAiWgAwIBAgIQf/MZd5csIkp2FV0TttaF4zAKBggqhkjOPQQDAzBHMQsw\n" \
  "CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n" \
  "MBIGA1UEAxMLR1RTIFJvb3QgUjQwHhcNMjMxMjEzMDkwMDAwWhcNMjkwMjIwMTQw\n" \
  "MDAwWjA7MQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZp\n" \
  "Y2VzMQwwCgYDVQQDEwNXRTEwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARvzTr+\n" \
  "Z1dHTCEDhUDCR127WEcPQMFcF4XGGTfn1XzthkubgdnXGhOlCgP4mMTG6J7/EFmP\n" \
  "LCaY9eYmJbsPAvpWo4H+MIH7MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggr\n" \
  "BgEFBQcDAQYIKwYBBQUHAQIwEgYDVR0TAQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQU\n" \
  "kHeSNWfE/6jMqeZ72YB5e8yT+TgwHwYDVR0jBBgwFoAUgEzW63T/STaj1dj8tT7F\n" \
  "avCUHYwwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzAChhhodHRwOi8vaS5wa2ku\n" \
  "Z29vZy9yNC5jcnQwKwYDVR0fBCQwIjAgoB6gHIYaaHR0cDovL2MucGtpLmdvb2cv\n" \
  "ci9yNC5jcmwwEwYDVR0gBAwwCjAIBgZngQwBAgEwCgYIKoZIzj0EAwMDaAAwZQIx\n" \
  "AOcCq1HW90OVznX+0RGU1cxAQXomvtgM8zItPZCuFQ8jSBJSjz5keROv9aYsAm5V\n" \
  "sQIwJonMaAFi54mrfhfoFNZEfuNMSQ6/bIBiNLiyoX46FohQvKeIoJ99cx7sUkFN\n" \
  "7uJW\n" \
  "-----END CERTIFICATE-----\n";


void onMessageCallback(WebsocketsMessage message) {
    Serial.print("Got Message: ");
    Serial.println(message.data());
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    } else if(event == WebsocketsEvent::GotPing) {
        Serial.println("Got a Ping!");
    } else if(event == WebsocketsEvent::GotPong) {
        Serial.println("Got a Pong!");
    }
}

// Função para salvar o token na NVS
void saveToken(String token) {
    preferences.begin("user_prefs", false); // Inicia o namespace 'user_prefs' para armazenamento
    preferences.putString("deviceToken", token); // Salva o token com a chave "deviceToken"
    preferences.end();
    Serial.println("Token do dispositivo salvo na NVS.");
}

// Função para carregar o token da NVS
String loadToken() {
    preferences.begin("user_prefs", true); // Abre o namespace 'user_prefs' em modo somente leitura
    String deviceToken = preferences.getString("deviceToken", ""); // Recupera o token ou retorna vazio se não existir
    preferences.end();
    return deviceToken;
}

void clearToken() {
    preferences.begin("user_prefs", false);
    preferences.remove("deviceToken");
    preferences.end();
    Serial.println("Token apagado da NVS!");
}

// Função para enviar dados com o token 
void sendDataWithToken(float temperature, float humidity, int gasLevel) {
    String message = "{\"temperature\":\"" + String(temperature) + 
                     "\", \"humidity\":\"" + String(humidity) + 
                     "\", \"gasLevel\":\"" + String(gasLevel) + 
                     "\", \"deviceId\":\"" + deviceToken + "\"}";

    client.send(message);
    Serial.println("Dados enviados: " + message);
}

void setup() {
    Serial.begin(115200);
    preferences.begin("user_prefs", false);
    
    dht.begin();
    pinMode(MQ_DIGITAL, INPUT);
    
    // Carrega o token salvo
    deviceToken = loadToken();
    Serial.println("Token no nvs: " + deviceToken);

    // Configuração do WiFiManager para receber o token
    WiFiManagerParameter custom_token("token", "Insira o Token", "", 512);
    wifiManager.addParameter(&custom_token);

    // Verifica se deve abrir o portal de configuração
    if (deviceToken == "") {
        Serial.println("Abrindo o portal de configuração WiFi...");
        wifiManager.startConfigPortal("ESP32-Config");

        // Recupera o token fornecido pelo usuário via WiFiManager
        String token = custom_token.getValue();

        // Salva o token na NVS se não estiver vazio
        if (token != "") {
            saveToken(token);
            deviceToken = token;
        } else {
            // Se o token não foi fornecido, tenta carregar da NVS novamente
            deviceToken = loadToken();
            if (deviceToken == "") {
                Serial.println("Nenhum token salvo e nenhum token fornecido.");
                ESP.restart();
            }
        }
    } else {
        // Existe token salvo, mas pode ter mudado o WiFi
        Serial.println("Tentando conectar ao WiFi automaticamente...");
        WiFi.begin();

        if (WiFi.waitForConnectResult() != WL_CONNECTED) {
            Serial.println("Falha ao conectar automaticamente.");
            Serial.println("Abrindo portal para reconfigurar WiFi e Token...");

            wifiManager.startConfigPortal("ESP32-Config");

            // Recupera o token fornecido
            String token = custom_token.getValue();

            // Se informado, salva
            if (token != "") {
                saveToken(token);
                deviceToken = token;
            }

            Serial.println("Nova configuração concluída.");
        } else {
            Serial.println("Conectado ao WiFi automaticamente!");
        }
    }

    // run callback when messages are received
    client.onMessage(onMessageCallback);
    
    // run callback when events are occuring
    client.onEvent(onEventsCallback);

    // Connect to server
    client.setCACert(ssl_ca_cert);
    bool connected = client.connect(websockets_connection_string);

    if (connected) {
      Serial.println("Connected");
      delay(1000);

      // Send a ping
      client.ping();
    } else {
      Serial.println("Connection failed.");
    }
}

void loop() {
    client.poll();
    
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    int gasLevel = digitalRead(MQ_DIGITAL);
    gasLevel = !gasLevel; // inverte logica, pois o sensor esta invertido. (ta retornando 0=Alerta e 1=Bom )

    Serial.printf("Temp: %.2f  Hum: %.2f  MQ_D0: %d\n",
                  temperature, humidity, gasLevel);

    // Envia dados se o token estiver presente e WiFi conectado
    if (WiFi.isConnected() && deviceToken != "") {
        sendDataWithToken(temperature, humidity, gasLevel);
    } else {
        Serial.println("Usuário não autenticado ou WiFi desconectado. Dados não enviados.");
    }

    delay(5000); // Intervalo de envio de dados
}
