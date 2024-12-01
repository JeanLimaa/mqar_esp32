
#include <WiFiManager.h>
#include <Preferences.h>
#include <HTTPClient.h>

#include <ArduinoWebsockets.h>

Preferences preferences;

String deviceToken = "";

WiFiManager wifiManager;

const char* websockets_connection_string = "wss://websockets-gerenciamento-residuos.onrender.com/ws"; //Enter server adress
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

// Função para enviar dados com o token 
void sendDataWithToken(float temperature, float humidity, float gasLevel) {
    String message = "{\"temperature\":\"" + String(temperature) + 
                     "\", \"humidity\":\"" + String(humidity) + 
                     "\", \"gasLevel\":\"" + String(gasLevel) + 
                     "\", \"deviceToken\":\"" + deviceToken + "\"}";

    client.send(message);
    Serial.println("Dados enviados: " + message);
}

void setup() {
    Serial.begin(115200);
    preferences.begin("user_prefs", false);
    
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
        // Tenta conectar automaticamente se o token estiver presente
        WiFi.begin();
        if (WiFi.waitForConnectResult() == WL_CONNECTED) {
            Serial.println("Conectado ao WiFi automaticamente!");
            Serial.print("Endereço IP: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("Falha ao conectar automaticamente, abrindo o portal.");
            wifiManager.startConfigPortal("ESP32-Config");
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
    
    // Simula dados de temperatura e umidade
    float temperature = random(20, 35);
    float humidity = random(30, 70);
    float gasLevel = random(30, 70);

    // Envia dados se o token estiver presente e WiFi conectado
    if (WiFi.isConnected() && deviceToken != "") {
        sendDataWithToken(temperature, humidity, gasLevel);
    } else {
        Serial.println("Usuário não autenticado ou WiFi desconectado. Dados não enviados.");
    }

    delay(2000); // Intervalo de envio de dados
}
