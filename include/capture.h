#ifndef CAPTURE_H
#define CAPTURE_H
#define RAYLIB
#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>

// Inclusions pour les API Windows
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

/**
 * @brief Types de capture d'écran
 */
typedef enum {
    CAPTURE_METHOD_RAYLIB,    // Méthode utilisant raylib (peut être limitée à la fenêtre)
    CAPTURE_METHOD_WIN_GDI,   // Méthode utilisant Windows GDI (BitBlt)
    CAPTURE_METHOD_AUTO       // Sélection automatique de la meilleure méthode
} CaptureMethod;

/**
 * @brief Information sur un moniteur
 */
typedef struct {
    int index;                // Index du moniteur
    char name[128];           // Nom du moniteur
    int width;                // Largeur en pixels
    int height;               // Hauteur en pixels
    int x;                    // Position X du moniteur dans l'espace d'écran virtuel
    int y;                    // Position Y du moniteur dans l'espace d'écran virtuel
    bool isPrimary;           // Indique si c'est le moniteur principal
} MonitorInfo;

/**
 * @brief Configuration du système de capture
 */
typedef struct {
    CaptureMethod method;           // Méthode de capture
    int quality;                    // Qualité de compression (0-100)
    int captureInterval;            // Intervalle entre captures en ms
    bool detectChanges;             // Activer la détection de changement
    int changeThreshold;            // Seuil pour considérer qu'un changement a eu lieu (0-100)
    bool autoAdjustQuality;         // Ajuster automatiquement la qualité
    int targetMonitor;              // Index du moniteur cible (-1 pour tous)
} CaptureConfig;

/**
 * @brief Structure contenant les données d'une capture d'écran
 */
typedef struct {
    Image image;                 // Image brute capturée
    Texture2D texture;           // Texture pour l'affichage
    unsigned char* compressedData; // Données compressées pour la transmission
    int compressedSize;          // Taille des données compressées
    uint8_t* encryptedData;      // Données chiffrées
    int encryptedSize;           // Taille des données chiffrées
    int width;                   // Largeur de l'image
    int height;                  // Hauteur de l'image
    bool isCompressed;           // Indique si les données sont compressées
    bool isEncrypted;            // Indique si les données sont chiffrées
    unsigned char* previousFrame; // Image précédente pour la détection de changements
    bool hasChanged;             // Indique si l'image a changé depuis la dernière capture
    int monitorIndex;            // Index du moniteur capturé (-1 si combiné)
    uint64_t timestamp;          // Timestamp de la capture
} CaptureData;

/**
 * @brief Initialise le système de capture d'écran
 * @param config Configuration de capture (NULL pour utiliser les valeurs par défaut)
 * @return true si l'initialisation réussit, false sinon
 */
bool InitCaptureSystem(CaptureConfig* config);

/**
 * @brief Termine le système de capture d'écran et libère les ressources
 */
void CloseCaptureSystem(void);

/**
 * @brief Obtient la liste des moniteurs disponibles
 * @param monitors Tableau pour stocker les informations des moniteurs (peut être NULL)
 * @param maxMonitors Taille maximale du tableau
 * @return Nombre de moniteurs détectés
 */
int GetMonitorsInfo(MonitorInfo* monitors, int maxMonitors);

/**
 * @brief Capture l'écran entier (tous les moniteurs ou celui spécifié dans la config)
 * @return Structure CaptureData contenant l'image capturée
 */
CaptureData CaptureScreen(void);

/**
 * @brief Capture un moniteur spécifique
 * @param monitorIndex Index du moniteur à capturer
 * @return Structure CaptureData contenant l'image capturée
 */
CaptureData CaptureMonitor(int monitorIndex);

/**
 * @brief Capture une région spécifique de l'écran
 * @param region Rectangle définissant la région à capturer
 * @return Structure CaptureData contenant l'image capturée
 */
CaptureData CaptureScreenRegion(Rectangle region);

/**
 * @brief Libère les ressources associées à une capture d'écran
 * @param capture Pointeur vers la structure CaptureData à libérer
 */
void UnloadCaptureData(CaptureData* capture);

/**
 * @brief Compresse les données de l'image pour la transmission
 * @param capture Pointeur vers la structure CaptureData à compresser
 * @param quality Niveau de qualité (0-100, 100 étant la meilleure qualité)
 * @return true si la compression réussit, false sinon
 */
bool CompressCaptureData(CaptureData* capture, int quality);

/**
 * @brief Détecte si l'image a changé significativement depuis la dernière capture
 * @param capture Pointeur vers la structure CaptureData actuelle
 * @param threshold Seuil de changement (0-100, 0 = tout changement, 100 = aucun changement)
 * @return true si l'image a changé, false sinon
 */
bool DetectChanges(CaptureData* capture, int threshold);

/**
 * @brief Met à jour la configuration de capture
 * @param config Nouvelle configuration
 * @return true si la mise à jour réussit, false sinon
 */
bool UpdateCaptureConfig(CaptureConfig config);

/**
 * @brief Obtient la configuration actuelle de capture
 * @return Structure CaptureConfig contenant la configuration actuelle
 */
CaptureConfig GetCaptureConfig(void);

#endif // CAPTURE_H