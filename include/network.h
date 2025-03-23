#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stdint.h>
#include "capture.h"
#include "rnet.h"

/**
 * @brief Structure contenant les informations d'un pair connecté
 */
typedef struct {
    int id;                     // Identifiant unique du pair
    char address[64];           // Adresse IP du pair
    int port;                   // Port du pair
    bool isConnected;           // État de la connexion
    uint64_t lastPacketTime;    // Horodatage du dernier paquet reçu
} Peer;

/**
 * @brief Structure contenant les informations de session pour le chiffrement
 */
typedef struct {
    uint8_t key[32];            // Clé de chiffrement
    uint8_t iv[16];             // Vecteur d'initialisation
    bool isEncryptionEnabled;   // Indique si le chiffrement est activé
} EncryptionSession;

/**
 * @brief Initialise le système réseau
 * @param port Port d'écoute local
 * @return true si l'initialisation réussit, false sinon
 */
bool InitNetworkSystem(int port);

/**
 * @brief Termine le système réseau et libère les ressources
 */
void CloseNetworkSystem(void);

/**
 * @brief Connecte à un pair distant
 * @param address Adresse IP du pair
 * @param port Port du pair
 * @return ID du pair si la connexion réussit, -1 sinon
 */
int ConnectToPeer(const char* address, int port);

/**
 * @brief Déconnecte d'un pair
 * @param peerId ID du pair à déconnecter
 */
void DisconnectFromPeer(int peerId);

/**
 * @brief Envoie des données de capture à un pair spécifique
 * @param peerId ID du pair destinataire (-1 pour tous les pairs)
 * @param captureData Données de capture à envoyer
 * @return true si l'envoi réussit, false sinon
 */
bool SendCaptureData(int peerId, const CaptureData* captureData);

/**
 * @brief Reçoit et traite les paquets entrants
 * @return Nombre de paquets traités
 */
int ProcessNetworkEvents(void);

/**
 * @brief Active le chiffrement pour la session
 * @param password Mot de passe utilisé pour générer la clé de chiffrement
 * @return true si l'activation réussit, false sinon
 */
bool EnableEncryption(const char* password);

/**
 * @brief Désactive le chiffrement pour la session
 */
void DisableEncryption(void);

/**
 * @brief Chiffre les données d'une capture
 * @param captureData Pointeur vers la structure CaptureData à chiffrer
 * @return true si le chiffrement réussit, false sinon
 */
bool EncryptCaptureData(CaptureData* captureData);

/**
 * @brief Déchiffre les données d'une capture
 * @param captureData Pointeur vers la structure CaptureData à déchiffrer
 * @return true si le déchiffrement réussit, false sinon
 */
bool DecryptCaptureData(CaptureData* captureData);

#endif // NETWORK_H