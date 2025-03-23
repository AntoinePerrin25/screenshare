#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Inclusions pour obtenir l'adresse IP locale
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif
#include <raylib.h>

#include "../include/capture.h"
#include "../include/network.h"
#include "../include/ui.h"

// Constantes
#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define APP_NAME "C_Screenshare - Peer-to-Peer Screen Sharing"
#define MAX_IP_LENGTH 64
#define DEFAULT_PORT 7890

// États de l'application
typedef enum {
    APP_STATE_IDLE,       // En attente, pas de partage actif
    APP_STATE_SHARING,    // Partage d'écran en cours
    APP_STATE_VIEWING     // Visualisation d'un partage en cours
} AppState;

/**
 * @brief Structure principale contenant l'état de l'application
 * @details Contient toutes les informations nécessaires pour gérer l'état de l'application.
 * @param bool running;              
 * @param bool minimized;            
 * @param AppState state;            
 * @param int captureInterval;       
 * @param int captureQuality;        
 * @param bool encryptionEnabled;    
 * @param Rectangle captureRegion;   
 * @param CaptureData currentCapture;
 * @param bool hasCaptureData;       
 * @param UIPage currentPage;        
 */
typedef struct {
    bool running;               // Indique si l'application est en cours d'exécution
    bool minimized;             // Indique si l'application est minimisée
    AppState state;             // État actuel de l'application
    int captureInterval;        // Intervalle de capture en ms
    int captureQuality;         // Qualité de la capture (0-100)
    bool encryptionEnabled;     // Indique si le chiffrement est activé
    Rectangle captureRegion;    // Région de capture (utilisée en mode partage)
    CaptureData currentCapture; // Dernière capture effectuée
    bool hasCaptureData;        // Indique si des données de capture sont disponibles
    UIPage currentPage;         // Page UI actuelle
    
    // Informations réseau
    char localIP[MAX_IP_LENGTH]; // IP locale de l'hôte
    int localPort;               // Port local
} AppContext;

// Prototypes de fonctions
void InitApplication(AppContext* ctx);
void CloseApplication(AppContext* ctx);
void UpdateApplication(AppContext* ctx);
void RenderApplication(AppContext* ctx);
void HandleEvents(AppContext* ctx);
void ToggleSharing(AppContext* ctx);
void ToggleMinimized(AppContext* ctx);
bool GetLocalIPAddress(char* ipBuffer, int bufferSize);
void RenderTopBar(AppContext* ctx); // Nouvelle fonction pour afficher la barre supérieure

int main(void) {
    // Initialisation du contexte de l'application
    AppContext appContext = {0};
    appContext.running = true;
    appContext.state = APP_STATE_IDLE;
    appContext.captureInterval = (int) (1000 / 0.5); // 10 FPS par défaut
    appContext.captureQuality = 100;   // 80% de qualité par défaut
    appContext.encryptionEnabled = false;
    // La région de capture sera initialisée après InitWindow() dans InitApplication()
    appContext.currentPage = UI_PAGE_MAIN;
    
    // Initialisation de l'application
    InitApplication(&appContext);
    
    // Boucle principale
    while (appContext.running && !WindowShouldClose()) {
        HandleEvents(&appContext);
        UpdateApplication(&appContext);
        RenderApplication(&appContext);
        RenderTopBar(&appContext);
    }
    
    // Nettoyage
    CloseApplication(&appContext);
    
    return 0;
}

void InitApplication(AppContext* ctx) {
    if (!ctx) return;
    
    // Initialisation de la fenêtre avec raylib
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, APP_NAME);
    SetTargetFPS(60);
    
    // Initialisation du système de capture
    if (!InitCaptureSystem()) {
        printf("[ERROR] Échec de l'initialisation du système de capture\n");
        ctx->running = false;
        return;
    }
    
    // Initialisation de la région de capture avec les dimensions de l'écran physique
    ctx->captureRegion = (Rectangle){ 
        0, 
        0, 
        GetMonitorWidth(0),  // Moniteur principal (index 0)
        GetMonitorHeight(0)
    };
    
    printf("[INFO] Région de capture initialisée: %.0fx%.0f\n", 
           ctx->captureRegion.width, ctx->captureRegion.height);
    
    // Initialisation des informations réseau
    ctx->localPort = DEFAULT_PORT;
    if (!GetLocalIPAddress(ctx->localIP, MAX_IP_LENGTH)) {
        printf("[WARNING] Impossible d'obtenir l'adresse IP locale, utilisation de 127.0.0.1\n");
        strncpy(ctx->localIP, "127.0.0.1", MAX_IP_LENGTH);
    }
    
    printf("[INFO] Adresse IP locale: %s:%d\n", ctx->localIP, ctx->localPort);
    
    // Note: Pour cette étape 2 nous n'initialisons pas encore le réseau complet
    // L'initialisation complète sera faite dans les étapes suivantes
    
    printf("[INFO] Application initialisée avec succès\n");
}

void CloseApplication(AppContext* ctx) {
    if (!ctx) return;
    
    // Libération des ressources de capture
    if (ctx->hasCaptureData) {
        UnloadCaptureData(&ctx->currentCapture);
        ctx->hasCaptureData = false;
    }
    
    // Fermeture du système de capture
    CloseCaptureSystem();
    
    // Fermeture de la fenêtre raylib
    CloseWindow();
    
    printf("[INFO] Application fermée\n");
}

void UpdateApplication(AppContext* ctx) {
    if (!ctx || !ctx->running) return;
    
    // En mode partage, faire une capture à intervalle régulier
    static double lastCaptureTime = 0;
    double currentTime = GetTime();
    
    if (ctx->state == APP_STATE_SHARING && 
        (currentTime - lastCaptureTime) * 1000 >= ctx->captureInterval) {
        
        // Libération de la capture précédente si elle existe
        if (ctx->hasCaptureData) {
            UnloadCaptureData(&ctx->currentCapture);
            ctx->hasCaptureData = false;
        }
        
        // Capture de l'écran entier ou d'une région spécifique
        // Utiliser les dimensions de l'écran physique pour la comparaison
        if (ctx->captureRegion.width == GetMonitorWidth(0) && 
            ctx->captureRegion.height == GetMonitorHeight(0)) {
            ctx->currentCapture = CaptureScreen();
        } else {
            ctx->currentCapture = CaptureScreenRegion(ctx->captureRegion);
        }
        
        ctx->hasCaptureData = true;
        
        // Compression des données de capture
        if (ctx->hasCaptureData) {
            CompressCaptureData(&ctx->currentCapture, ctx->captureQuality);
        }
        
        // Dans une étape future, on enverrait ici les données via le réseau
        
        lastCaptureTime = currentTime;
    }
}

void RenderApplication(AppContext* ctx) {
    if (!ctx || !ctx->running) return;
    
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    // Afficher la barre supérieure avec l'IP locale
    RenderTopBar(ctx);
    
    // Affichage de la capture si disponible
    if (ctx->hasCaptureData) {
        // Calculer les dimensions pour afficher l'image à l'échelle dans la fenêtre
        float scale = fmin((float)GetScreenWidth() / ctx->currentCapture.width, 
                           (float)GetScreenHeight() / ctx->currentCapture.height);
        
        int displayWidth = (int)(ctx->currentCapture.width * scale);
        int displayHeight = (int)(ctx->currentCapture.height * scale);
        int posX = (GetScreenWidth() - displayWidth) / 2;
        int posY = (GetScreenHeight() - displayHeight) / 2;
        
        // Afficher la texture
        DrawTexturePro(ctx->currentCapture.texture, 
                     (Rectangle){0, 0, (float)ctx->currentCapture.width, (float)ctx->currentCapture.height},
                     (Rectangle){(float)posX, (float)posY, (float)displayWidth, (float)displayHeight},
                     (Vector2){0, 0}, 0.0f, WHITE);
        
        // Afficher des informations sur la capture
        DrawText(TextFormat("Capture: %dx%d", ctx->currentCapture.width, ctx->currentCapture.height), 
                 10, 40, 20, DARKGRAY);  // Décalé vers le bas pour éviter la barre supérieure
                 
        if (ctx->currentCapture.isCompressed) {
            DrawText(TextFormat("Compression: %d octets (Ratio: %.2f:1)", 
                   ctx->currentCapture.compressedSize,
                   (float)(ctx->currentCapture.width * ctx->currentCapture.height * 4) / ctx->currentCapture.compressedSize), 
                   10, 70, 20, DARKGRAY);  // Décalé vers le bas
        }
    }
    
    // Affichage de l'état et des contrôles
    DrawText(ctx->state == APP_STATE_SHARING ? "État: Partage en cours" : "État: En attente", 
             10, GetScreenHeight() - 60, 20, ctx->state == APP_STATE_SHARING ? GREEN : GRAY);
    
    DrawText("Appuyez sur S pour démarrer/arrêter le partage", 
             10, GetScreenHeight() - 30, 20, DARKGRAY);
    
    EndDrawing();
}

void HandleEvents(AppContext* ctx) {
    if (!ctx || !ctx->running) return;
    
    // Gestion des touches
    if (IsKeyPressed(KEY_S)) {
        ToggleSharing(ctx);
    }
    
    if (IsKeyPressed(KEY_ESCAPE)) {
        ctx->running = false;
    }
    
    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
    }
    
    // Pour l'étape 5, nous ajouterons ici la gestion de l'icône de la zone de notification
}

void ToggleSharing(AppContext* ctx) {
    if (!ctx) return;
    
    if (ctx->state == APP_STATE_IDLE) {
        ctx->state = APP_STATE_SHARING;
        printf("[INFO] Démarrage du partage d'écran\n");
    } else if (ctx->state == APP_STATE_SHARING) {
        ctx->state = APP_STATE_IDLE;
        printf("[INFO] Arrêt du partage d'écran\n");
    }
}

void ToggleMinimized(AppContext* ctx) {
    if (!ctx) return;
    
    ctx->minimized = !ctx->minimized;
    
    // Dans l'étape 5, nous intégrerons ici la gestion de la zone de notification
}

// Fonction pour obtenir l'adresse IP locale
bool GetLocalIPAddress(char* ipBuffer, int bufferSize) {
    if (!ipBuffer || bufferSize <= 0) return false;
    
    // Initialisation par défaut
    strncpy(ipBuffer, "127.0.0.1", bufferSize);
    
#ifdef _WIN32
    // Initialisation de Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[ERROR] Échec de l'initialisation de WSAStartup\n");
        return false;
    }
    
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        printf("[ERROR] Échec de l'obtention du hostname\n");
        WSACleanup();
        return false;
    }
    
    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(hostname, NULL, &hints, &result) != 0) {
        printf("[ERROR] Échec de getaddrinfo\n");
        WSACleanup();
        return false;
    }
    
    // Parcourir les résultats et prendre la première adresse IPv4 non-localhost
    for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        struct sockaddr_in* addr = (struct sockaddr_in*)ptr->ai_addr;
        const char* ip = inet_ntoa(addr->sin_addr);
        
        // Vérifier si ce n'est pas localhost
        if (strcmp(ip, "127.0.0.1") != 0) {
            strncpy(ipBuffer, ip, bufferSize);
            freeaddrinfo(result);
            WSACleanup();
            return true;
        }
    }
    
    freeaddrinfo(result);
    WSACleanup();
#else
    struct ifaddrs* ifaddr, *ifa;
    int family, s;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return false;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        family = ifa->ifa_addr->sa_family;
        
        // Nous voulons uniquement les adresses IPv4
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                           ipBuffer, bufferSize,
                           NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("[ERROR] getnameinfo() a échoué: %s\n", gai_strerror(s));
                freeifaddrs(ifaddr);
                return false;
            }
            
            // Ignorer localhost
            if (strcmp(ipBuffer, "127.0.0.1") != 0) {
                freeifaddrs(ifaddr);
                return true;
            }
        }
    }
    
    freeifaddrs(ifaddr);
#endif
    
    return true; // Retourne true même si on n'a trouvé que localhost
}

// Fonction pour dessiner la barre supérieure avec l'IP
void RenderTopBar(AppContext* ctx) {
    if (!ctx) return;
    
    // Fond de la barre supérieure
    DrawRectangle(0, 0, GetScreenWidth(), 30, DARKGRAY);
    
    // Afficher l'IP locale
    DrawText(TextFormat("IP Client: %s:%d", ctx->localIP, ctx->localPort), 10, 5, 20, WHITE);
}