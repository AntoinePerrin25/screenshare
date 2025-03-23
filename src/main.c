#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <raylib.h>

// Inclusion pour obtenir l'adresse IP local
#ifdef _WIN32
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

#include "../include/capture.h"
#include "../include/network.h"
#include "../include/ui.h"

// Constantes
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define APP_NAME "C_Screenshare - Peer-to-Peer Screen Sharing"
#define MAX_PEERS 10
#define MAX_IP_LENGTH 64
#define DEFAULT_PORT 7890

// États de l'application
typedef enum {
    APP_STATE_IDLE,       // En attente, pas de partage actif
    APP_STATE_SHARING,    // Partage d'écran en cours
    APP_STATE_VIEWING     // Visualisation d'un partage en cours
} AppState;

// Structure pour gérer un pair
typedef struct {
    char ip[MAX_IP_LENGTH];  // Adresse IP du pair
    int port;                // Port du pair
    bool connected;          // Indique si le pair est connecté
    bool isSharing;          // Indique si on partage l'écran avec ce pair
} PeerInfo;

// Structure principale contenant l'état de l'application
typedef struct {
    bool running;          // Indique si l'application est en cours d'exécution
    bool minimized;        // Indique si l'application est minimisée
    AppState state;        // État actuel de l'application
    int captureInterval;   // Intervalle de capture en ms
    int captureQuality;    // Qualité de la capture (0-100)
    bool encryptionEnabled; // Indique si le chiffrement est activé
    Rectangle captureRegion; // Région de capture (utilisée en mode partage)
    CaptureData currentCapture; // Dernière capture effectuée
    bool hasCaptureData;   // Indique si des données de capture sont disponibles
    UIPage currentPage;    // Page UI actuelle
    
    // Informations réseau
    char localIP[MAX_IP_LENGTH]; // IP locale de l'hôte
    int localPort;               // Port local
    PeerInfo peers[MAX_PEERS];   // Liste des pairs
    int peerCount;               // Nombre de pairs
    int selectedPeer;            // Index du pair sélectionné (-1 pour aucun)
    bool shareWithAll;           // Partager avec tous les pairs
    
    // UI
    bool addingPeer;             // Mode ajout de pair actif
    char inputBuffer[MAX_IP_LENGTH]; // Buffer pour l'entrée d'adresse IP
    int inputCursorIndex;        // Position du curseur dans le buffer
    bool inputActive;            // Indique si l'entrée est active
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
void AddPeer(AppContext* ctx, const char* ip, int port);
void RemovePeer(AppContext* ctx, int index);
void RenderTopBar(AppContext* ctx);
void HandlePeerManagement(AppContext* ctx);
void RenderPeerList(AppContext* ctx);

int main(void) {
    // Initialisation du contexte de l'application
    AppContext appContext = {0};
    appContext.running = true;
    appContext.state = APP_STATE_IDLE;
    appContext.captureInterval = 100; // 10 FPS par défaut
    appContext.captureQuality = 80;   // 80% de qualité par défaut
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
    
    ctx->peerCount = 0;
    ctx->selectedPeer = -1;
    ctx->shareWithAll = true;
    ctx->addingPeer = false;
    
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
        
        // Décaler l'affichage pour laisser de la place à la liste des pairs
        posX += 150;  // Décalage pour la liste des pairs
        
        // Afficher la texture
        DrawTexturePro(ctx->currentCapture.texture, 
                     (Rectangle){0, 0, (float)ctx->currentCapture.width, (float)ctx->currentCapture.height},
                     (Rectangle){(float)posX, (float)posY, (float)displayWidth, (float)displayHeight},
                     (Vector2){0, 0}, 0.0f, WHITE);
        
        // Afficher des informations sur la capture
        DrawText(TextFormat("Capture: %dx%d", ctx->currentCapture.width, ctx->currentCapture.height), 
                 310, 40, 20, DARKGRAY);
                 
        if (ctx->currentCapture.isCompressed) {
            DrawText(TextFormat("Compression: %d octets (Ratio: %.2f:1)", 
                   ctx->currentCapture.compressedSize,
                   (float)(ctx->currentCapture.width * ctx->currentCapture.height * 4) / ctx->currentCapture.compressedSize), 
                   310, 70, 20, DARKGRAY);
        }
    }
    
    // Afficher la liste des pairs et options de partage
    RenderPeerList(ctx);
    
    // Affichage de l'état et des contrôles
    DrawText(ctx->state == APP_STATE_SHARING ? "État: Partage en cours" : "État: En attente", 
             310, GetScreenHeight() - 60, 20, ctx->state == APP_STATE_SHARING ? GREEN : GRAY);
    
    DrawText("Appuyez sur S pour démarrer/arrêter le partage", 
             310, GetScreenHeight() - 30, 20, DARKGRAY);
    
    EndDrawing();
}

void HandleEvents(AppContext* ctx) {
    if (!ctx || !ctx->running) return;
    
    // Gestion des touches
    if (IsKeyPressed(KEY_S)) {
        ToggleSharing(ctx);
    }
    
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (ctx->addingPeer) {
            ctx->addingPeer = false;
            ctx->inputActive = false;
        } else {
            ctx->running = false;
        }
    }
    
    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
    }
    
    // Gestion de la saisie d'adresse IP
    if (ctx->addingPeer && ctx->inputActive) {
        int key = GetCharPressed();
        
        // Traiter les caractères pour l'adresse IP (chiffres, points)
        while (key > 0) {
            // Vérifier si le caractère est valide pour une adresse IP (chiffres et points)
            if ((key >= '0' && key <= '9') || key == '.') {
                if (ctx->inputCursorIndex < MAX_IP_LENGTH - 1) {
                    ctx->inputBuffer[ctx->inputCursorIndex] = (char)key;
                    ctx->inputCursorIndex++;
                    ctx->inputBuffer[ctx->inputCursorIndex] = '\0';
                }
            }
            
            key = GetCharPressed();  // Vérifier le prochain caractère
        }
        
        // Gestion des touches spéciales
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (ctx->inputCursorIndex > 0) {
                ctx->inputCursorIndex--;
                ctx->inputBuffer[ctx->inputCursorIndex] = '\0';
            }
        }
        
        if (IsKeyPressed(KEY_ENTER)) {
            if (strlen(ctx->inputBuffer) > 0) {
                AddPeer(ctx, ctx->inputBuffer, DEFAULT_PORT);
                ctx->addingPeer = false;
                ctx->inputActive = false;
            }
        }
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

// Fonction pour ajouter un pair
void AddPeer(AppContext* ctx, const char* ip, int port) {
    if (!ctx || !ip) return;
    
    // Vérifier si le pair existe déjà
    for (int i = 0; i < ctx->peerCount; i++) {
        if (strcmp(ctx->peers[i].ip, ip) == 0 && ctx->peers[i].port == port) {
            printf("[INFO] Le pair %s:%d existe déjà\n", ip, port);
            return;
        }
    }
    
    // Vérifier si la liste est pleine
    if (ctx->peerCount >= MAX_PEERS) {
        printf("[WARNING] Impossible d'ajouter plus de pairs (maximum: %d)\n", MAX_PEERS);
        return;
    }
    
    // Ajouter le nouveau pair
    strncpy(ctx->peers[ctx->peerCount].ip, ip, MAX_IP_LENGTH);
    ctx->peers[ctx->peerCount].port = port;
    ctx->peers[ctx->peerCount].connected = false;  // Initialement non connecté
    ctx->peers[ctx->peerCount].isSharing = ctx->shareWithAll;  // Partage selon le réglage global
    
    printf("[INFO] Nouveau pair ajouté: %s:%d\n", ip, port);
    ctx->peerCount++;
}

// Fonction pour supprimer un pair
void RemovePeer(AppContext* ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->peerCount) return;
    
    printf("[INFO] Pair supprimé: %s:%d\n", ctx->peers[index].ip, ctx->peers[index].port);
    
    // Déplacer tous les pairs après l'index vers le haut
    for (int i = index; i < ctx->peerCount - 1; i++) {
        ctx->peers[i] = ctx->peers[i + 1];
    }
    
    ctx->peerCount--;
    
    // Si le pair sélectionné est supprimé, réinitialiser la sélection
    if (ctx->selectedPeer == index) {
        ctx->selectedPeer = -1;
    }
    // Si le pair sélectionné était après celui supprimé, ajuster l'index
    else if (ctx->selectedPeer > index) {
        ctx->selectedPeer--;
    }
}

// Fonction pour dessiner la barre supérieure avec l'IP
void RenderTopBar(AppContext* ctx) {
    // Fond de la barre supérieure
    DrawRectangle(0, 0, GetScreenWidth(), 30, DARKGRAY);
    
    // Afficher l'IP locale
    DrawText(TextFormat("IP: %s:%d", ctx->localIP, ctx->localPort), 10, 5, 20, WHITE);
    
    // Afficher le nombre de pairs
    DrawText(TextFormat("Pairs: %d/%d", ctx->peerCount, MAX_PEERS), GetScreenWidth() - 150, 5, 20, WHITE);
    
    // Bouton pour ajouter un pair
    Rectangle addButton = { GetScreenWidth() - 40, 5, 25, 20 };
    DrawRectangleRec(addButton, ctx->addingPeer ? DARKBLUE : BLUE);
    DrawText("+", GetScreenWidth() - 30, 5, 20, WHITE);
    
    // Si on clique sur le bouton
    if (CheckCollisionPointRec(GetMousePosition(), addButton) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ctx->addingPeer = !ctx->addingPeer;
        if (ctx->addingPeer) {
            // Initialiser le buffer pour l'entrée d'une nouvelle IP
            memset(ctx->inputBuffer, 0, MAX_IP_LENGTH);
            ctx->inputCursorIndex = 0;
            ctx->inputActive = true;
        }
    }
    
    // Si on est en mode ajout de pair, afficher la zone de saisie
    if (ctx->addingPeer) {
        // Fond de la zone de saisie
        DrawRectangle(GetScreenWidth() / 2 - 150, 35, 300, 30, LIGHTGRAY);
        DrawText("Adresse IP du pair:", GetScreenWidth() / 2 - 140, 40, 20, BLACK);
        
        // Champ de saisie
        Rectangle inputField = { GetScreenWidth() / 2 - 140, 65, 280, 30 };
        DrawRectangleRec(inputField, WHITE);
        DrawRectangleLinesEx(inputField, 2, ctx->inputActive ? BLUE : GRAY);
        DrawText(ctx->inputBuffer, inputField.x + 5, inputField.y + 5, 20, BLACK);
        
        // Curseur
        if (ctx->inputActive) {
            // Calculer la position du curseur
            int cursorPosX = inputField.x + 5 + MeasureText(ctx->inputBuffer, 20);
            DrawRectangle(cursorPosX, inputField.y + 5, 2, 20, (((int)(GetTime() * 2)) % 2) ? BLACK : WHITE);
            
            // Bouton "Ajouter"
            Rectangle addPeerButton = { GetScreenWidth() / 2 - 60, 100, 120, 30 };
            DrawRectangleRec(addPeerButton, GREEN);
            DrawText("Ajouter", addPeerButton.x + 20, addPeerButton.y + 5, 20, WHITE);
            
            // Gestion du clic sur le bouton "Ajouter"
            if (CheckCollisionPointRec(GetMousePosition(), addPeerButton) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                if (strlen(ctx->inputBuffer) > 0) {
                    AddPeer(ctx, ctx->inputBuffer, DEFAULT_PORT);
                    ctx->addingPeer = false;
                    ctx->inputActive = false;
                }
            }
        }
        
        // Clic en dehors de la zone d'ajout de pair pour annuler
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && 
            !CheckCollisionPointRec(GetMousePosition(), inputField) && 
            !CheckCollisionPointRec(GetMousePosition(), addButton)) {
            ctx->addingPeer = false;
            ctx->inputActive = false;
        }
    }
}

// Fonction pour gérer la liste des pairs
void RenderPeerList(AppContext* ctx) {
    if (!ctx) return;
    
    // Fond de la liste des pairs
    DrawRectangle(5, 150, 300, GetScreenHeight() - 200, LIGHTGRAY);
    DrawText("Pairs connectés:", 15, 160, 20, BLACK);
    
    // Afficher chaque pair
    for (int i = 0; i < ctx->peerCount; i++) {
        Rectangle peerRect = { 10, 190 + i * 40, 290, 35 };
        
        // Fond de l'entrée du pair (sélectionné ou non)
        if (i == ctx->selectedPeer) {
            DrawRectangleRec(peerRect, SKYBLUE);
        } else {
            DrawRectangleRec(peerRect, WHITE);
        }
        
        // Informations du pair
        DrawText(TextFormat("%s:%d", ctx->peers[i].ip, ctx->peers[i].port), 
                 peerRect.x + 5, peerRect.y + 5, 20, BLACK);
        
        // Indicateur de partage
        if (ctx->peers[i].isSharing) {
            DrawRectangle(peerRect.x + 250, peerRect.y + 5, 25, 25, GREEN);
            DrawText("✓", peerRect.x + 258, peerRect.y + 5, 20, WHITE);
        } else {
            DrawRectangle(peerRect.x + 250, peerRect.y + 5, 25, 25, RED);
            DrawText("✗", peerRect.x + 258, peerRect.y + 5, 20, WHITE);
        }
        
        // Gestion du clic sur un pair
        if (CheckCollisionPointRec(GetMousePosition(), peerRect) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            ctx->selectedPeer = i;
        }
        
        // Gestion du double-clic pour basculer le partage avec ce pair
        static float lastClickTime = 0;
        static int lastClickedPeer = -1;
        
        if (i == ctx->selectedPeer && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && 
            CheckCollisionPointRec(GetMousePosition(), peerRect)) {
            
            float currentTime = GetTime();
            
            if (lastClickedPeer == i && (currentTime - lastClickTime) < 0.5f) {
                // Double-clic détecté
                ctx->peers[i].isSharing = !ctx->peers[i].isSharing;
                lastClickTime = 0;  // Réinitialiser pour éviter les triples clics
            } else {
                lastClickTime = currentTime;
                lastClickedPeer = i;
            }
        }
    }
    
    // Contrôle pour partager avec tous les pairs
    Rectangle shareAllRect = { 10, GetScreenHeight() - 45, 290, 30 };
    DrawRectangleRec(shareAllRect, ctx->shareWithAll ? GREEN : LIGHTGRAY);
    DrawText("Partager avec tous", shareAllRect.x + 10, shareAllRect.y + 5, 20, BLACK);
    
    // Gestion du clic sur "Partager avec tous"
    if (CheckCollisionPointRec(GetMousePosition(), shareAllRect) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ctx->shareWithAll = !ctx->shareWithAll;
        
        // Mettre à jour le statut de partage pour tous les pairs
        for (int i = 0; i < ctx->peerCount; i++) {
            ctx->peers[i].isSharing = ctx->shareWithAll;
        }
    }
}