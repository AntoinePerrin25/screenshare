#ifndef CAPTURE_H
#define CAPTURE_H

#include <raylib.h>
#include <stdint.h>
#include <stdbool.h>

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
} CaptureData;

/**
 * @brief Initialise le système de capture d'écran
 * @return true si l'initialisation réussit, false sinon
 */
bool InitCaptureSystem(void);

/**
 * @brief Termine le système de capture d'écran et libère les ressources
 */
void CloseCaptureSystem(void);

/**
 * @brief Capture l'écran entier
 * @return Structure CaptureData contenant l'image capturée
 */
CaptureData CaptureScreen(void);

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

#endif // CAPTURE_H