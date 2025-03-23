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
//#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif
#include <raylib.h>
#define NETWORK_IMPL
#include "../include/capture.h"
#include "../include/network.h"
#include "../include/ui.h"

// Constantes
#define WINDOW_WIDTH \
                    960     
                    /*
                    1920
                    1600
                    1366
                    1280
                    854 
                    640 
                    */
#define WINDOW_HEIGHT WINDOW_WIDTH * 9 / 16 // Ratio 16:9
#define TARGET_FPS 60
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
    
    // Nouvelles informations pour le partage P2P
    bool networkInitialized;     // Indique si le réseau est initialisé
    char remotePeerIP[MAX_IP_LENGTH]; // IP du pair distant pour la connexion
    int remotePeerPort;          // Port du pair distant pour la connexion
    int connectedPeerID;         // ID du pair connecté (-1 si aucun)
    bool isConnecting;           // Indique si une connexion est en cours
    int lastNetworkActivity;     // Horodatage de la dernière activité réseau
    char connectionStatus[256];  // Message de statut de connexion
    char connectionPassword[64]; // Mot de passe pour le chiffrement
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

// Nouvelles fonctions pour la gestion des connexions réseau
void ConnectToPeerByIP(AppContext* ctx);
void DisconnectFromCurrentPeer(AppContext* ctx);
void ToggleEncryption(AppContext* ctx);

int main(void) {
    // Initialisation du contexte de l'application
    AppContext appContext = {0};
    appContext.running = true;
    appContext.state = APP_STATE_IDLE;
    appContext.captureInterval = (int) (1000 / TARGET_FPS); // 10 FPS par défaut
    appContext.captureQuality = 50;   // 80% de qualité par défaut
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
    
    // Configuration du système de capture avec les nouvelles options
    CaptureConfig captureConfig = {0};
    captureConfig.method = CAPTURE_METHOD_AUTO; // Sélection automatique de la meilleure méthode
    captureConfig.quality = ctx->captureQuality;
    captureConfig.captureInterval = ctx->captureInterval;
    captureConfig.detectChanges = true; // Activer la détection de changements
    captureConfig.changeThreshold = 5;  // 5% de tolérance pour les changements
    captureConfig.autoAdjustQuality = true;
    captureConfig.targetMonitor = -1;   // Capturer tous les moniteurs par défaut
    
    // Initialisation du système de capture avec la configuration
    if (!InitCaptureSystem(&captureConfig)) {
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
    
    // Initialisation du système réseau P2P
    if (InitNetworkSystem(ctx->localPort)) {
        ctx->networkInitialized = true;
        strcpy(ctx->connectionStatus, "Réseau initialisé, en attente de connexion");
        printf("[INFO] Système réseau initialisé sur le port %d\n", ctx->localPort);
    } else {
        ctx->networkInitialized = false;
        strcpy(ctx->connectionStatus, "Échec de l'initialisation réseau");
        printf("[ERROR] Échec de l'initialisation du système réseau\n");
    }
    
    // Initialisation des autres variables réseau
    ctx->connectedPeerID = -1;
    ctx->isConnecting = false;
    ctx->remotePeerPort = DEFAULT_PORT;
    ctx->lastNetworkActivity = GetTime();
    
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
    
    // Fermeture du système réseau
    if (ctx->networkInitialized) {
        printf("[INFO] Fermeture du système réseau\n");
        CloseNetworkSystem();
        ctx->networkInitialized = false;
    }
    
    // Fermeture de la fenêtre raylib
    CloseWindow();
    
    printf("[INFO] Application fermée\n");
}

void UpdateApplication(AppContext* ctx) {
    if (!ctx || !ctx->running) return;
    
    // Traitement des événements réseau
    if (ctx->networkInitialized) {
        int processedPackets = ProcessNetworkEvents();
        if (processedPackets > 0) {
            ctx->lastNetworkActivity = GetTime();
        }
    }
    
    // En mode partage, faire une capture à intervalle régulier
    static double lastCaptureTime = 0;
    double currentTime = GetTime();
    
    if (ctx->state == APP_STATE_SHARING && 
        (currentTime - lastCaptureTime) * 1000 >= ctx->captureInterval) {
        
        // Récupération de la configuration actuelle
        CaptureConfig config = GetCaptureConfig();
        
        // Libération de la capture précédente si elle existe
        if (ctx->hasCaptureData) {
            // Conserver les données précédentes pour la détection de changements
            // UnloadCaptureData libère automatiquement ces ressources
            UnloadCaptureData(&ctx->currentCapture);
            ctx->hasCaptureData = false;
        }
        
        // Capture selon la configuration (moniteur spécifique ou région)
        if (config.targetMonitor >= 0) {
            // Capture d'un moniteur spécifique
            ctx->currentCapture = CaptureMonitor(config.targetMonitor);
        } else if (ctx->captureRegion.width > 0 && ctx->captureRegion.height > 0) {
            // Capture d'une région spécifique
            ctx->currentCapture = CaptureScreenRegion(ctx->captureRegion);
        } else {
            // Capture de tout l'écran
            ctx->currentCapture = CaptureScreen();
        }
        
        // Vérifier si la capture a réussi
        if (ctx->currentCapture.image.data != NULL) {
            ctx->hasCaptureData = true;
            
            // Détection des changements si activée
            if (config.detectChanges) {
                DetectChanges(&ctx->currentCapture, config.changeThreshold);
                
                // Si l'image n'a pas changé significativement, on peut ajuster la qualité
                // ou la fréquence de capture pour économiser la bande passante
                if (!ctx->currentCapture.hasChanged && config.autoAdjustQuality) {
                    // Réduire temporairement la qualité pour les images qui changent peu
                    int adjustedQuality = ctx->captureQuality * 0.7;
                    CompressCaptureData(&ctx->currentCapture, adjustedQuality);
                } else {
                    // Utiliser la qualité normale pour les images avec changements
                    CompressCaptureData(&ctx->currentCapture, ctx->captureQuality);
                }
            } else {
                // Compression standard si la détection de changements est désactivée
                CompressCaptureData(&ctx->currentCapture, ctx->captureQuality);
            }
            
            // Envoi des données de capture via le réseau si connecté à un pair
            if (ctx->networkInitialized && ctx->connectedPeerID >= 0) {
                // Chiffrer les données si le chiffrement est activé
                if (ctx->encryptionEnabled) {
                    EncryptCaptureData(&ctx->currentCapture);
                }
                
                // Envoyer les données au pair connecté
                if (SendCaptureData(ctx->connectedPeerID, &ctx->currentCapture)) {
                    strcpy(ctx->connectionStatus, "Capture envoyée avec succès");
                } else {
                    strcpy(ctx->connectionStatus, "Échec de l'envoi de la capture");
                    printf("[ERROR] Échec de l'envoi des données de capture au pair %d\n", 
                           ctx->connectedPeerID);
                }
            }
        } else {
            printf("[ERROR] Échec de la capture d'écran\n");
        }
        
        lastCaptureTime = currentTime;
    }
    
    // Vérifier l'état de la connexion (timeout, etc.)
    if (ctx->networkInitialized && ctx->connectedPeerID >= 0) {
        double timeSinceLastActivity = currentTime - ctx->lastNetworkActivity;
        
        // Si aucune activité pendant 10 secondes, on considère la connexion comme perdue
        if (timeSinceLastActivity > 10.0) {
            printf("[WARNING] Timeout de connexion avec le pair %d\n", ctx->connectedPeerID);
            DisconnectFromPeer(ctx->connectedPeerID);
            ctx->connectedPeerID = -1;
            strcpy(ctx->connectionStatus, "Connexion perdue (timeout)");
        }
    }
}

void RenderApplication(AppContext* ctx) {
    if (!ctx || !ctx->running) return;
    
    BeginDrawing();
    ClearBackground(RAYWHITE);
    
    // Afficher la barre supérieure avec l'IP locale
    RenderTopBar(ctx);
    
    // Récupération de la configuration actuelle
    CaptureConfig config = GetCaptureConfig();
    
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
        int y = 40; // Position Y initiale
        
        // Dimensions et source
        const char* sourceText = "Écran complet";
        if (ctx->currentCapture.monitorIndex >= 0) {
            sourceText = TextFormat("Moniteur %d", ctx->currentCapture.monitorIndex);
        } else if (ctx->captureRegion.width != GetMonitorWidth(0) || 
                  ctx->captureRegion.height != GetMonitorHeight(0)) {
            sourceText = "Région personnalisée";
        }
        
        DrawText(TextFormat("Capture: %dx%d (%s)", 
                           ctx->currentCapture.width, 
                           ctx->currentCapture.height,
                           sourceText), 
                 10, y, 20, DARKGRAY);
        y += 30;
        
        // Méthode de capture utilisée
        const char* methodText = "raylib";
        if (config.method == CAPTURE_METHOD_WIN_GDI) {
            methodText = "Windows GDI";
        }
        DrawText(TextFormat("Méthode: %s", methodText), 10, y, 20, DARKGRAY);
        y += 30;
        
        // Informations de compression
        if (ctx->currentCapture.isCompressed) {
            float ratio = (float)(ctx->currentCapture.width * ctx->currentCapture.height * 4) / 
                         ctx->currentCapture.compressedSize;
            
            DrawText(TextFormat("Compression: %d Ko (Ratio: %.2f:1)", 
                              ctx->currentCapture.compressedSize / 1024,
                              ratio), 
                    10, y, 20, DARKGRAY);
            y += 30;
        }
        
        // Informations sur la détection de changements
        if (config.detectChanges) {
            Color changeColor = ctx->currentCapture.hasChanged ? GREEN : GRAY;
            DrawText(TextFormat("Changement détecté: %s", 
                              ctx->currentCapture.hasChanged ? "Oui" : "Non"), 
                    10, y, 20, changeColor);
            y += 30;
        }
        
        // Informations sur le moniteur actif ou la région
        int monitorCount = GetMonitorCount();
        DrawText(TextFormat("Moniteurs disponibles: %d", monitorCount), 10, y, 20, DARKGRAY);
        y += 30;
        
        // Informations d'intervalle et qualité
        DrawText(TextFormat("Intervalle: %d ms, Qualité: %d%%", 
                          ctx->captureInterval, ctx->captureQuality), 
                10, y, 20, DARKGRAY);
        y += 30;
        
        // Informations sur la connexion réseau
        if (ctx->networkInitialized) {
            Color netColor = ctx->connectedPeerID >= 0 ? GREEN : GRAY;
            DrawText(TextFormat("Réseau: %s", ctx->connectedPeerID >= 0 ? 
                              "Connecté" : "En attente"), 10, y, 20, netColor);
            y += 30;
            
            if (ctx->connectedPeerID >= 0) {
                DrawText(TextFormat("Pair connecté: %s (ID %d)", 
                                  ctx->remotePeerIP, ctx->connectedPeerID), 10, y, 20, netColor);
                y += 30;
            }
            
            if (ctx->encryptionEnabled) {
                DrawText("Chiffrement: Activé", 10, y, 20, GREEN);
            } else {
                DrawText("Chiffrement: Désactivé", 10, y, 20, GRAY);
            }
            y += 30;
        }

        // Current fps on white rectangle
        DrawRectangle(0, y+10, 90, 20, WHITE);
        DrawText(TextFormat("FPS: %d", GetFPS()), 10, y + 10, 20, DARKGRAY);
    }
    
    // Affichage de l'état et des contrôles
    int bottomY = GetScreenHeight() - 120;
    
    // Affichage de l'état de l'application
    DrawText(ctx->state == APP_STATE_SHARING ? "État: Partage en cours" : "État: En attente", 
             10, bottomY, 20, ctx->state == APP_STATE_SHARING ? GREEN : GRAY);
    bottomY += 30;
    
    // Affichage du statut de connexion
    if (ctx->networkInitialized) {
        DrawText(ctx->connectionStatus, 10, bottomY, 20, 
                ctx->connectedPeerID >= 0 ? GREEN : ORANGE);
        bottomY += 30;
    }
    
    // Affichage des touches de contrôle
    DrawText("Contrôles:", 10, bottomY, 20, DARKGRAY);
    bottomY += 30;
    
    DrawText("S: Démarrer/Arrêter le partage | C: Connecter à un pair | D: Déconnecter", 
             10, bottomY, 20, DARKGRAY);
    bottomY += 30;
    
    DrawText("E: Activer/Désactiver chiffrement | ESC: Quitter | F11: Plein écran", 
             10, bottomY, 20, DARKGRAY);
    
    EndDrawing();
}

void HandleEvents(AppContext* ctx) {
    if (!ctx || !ctx->running) return;
    
    // Gestion des touches pour le partage d'écran
    if (IsKeyPressed(KEY_S)) {
        ToggleSharing(ctx);
    }
    
    // Gestion des touches pour la connexion réseau
    if (IsKeyPressed(KEY_C)) {
        // Demander l'adresse IP à l'utilisateur
        const char* input = TextInputBox((Rectangle){GetScreenWidth()/2 - 200, GetScreenHeight()/2 - 50, 400, 100}, 
                                       "Connexion au pair", "Entrez l'adresse IP du pair:", "192.168.1.x");
        if (input != NULL && strlen(input) > 0) {
            strncpy(ctx->remotePeerIP, input, MAX_IP_LENGTH);
            ConnectToPeerByIP(ctx);
        }
    }
    
    // Touche pour déconnecter
    if (IsKeyPressed(KEY_D)) {
        if (ctx->connectedPeerID >= 0) {
            DisconnectFromCurrentPeer(ctx);
        } else {
            strcpy(ctx->connectionStatus, "Aucun pair connecté");
        }
    }
    
    // Touche pour activer/désactiver le chiffrement
    if (IsKeyPressed(KEY_E)) {
        if (!ctx->encryptionEnabled) {
            // Demander le mot de passe à l'utilisateur
            const char* password = TextInputBox((Rectangle){GetScreenWidth()/2 - 200, GetScreenHeight()/2 - 50, 400, 100}, 
                                              "Chiffrement", "Entrez un mot de passe:", "");
            if (password != NULL && strlen(password) >= 4) {
                strncpy(ctx->connectionPassword, password, sizeof(ctx->connectionPassword) - 1);
                ToggleEncryption(ctx);
            } else {
                strcpy(ctx->connectionStatus, "Mot de passe trop court ou annulé");
            }
        } else {
            ToggleEncryption(ctx);
        }
    }
    
    // Autres commandes existantes
    if (IsKeyPressed(KEY_ESCAPE)) {
        ctx->running = false;
    }
    
    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
    }
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

// Fonction pour connecter à un pair distant
void ConnectToPeerByIP(AppContext* ctx) {
    if (!ctx || !ctx->networkInitialized) return;
    
    // Vérifier si on est déjà connecté
    if (ctx->connectedPeerID >= 0) {
        printf("[INFO] Déjà connecté au pair ID %d, déconnexion d'abord\n", ctx->connectedPeerID);
        DisconnectFromCurrentPeer(ctx);
    }
    
    // Vérifier si l'adresse IP est valide
    if (strlen(ctx->remotePeerIP) < 7) { // Longueur minimale (1.1.1.1)
        strcpy(ctx->connectionStatus, "Adresse IP invalide");
        return;
    }
    
    // Mise à jour du statut
    ctx->isConnecting = true;
    sprintf(ctx->connectionStatus, "Connexion à %s:%d...", ctx->remotePeerIP, ctx->remotePeerPort);
    
    // Tentative de connexion
    int peerId = ConnectToPeer(ctx->remotePeerIP, ctx->remotePeerPort);
    
    if (peerId >= 0) {
        ctx->connectedPeerID = peerId;
        sprintf(ctx->connectionStatus, "Connecté à %s:%d (ID %d)", 
               ctx->remotePeerIP, ctx->remotePeerPort, peerId);
        
        // Activation du chiffrement si nécessaire
        if (ctx->encryptionEnabled && strlen(ctx->connectionPassword) > 0) {
            if (EnableEncryption(ctx->connectionPassword)) {
                strcat(ctx->connectionStatus, " (Chiffré)");
            } else {
                strcat(ctx->connectionStatus, " (Échec du chiffrement)");
            }
        }
        
        ctx->lastNetworkActivity = GetTime();
    } else {
        sprintf(ctx->connectionStatus, "Échec de connexion à %s:%d", 
               ctx->remotePeerIP, ctx->remotePeerPort);
    }
    
    ctx->isConnecting = false;
}

// Fonction pour se déconnecter du pair actuel
void DisconnectFromCurrentPeer(AppContext* ctx) {
    if (!ctx || !ctx->networkInitialized || ctx->connectedPeerID < 0) return;
    
    // Déconnecter du pair
    DisconnectFromPeer(ctx->connectedPeerID);
    
    // Mise à jour du statut
    sprintf(ctx->connectionStatus, "Déconnecté du pair ID %d", ctx->connectedPeerID);
    ctx->connectedPeerID = -1;
    
    // Désactiver le chiffrement
    if (ctx->encryptionEnabled) {
        DisableEncryption();
        ctx->encryptionEnabled = false;
    }
}

// Fonction pour activer/désactiver le chiffrement
void ToggleEncryption(AppContext* ctx) {
    if (!ctx) return;
    
    ctx->encryptionEnabled = !ctx->encryptionEnabled;
    
    if (ctx->encryptionEnabled) {
        // Vérifier si un mot de passe est défini
        if (strlen(ctx->connectionPassword) < 4) {
            strcpy(ctx->connectionStatus, "Mot de passe trop court (min 4 caractères)");
            ctx->encryptionEnabled = false;
            return;
        }
        
        // Activer le chiffrement si connecté
        if (ctx->connectedPeerID >= 0) {
            if (EnableEncryption(ctx->connectionPassword)) {
                sprintf(ctx->connectionStatus, "Chiffrement activé pour le pair ID %d", 
                       ctx->connectedPeerID);
            } else {
                sprintf(ctx->connectionStatus, "Échec de l'activation du chiffrement");
                ctx->encryptionEnabled = false;
            }
        } else {
            sprintf(ctx->connectionStatus, "Chiffrement activé, sera utilisé à la connexion");
        }
    } else {
        // Désactiver le chiffrement
        if (ctx->connectedPeerID >= 0) {
            DisableEncryption();
            sprintf(ctx->connectionStatus, "Chiffrement désactivé pour le pair ID %d", 
                   ctx->connectedPeerID);
        } else {
            sprintf(ctx->connectionStatus, "Chiffrement désactivé");
        }
    }
}

const char* TextInputBox(Rectangle bounds, const char* title, const char* message, const char* defaultText) {
    static char inputBuffer[256] = {0};
    bool inputActive = true;
    int framesCounter = 0;
    
    // Initialize with default text if provided
    if (defaultText != NULL) strncpy(inputBuffer, defaultText, 255);
    
    while (inputActive && !WindowShouldClose()) {
        // Handle input
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (strlen(inputBuffer) < 255)) {
                int len = strlen(inputBuffer);
                inputBuffer[len] = (char)key;
                inputBuffer[len + 1] = '\0';
            }
            key = GetCharPressed();
        }
        
        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = strlen(inputBuffer);
            if (len > 0) inputBuffer[len-1] = '\0';
        }
        
        if (IsKeyPressed(KEY_ENTER)) inputActive = false;
        if (IsKeyPressed(KEY_ESCAPE)) {
            inputBuffer[0] = '\0';
            inputActive = false;
        }
        
        // Draw
        BeginDrawing();
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(RAYWHITE, 0.8f));
            DrawRectangleRec(bounds, WHITE);
            DrawRectangleLinesEx(bounds, 2, DARKGRAY);
            DrawText(title, bounds.x + 10, bounds.y + 10, 20, BLACK);
            DrawText(message, bounds.x + 10, bounds.y + 40, 18, DARKGRAY);
            
            DrawRectangle(bounds.x + 10, bounds.y + 70, bounds.width - 20, 30, LIGHTGRAY);
            DrawRectangleLinesEx((Rectangle){bounds.x + 10, bounds.y + 70, bounds.width - 20, 30}, 2, DARKGRAY);
            DrawText(inputBuffer, bounds.x + 15, bounds.y + 75, 18, BLACK);
            
            if ((framesCounter/20)%2 == 0) 
                DrawText("_", bounds.x + 15 + MeasureText(inputBuffer, 18), bounds.y + 75, 18, BLACK);
            
            DrawText("[ENTER] to Accept | [ESC] to Cancel", bounds.x + 10, bounds.y + bounds.height - 30, 15, DARKGRAY);
        EndDrawing();
        framesCounter++;
    }
    
    return inputBuffer[0] != '\0' ? inputBuffer : NULL;
}