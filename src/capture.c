#include "../include/capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Variables statiques pour le système de capture
static bool captureSystemInitialized = false;
static int screenWidth = 0;
static int screenHeight = 0;

bool InitCaptureSystem(void) {
    if (captureSystemInitialized) return true;  // Déjà initialisé
    
    // Récupération des dimensions de l'écran physique avec GetMonitorWidth/Height
    // au lieu de GetScreenWidth/Height qui retourne les dimensions de la fenêtre
    screenWidth = GetMonitorWidth(0);   // Moniteur principal (index 0)
    screenHeight = GetMonitorHeight(0);
    
    if (screenWidth <= 0 || screenHeight <= 0) {
        printf("[ERROR] Impossible d'obtenir les dimensions de l'écran\n");
        return false;
    }
    
    printf("[INFO] Système de capture initialisé: Écran %dx%d\n", screenWidth, screenHeight);
    captureSystemInitialized = true;
    return true;
}

void CloseCaptureSystem(void) {
    if (!captureSystemInitialized) return;
    
    captureSystemInitialized = false;
    screenWidth = 0;
    screenHeight = 0;
    
    printf("[INFO] Système de capture terminé\n");
}

CaptureData CaptureScreen(void) {
    CaptureData captureData = {0};
    
    if (!captureSystemInitialized) {
        printf("[ERROR] Le système de capture n'est pas initialisé\n");
        return captureData;
    }
    
    // Initialisation des dimensions
    captureData.width = screenWidth;
    captureData.height = screenHeight;
    
    // Capture de l'écran avec raylib
    captureData.image = LoadImageFromScreen();
    
    // Création de la texture pour l'affichage
    captureData.texture = LoadTextureFromImage(captureData.image);
    
    // Initialisation des autres champs
    captureData.compressedData = NULL;
    captureData.compressedSize = 0;
    captureData.encryptedData = NULL;
    captureData.encryptedSize = 0;
    captureData.isCompressed = false;
    captureData.isEncrypted = false;
    
    return captureData;
}

CaptureData CaptureScreenRegion(Rectangle region) {
    CaptureData captureData = {0};
    
    if (!captureSystemInitialized) {
        printf("[ERROR] Le système de capture n'est pas initialisé\n");
        return captureData;
    }
    
    // Vérification des limites de la région
    if (region.x < 0) region.x = 0;
    if (region.y < 0) region.y = 0;
    if (region.x + region.width > screenWidth) region.width = screenWidth - region.x;
    if (region.y + region.height > screenHeight) region.height = screenHeight - region.y;
    
    if (region.width <= 0 || region.height <= 0) {
        printf("[ERROR] Région de capture invalide\n");
        return captureData;
    }
    
    // Initialisation des dimensions
    captureData.width = (int)region.width;
    captureData.height = (int)region.height;
    printf("[INFO] Capture de la région: %.0fx%.0f\n", region.width, region.height);
    // Capture de l'écran entier d'abord
    Image fullScreenImage = LoadImageFromScreen();
    
    // Extraction de la région spécifiée
    // Correction du paramètre pour ImageFromImage qui attend x, y, width, height séparément
    // Image ImageFromImage(Image image, Rectangle rec)
    captureData.image = ImageFromImage(fullScreenImage, 
                                        (Rectangle){region.x, region.y, region.width, region.height}
                                    );
    
    // Création de la texture pour l'affichage
    captureData.texture = LoadTextureFromImage(captureData.image);
    
    // Initialisation des autres champs
    captureData.compressedData = NULL;
    captureData.compressedSize = 0;
    captureData.encryptedData = NULL;
    captureData.encryptedSize = 0;
    captureData.isCompressed = false;
    captureData.isEncrypted = false;
    
    // Libération de l'image temporaire complète
    UnloadImage(fullScreenImage);
    
    return captureData;
}

void UnloadCaptureData(CaptureData* capture) {
    if (capture == NULL) return;
    
    // Libération des ressources raylib
    if (capture->image.data != NULL) UnloadImage(capture->image);
    if (capture->texture.id > 0) UnloadTexture(capture->texture);
    
    // Libération des données compressées
    if (capture->compressedData != NULL) {
        free(capture->compressedData);
        capture->compressedData = NULL;
        capture->compressedSize = 0;
        capture->isCompressed = false;
    }
    
    // Libération des données chiffrées
    if (capture->encryptedData != NULL) {
        free(capture->encryptedData);
        capture->encryptedData = NULL;
        capture->encryptedSize = 0;
        capture->isEncrypted = false;
    }
    
    // Réinitialisation des autres champs
    capture->width = 0;
    capture->height = 0;
}

bool CompressCaptureData(CaptureData* capture, int quality) {
    if (capture == NULL || !capture->image.data) return false;
    
    // Limiter la qualité entre 0 et 100
    if (quality < 0) quality = 0;
    if (quality > 100) quality = 100;
    
    // Libérer les données compressées existantes
    if (capture->compressedData != NULL) {
        free(capture->compressedData);
        capture->compressedData = NULL;
        capture->compressedSize = 0;
    }
    
    // Raylib ne fournit pas directement de fonction de compression d'image
    // On utilise donc ExportImage pour exporter en PNG et récupérer les données
    // Note: Cette méthode n'est pas optimale et devrait être améliorée avec une vraie compression
    
    // Créer un nom de fichier temporaire
    char tempFile[256] = {0};
    sprintf(tempFile, "temp_capture_%d.png", GetRandomValue(0, 99999));
    
    // Exporter l'image en PNG
    ExportImage(capture->image, tempFile);
    
    // Charger le fichier PNG en mémoire
    FILE* file = fopen(tempFile, "rb");
    if (!file) {
        printf("[ERROR] Impossible d'ouvrir le fichier temporaire pour la compression\n");
        return false;
    }
    
    // Obtenir la taille du fichier
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Allouer de la mémoire pour les données compressées
    capture->compressedData = (unsigned char*)malloc(fileSize);
    if (capture->compressedData == NULL) {
        fclose(file);
        remove(tempFile);
        printf("[ERROR] Impossible d'allouer de la mémoire pour les données compressées\n");
        return false;
    }
    
    // Lire les données du fichier
    capture->compressedSize = (int)fread(capture->compressedData, 1, fileSize, file);
    fclose(file);
    
    // Supprimer le fichier temporaire
    remove(tempFile);
    
    if (capture->compressedSize <= 0) {
        free(capture->compressedData);
        capture->compressedData = NULL;
        capture->compressedSize = 0;
        printf("[ERROR] Échec de la compression de l'image\n");
        return false;
    }
    
    capture->isCompressed = true;
    
    printf("[INFO] Image compressée: %d octets (qualité: %d)\n", 
           capture->compressedSize, quality);
    
    return true;
}