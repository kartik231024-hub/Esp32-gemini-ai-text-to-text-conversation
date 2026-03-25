#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h> // Make sure to install ArduinoJson library (v7 is recommended)

// ==========================================
// CONFIGURATION
// ==========================================
const char* ssid = "Jai_shree_ram_2.4G";
const char* password = "ak@123456";
const char* apiKey = "AIzaSyBNpB-lRGfodQeifbpyvP5xjytbVsi7tCs"; // Get from Google AI Studio

WebServer server(80);

// GLOBAL client to prevent Stack Overflow/Memory allocation fail (which causes Error -1)
WiFiClientSecure secureClient; 

// Modern, Premium Web UI embedded as a raw string
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Gemini AI</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --chat-bg: #1e293b;
            --user-msg: #3b82f6;
            --ai-msg: #334155;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-main);
            display: flex;
            flex-direction: column;
            height: 100vh;
        }
        .header {
            background-color: var(--chat-bg);
            padding: 1.5rem;
            text-align: center;
            font-size: 1.5rem;
            font-weight: 600;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
            z-index: 10;
        }
        .header span {
            background: linear-gradient(to right, #60a5fa, #a78bfa);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .chat-container {
            flex: 1;
            overflow-y: auto;
            padding: 2rem 1rem;
            display: flex;
            flex-direction: column;
            gap: 1.5rem;
            max-width: 800px;
            margin: 0 auto;
            width: 100%;
        }
        .message-wrapper {
            display: flex;
            width: 100%;
        }
        .user-wrapper { justify-content: flex-end; }
        .ai-wrapper { justify-content: flex-start; }
        .message {
            max-width: 80%;
            padding: 1rem 1.25rem;
            border-radius: 1.25rem;
            line-height: 1.5;
            font-size: 1rem;
            box-shadow: 0 1px 2px rgba(0,0,0,0.1);
        }
        .user-message {
            background-color: var(--user-msg);
            color: white;
            border-bottom-right-radius: 0.25rem;
        }
        .ai-message {
            background-color: var(--ai-msg);
            color: var(--text-main);
            border-bottom-left-radius: 0.25rem;
            white-space: pre-wrap; /* For code blocks and newlines */
        }
        .input-area {
            background-color: var(--chat-bg);
            padding: 1.5rem;
            box-shadow: 0 -4px 6px -1px rgba(0, 0, 0, 0.1);
        }
        .input-container {
            max-width: 800px;
            margin: 0 auto;
            display: flex;
            gap: 1rem;
        }
        input[type="text"] {
            flex: 1;
            padding: 1rem 1.5rem;
            border: 1px solid #475569;
            border-radius: 9999px;
            background-color: var(--bg-color);
            color: white;
            font-size: 1rem;
            transition: border-color 0.2s;
        }
        input[type="text"]:focus {
            outline: none;
            border-color: #60a5fa;
        }
        button {
            background: linear-gradient(to right, #3b82f6, #8b5cf6);
            color: white;
            border: none;
            padding: 0 1.5rem;
            border-radius: 9999px;
            font-weight: 600;
            font-size: 1rem;
            cursor: pointer;
            transition: opacity 0.2s;
        }
        button:hover { opacity: 0.9; }
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        .typing-indicator {
            display: none;
            padding: 1rem 1.25rem;
            background-color: var(--ai-msg);
            border-radius: 1.25rem;
            border-bottom-left-radius: 0.25rem;
            align-self: flex-start;
        }
        .dot {
            display: inline-block;
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background-color: var(--text-muted);
            margin-right: 4px;
            animation: pulse 1.5s infinite ease-in-out;
        }
        .dot:nth-child(2) { animation-delay: 0.2s; }
        .dot:nth-child(3) { animation-delay: 0.4s; margin-right: 0; }
        @keyframes pulse {
            0%, 100% { transform: scale(0.8); opacity: 0.5; }
            50% { transform: scale(1.2); opacity: 1; }
        }
    </style>
</head>
<body>
    <div class="header">ESP32 <span>Gemini 2.0 Flash</span></div>
    
    <div class="chat-container" id="chat">
        <div class="message-wrapper ai-wrapper">
            <div class="message ai-message">Hello! I'm Gemini 2.0 Flash, connected directly through your ESP32. How can I assist you today?</div>
        </div>
        <div class="message-wrapper ai-wrapper" id="loader-wrapper" style="display: none;">
            <div class="typing-indicator" id="typing">
                <div class="dot"></div><div class="dot"></div><div class="dot"></div>
            </div>
        </div>
    </div>

    <div class="input-area">
        <div class="input-container">
            <input type="text" id="userInput" placeholder="Ask Gemini..." autocomplete="off">
            <button id="sendBtn" onclick="sendMessage()">Send</button>
        </div>
    </div>

    <script>
        const chat = document.getElementById('chat');
        const userInput = document.getElementById('userInput');
        const sendBtn = document.getElementById('sendBtn');
        const loaderWrapper = document.getElementById('loader-wrapper');
        const typing = document.getElementById('typing');

        userInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') sendMessage();
        });

        async function sendMessage() {
            const text = userInput.value.trim();
            if(!text) return;

            addMessage(text, 'user');
            userInput.value = '';
            userInput.disabled = true;
            sendBtn.disabled = true;
            
            loaderWrapper.style.display = 'flex';
            typing.style.display = 'block';
            chat.appendChild(loaderWrapper);
            scrollToBottom();

            try {
                const response = await fetch('/chat', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'message=' + encodeURIComponent(text)
                });
                
                loaderWrapper.style.display = 'none';
                typing.style.display = 'none';
                
                if(response.ok) {
                    const data = await response.text();
                    addMessage(data, 'ai');
                } else {
                    addMessage("Error: ESP32 could not connect to Gemini API.", 'ai');
                }
            } catch(e) {
                loaderWrapper.style.display = 'none';
                typing.style.display = 'none';
                addMessage("Network Error: Could not reach the ESP32.", 'ai');
            }

            userInput.disabled = false;
            sendBtn.disabled = false;
            userInput.focus();
        }

        function addMessage(text, sender) {
            const wrapper = document.createElement('div');
            wrapper.className = 'message-wrapper ' + (sender === 'user' ? 'user-wrapper' : 'ai-wrapper');
            
            const msg = document.createElement('div');
            msg.className = 'message ' + (sender === 'user' ? 'user-message' : 'ai-message');
            msg.textContent = text;
            
            wrapper.appendChild(msg);
            chat.insertBefore(wrapper, loaderWrapper);
            scrollToBottom();
        }

        function scrollToBottom() {
            chat.scrollTop = chat.scrollHeight;
        }
    </script>
</body>
</html>
)rawliteral";

String callGeminiAPI(String prompt) {
  HTTPClient https;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + String(apiKey);  
  // Set longer timeout for API limits (15 seconds)
  https.setTimeout(15000);
  
  if (https.begin(secureClient, url)) {
    https.addHeader("Content-Type", "application/json");

    // Construct the clean JSON payload for REST API using ArduinoJson V7
    JsonDocument doc; 
    doc["contents"][0]["parts"][0]["text"] = prompt;

    String payload;
    serializeJson(doc, payload);

    int httpCode = https.POST(payload);

    if (httpCode == HTTP_CODE_OK) {
      String response = https.getString();
      
      // Parse the JSON response
      JsonDocument resDoc;
      DeserializationError error = deserializeJson(resDoc, response);

      if (!error) {
        // Extract plain text reply as requested (no JSON layout returned)
        String answer = resDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
        https.end();
        return answer;
      } else {
        https.end();
        return "JSON parsing error on ESP32.";
      }
    } else {
      String errStr = "HTTP Error: " + String(httpCode);
      // Give more details on the -1 error (Connection Refused) if it happens again
      if(httpCode < 0) {
        errStr += " (" + https.errorToString(httpCode) + ")";
      } else {
        errStr += " -> " + https.getString();
      }
      https.end();
      return errStr;
    }
  } else {
    return "Failed to connect to API Server.";
  }
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleChat() {
  if (server.hasArg("message")) {
    String message = server.arg("message");
    
    // Output UI conversation seamlessly in Serial Monitor 
    Serial.println("\n[Web UI User Request]: " + message);
    
    // Call AI text-to-text API
    String reply = callGeminiAPI(message);
    
    // Plain text print on ESP32 serial 
    Serial.println("[Gemini Context Reply]:\n" + reply);
    
    // Send standard string response back directly to Frontend
    server.send(200, "text/plain", reply);
  } else {
    server.send(400, "text/plain", "Missing message");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("===============================================");
  Serial.println("  ESP32 Gemini 2.0 Flash WebServer Auth Node   ");
  Serial.println("===============================================");
  
  // Bypass SSL Certificate verification on a GLOBAL level
  secureClient.setInsecure(); 

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected.");
  Serial.print("Access Web UI at: http://");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/chat", HTTP_POST, handleChat);
  server.begin();

  Serial.println("Web server running!");
  Serial.println("\nReady! You can type a message in the Serial Monitor directly to test the text-to-text chat locally.");
  Serial.println("-----------------------------------------------");
}

void loop() {
  // 1. Maintain web page server queue and calls
  server.handleClient();

  // 2. Allow Serial Conversation parallelly (Local input) 
  if (Serial.available() > 0) {
    String serialInput = Serial.readStringUntil('\n');
    serialInput.trim(); 
    
    if (serialInput.length() > 0) {
      // 3. Serial Monitor plain text interaction only output
      Serial.println("\n[Serial Target Request]: " + serialInput);
      Serial.println("Thinking...");
      
      String reply = callGeminiAPI(serialInput);
      
      Serial.println("\n[Gemini Response]:\n" + reply);
      Serial.println("\n-----------------------------------------------");
      Serial.println("Ready for next message.");
    }
  }
}
