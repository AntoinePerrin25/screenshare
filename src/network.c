#define NETWORK_IMPL

#include "../include/rnet.h"
#include "../include/network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Constantes
#define MAX_PEERS 32
#define CONNECTION_TIMEOUT 5000 // ms
#define PACKET_TYPE_CAPTURE 1
#define PACKET_TYPE_CONTROL 2
#define PACKET_TYPE_HANDSHAKE 3

// Structure d'en-tête de paquet
typedef struct {
    uint8_t type;       // Type de paquet (capture, contrôle, handshake)
    uint8_t flags;      // Drapeaux spécifiques au type de paquet
    uint16_t sequence;  // Numéro de séquence pour la vérification de l'ordre
    uint32_t timestamp; // Horodatage pour mesurer la latence
    uint32_t dataSize;  // Taille des données
} PacketHeader;

// Variables statiques
static bool networkInitialized = false;
static rnetPeer* hostPeer = NULL;
static Peer connectedPeers[MAX_PEERS] = {0};
static int peerCount = 0;
static uint16_t nextSequence = 0;
static EncryptionSession encSession = {0};

// Fonctions utilitaires privées
static int FindPeerById(int id);
static int FindPeerByAddress(const char* address, int port);
static int AddPeer(const char* address, int port);
static void UpdatePeerStatus(int index, bool isConnected);
static bool SendPacket(int peerId, uint8_t type, const void* data, uint32_t size, int flags);
static void HandleCapturePacket(const PacketHeader* header, const void* data, size_t size, int senderId);
static void HandleControlPacket(const PacketHeader* header, const void* data, size_t size, int senderId);
static void HandleHandshakePacket(const PacketHeader* header, const void* data, size_t size, int senderId);

// Implémentation des fonctions publiques
bool InitNetworkSystem(int port) {
    if (networkInitialized) {
        printf("[INFO] Système réseau déjà initialisé\n");
        return true;
    }
    
    // Initialisation de rnet
    if (!rnetInit()) {
        printf("[ERROR] Échec de l'initialisation de rnet\n");
        return false;
    }
    
    // Création d'un hôte sur le port spécifié
    hostPeer = rnetHost((uint16_t)port);
    if (!hostPeer) {
        printf("[ERROR] Impossible de créer un hôte sur le port %d\n", port);
        rnetShutdown();
        return false;
    }
    
    // Initialisation des tableaux et variables
    memset(connectedPeers, 0, sizeof(connectedPeers));
    peerCount = 0;
    nextSequence = 0;
    
    networkInitialized = true;
    printf("[INFO] Système réseau initialisé sur le port %d\n", port);
    return true;
}

void CloseNetworkSystem(void) {
    if (!networkInitialized) return;
    
    // Déconnexion de tous les pairs
    for (int i = 0; i < peerCount; i++) {
        if (connectedPeers[i].isConnected) {
            DisconnectFromPeer(connectedPeers[i].id);
        }
    }
    
    // Fermeture de l'hôte
    if (hostPeer) {
        rnetClose(hostPeer);
        hostPeer = NULL;
    }
    
    // Arrêt de rnet
    rnetShutdown();
    
    networkInitialized = false;
    printf("[INFO] Système réseau fermé\n");
}

int ConnectToPeer(const char* address, int port) {
    if (!networkInitialized || !hostPeer) {
        printf("[ERROR] Système réseau non initialisé\n");
        return -1;
    }
    
    // Vérifier si on est déjà connecté à ce pair
    int existingIndex = FindPeerByAddress(address, port);
    if (existingIndex >= 0) {
        if (connectedPeers[existingIndex].isConnected) {
            printf("[INFO] Déjà connecté au pair %s:%d (ID %d)\n", 
                  address, port, connectedPeers[existingIndex].id);
            return connectedPeers[existingIndex].id;
        } else {
            // Le pair existe mais n'est pas connecté, on réutilise son entrée
            printf("[INFO] Reconnexion au pair %s:%d (ID %d)\n", 
                  address, port, connectedPeers[existingIndex].id);
        }
    } else {
        // Ajouter un nouveau pair
        existingIndex = AddPeer(address, port);
        if (existingIndex < 0) {
            printf("[ERROR] Impossible d'ajouter un nouveau pair, limite atteinte\n");
            return -1;
        }
    }
    
    // Création d'une connexion vers le pair
    rnetPeer* connectionPeer = rnetConnect(address, (uint16_t)port);
    if (!connectionPeer) {
        printf("[ERROR] Échec de la connexion à %s:%d\n", address, port);
        return -1;
    }
    
    // Mise à jour du statut de connexion
    UpdatePeerStatus(existingIndex, true);
    connectedPeers[existingIndex].lastPacketTime = time(NULL);
    
    // Envoi d'un paquet de handshake
    char handshakeData[128] = {0};
    sprintf(handshakeData, "C_Screenshare Handshake");
    
    if (!SendPacket(connectedPeers[existingIndex].id, PACKET_TYPE_HANDSHAKE, 
                  handshakeData, strlen(handshakeData) + 1, RNET_RELIABLE)) {
        printf("[ERROR] Échec de l'envoi du handshake à %s:%d\n", address, port);
        rnetClose(connectionPeer);
        UpdatePeerStatus(existingIndex, false);
        return -1;
    }
    
    // Fermeture de la connexion temporaire (la connexion est maintenue par hostPeer)
    rnetClose(connectionPeer);
    
    printf("[INFO] Connexion établie avec %s:%d (ID %d)\n", 
           address, port, connectedPeers[existingIndex].id);
    
    return connectedPeers[existingIndex].id;
}

void DisconnectFromPeer(int peerId) {
    if (!networkInitialized || !hostPeer) {
        printf("[ERROR] Système réseau non initialisé\n");
        return;
    }
    
    int index = FindPeerById(peerId);
    if (index < 0) {
        printf("[ERROR] Pair avec ID %d non trouvé\n", peerId);
        return;
    }
    
    // Mise à jour du statut de connexion
    UpdatePeerStatus(index, false);
    
    printf("[INFO] Déconnexion du pair %s:%d (ID %d)\n", 
           connectedPeers[index].address, connectedPeers[index].port, peerId);
}

bool SendCaptureData(int peerId, const CaptureData* captureData) {
    if (!networkInitialized || !hostPeer || !captureData) {
        printf("[ERROR] Système réseau non initialisé ou données de capture invalides\n");
        return false;
    }
    
    // Vérifier si on a des données compressées
    if (!captureData->isCompressed || !captureData->compressedData || captureData->compressedSize <= 0) {
        printf("[ERROR] Les données de capture doivent être compressées avant envoi\n");
        return false;
    }
    
    // Créer une structure pour les métadonnées
    struct {
        int width;
        int height;
        int dataSize;
        bool hasChanged;
        uint64_t timestamp;
        int monitorIndex;
    } metadata = {
        captureData->width,
        captureData->height,
        captureData->compressedSize,
        captureData->hasChanged,
        captureData->timestamp,
        captureData->monitorIndex
    };
    
    // Allouer un buffer pour les métadonnées + données compressées
    size_t totalSize = sizeof(metadata) + captureData->compressedSize;
    uint8_t* buffer = (uint8_t*)malloc(totalSize);
    if (!buffer) {
        printf("[ERROR] Échec d'allocation mémoire pour l'envoi de données\n");
        return false;
    }
    
    // Copier les métadonnées et les données dans le buffer
    memcpy(buffer, &metadata, sizeof(metadata));
    memcpy(buffer + sizeof(metadata), captureData->compressedData, captureData->compressedSize);
    
    // Chiffrer les données si nécessaire
    if (encSession.isEncryptionEnabled) {
        // TODO: Implémenter le chiffrement, non implémenté dans cette version
    }
    
    // Envoyer le paquet
    bool success = false;
    
    if (peerId < 0) {
        // Envoi à tous les pairs connectés
        bool allSuccess = true;
        for (int i = 0; i < peerCount; i++) {
            if (connectedPeers[i].isConnected) {
                if (!SendPacket(connectedPeers[i].id, PACKET_TYPE_CAPTURE, 
                              buffer, totalSize, RNET_UNRELIABLE)) {
                    printf("[ERROR] Échec de l'envoi au pair ID %d\n", connectedPeers[i].id);
                    allSuccess = false;
                }
            }
        }
        success = allSuccess;
    } else {
        // Envoi à un pair spécifique
        success = SendPacket(peerId, PACKET_TYPE_CAPTURE, buffer, totalSize, RNET_UNRELIABLE);
    }
    
    free(buffer);
    return success;
}

int ProcessNetworkEvents(void) {
    if (!networkInitialized || !hostPeer) {
        return 0;
    }
    
    rnetPacket packet;
    int processedPackets = 0;
    
    // Traiter tous les paquets en attente
    while (rnetReceive(hostPeer, &packet)) {
        processedPackets++;
        
        // Obtenir le pair qui a envoyé le paquet
        rnetTargetPeer* sender = rnetGetLastEventPeer(hostPeer);
        
        // Vérifier que le paquet a une taille minimale pour l'en-tête
        if (packet.size < sizeof(PacketHeader)) {
            printf("[ERROR] Paquet reçu trop petit pour contenir un en-tête\n");
            rnetFreePacket(&packet);
            continue;
        }
        
        // Extraire l'en-tête et les données
        PacketHeader* header = (PacketHeader*)packet.data;
        void* data = (uint8_t*)packet.data + sizeof(PacketHeader);
        size_t dataSize = packet.size - sizeof(PacketHeader);
        
        // Vérifier la taille des données
        if (dataSize < header->dataSize) {
            printf("[ERROR] Taille des données inconsistante\n");
            rnetFreePacket(&packet);
            continue;
        }
        
        // Trouver l'ID du pair expéditeur
        int senderId = -1;
        // TODO: Implémenter la recherche du pair basée sur sender
        
        // Traiter le paquet selon son type
        switch (header->type) {
            case PACKET_TYPE_CAPTURE:
                HandleCapturePacket(header, data, dataSize, senderId);
                break;
                
            case PACKET_TYPE_CONTROL:
                HandleControlPacket(header, data, dataSize, senderId);
                break;
                
            case PACKET_TYPE_HANDSHAKE:
                HandleHandshakePacket(header, data, dataSize, senderId);
                break;
                
            default:
                printf("[ERROR] Type de paquet inconnu: %d\n", header->type);
                break;
        }
        
        rnetFreePacket(&packet);
    }
    
    return processedPackets;
}

bool EnableEncryption(const char* password) {
    if (!password) return false;
    
    // Génération simple de clé à partir du mot de passe (à améliorer dans une version future)
    memset(&encSession.key, 0, sizeof(encSession.key));
    memset(&encSession.iv, 0, sizeof(encSession.iv));
    
    size_t pwdLen = strlen(password);
    for (size_t i = 0; i < pwdLen && i < sizeof(encSession.key); i++) {
        encSession.key[i] = password[i];
    }
    
    // Initialiser le vecteur d'initialisation avec une valeur temporelle
    uint64_t timestamp = time(NULL);
    memcpy(encSession.iv, &timestamp, sizeof(timestamp));
    
    encSession.isEncryptionEnabled = true;
    printf("[INFO] Chiffrement activé\n");
    return true;
}

void DisableEncryption(void) {
    memset(&encSession.key, 0, sizeof(encSession.key));
    memset(&encSession.iv, 0, sizeof(encSession.iv));
    encSession.isEncryptionEnabled = false;
    printf("[INFO] Chiffrement désactivé\n");
}

bool EncryptCaptureData(CaptureData* captureData) {
    if (!captureData || !encSession.isEncryptionEnabled) return false;
    
    // TODO: Implémenter le chiffrement des données
    // Dans cette étape, nous nous concentrons sur la connexion P2P de base
    // Le chiffrement sera implémenté dans une version future
    
    return true;
}

bool DecryptCaptureData(CaptureData* captureData) {
    if (!captureData || !encSession.isEncryptionEnabled) return false;
    
    // TODO: Implémenter le déchiffrement des données
    // Dans cette étape, nous nous concentrons sur la connexion P2P de base
    // Le déchiffrement sera implémenté dans une version future
    
    return true;
}

// Implémentation des fonctions utilitaires privées
static int FindPeerById(int id) {
    for (int i = 0; i < peerCount; i++) {
        if (connectedPeers[i].id == id) {
            return i;
        }
    }
    return -1;
}

static int FindPeerByAddress(const char* address, int port) {
    for (int i = 0; i < peerCount; i++) {
        if (strcmp(connectedPeers[i].address, address) == 0 && connectedPeers[i].port == port) {
            return i;
        }
    }
    return -1;
}

static int AddPeer(const char* address, int port) {
    if (peerCount >= MAX_PEERS) {
        return -1;
    }
    
    int index = peerCount++;
    connectedPeers[index].id = index + 1; // IDs commencent à 1
    strncpy(connectedPeers[index].address, address, sizeof(connectedPeers[index].address) - 1);
    connectedPeers[index].port = port;
    connectedPeers[index].isConnected = false;
    connectedPeers[index].lastPacketTime = 0;
    
    return index;
}

static void UpdatePeerStatus(int index, bool isConnected) {
    if (index >= 0 && index < peerCount) {
        connectedPeers[index].isConnected = isConnected;
        if (isConnected) {
            connectedPeers[index].lastPacketTime = time(NULL);
        }
    }
}

static bool SendPacket(int peerId, uint8_t type, const void* data, uint32_t size, int flags) {
    if (!networkInitialized || !hostPeer) return false;
    
    int index = FindPeerById(peerId);
    if (index < 0) {
        printf("[ERROR] Pair avec ID %d non trouvé\n", peerId);
        return false;
    }
    
    // Créer l'en-tête
    PacketHeader header = {
        .type = type,
        .flags = 0,
        .sequence = nextSequence++,
        .timestamp = (uint32_t)time(NULL),
        .dataSize = size
    };
    
    // Allouer un buffer pour l'en-tête + données
    size_t totalSize = sizeof(header) + size;
    uint8_t* buffer = (uint8_t*)malloc(totalSize);
    if (!buffer) {
        printf("[ERROR] Échec d'allocation mémoire pour l'envoi de paquet\n");
        return false;
    }
    
    // Copier l'en-tête et les données
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), data, size);
    
    // Envoyer le paquet
    bool success = rnetSend(hostPeer, buffer, totalSize, flags);
    
    free(buffer);
    return success;
}

static void HandleCapturePacket(const PacketHeader* header, const void* data, size_t size, int senderId) {
    printf("[INFO] Paquet de capture reçu du pair %d\n", senderId);
    
    // TODO: Implémenter le traitement des paquets de capture
    // Cette fonction sera complétée dans la phase d'intégration avec main.c
}

static void HandleControlPacket(const PacketHeader* header, const void* data, size_t size, int senderId) {
    printf("[INFO] Paquet de contrôle reçu du pair %d\n", senderId);
    
    // TODO: Implémenter le traitement des paquets de contrôle
    // Cette fonction sera complétée dans la phase d'intégration avec main.c
}

static void HandleHandshakePacket(const PacketHeader* header, const void* data, size_t size, int senderId) {
    printf("[INFO] Paquet de handshake reçu du pair %d\n", senderId);
    
    // Vérifier les données de handshake
    const char* handshakeData = (const char*)data;
    if (strcmp(handshakeData, "C_Screenshare Handshake") == 0) {
        printf("[INFO] Handshake valide du pair %d\n", senderId);
        
        // Mise à jour du statut de connexion
        int index = FindPeerById(senderId);
        if (index >= 0) {
            UpdatePeerStatus(index, true);
        }
        
        // Répondre au handshake si nécessaire
        // TODO: Implémenter la réponse au handshake
    } else {
        printf("[ERROR] Handshake invalide du pair %d\n", senderId);
    }
}