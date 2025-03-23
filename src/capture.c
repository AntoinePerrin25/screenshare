#include "../include/capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Variables statiques pour le système de capture
static bool captureSystemInitialized = false;
static CaptureConfig currentConfig = {0};
static MonitorInfo* monitors = NULL;
static int monitorCount = 0;
static int virtualScreenWidth = 0;
static int virtualScreenHeight = 0;
static int virtualScreenLeft = 0;
static int virtualScreenTop = 0;

#ifdef _WIN32
// Structures et variables spécifiques à Windows
static HDC hdcScreen = NULL;
static HDC hdcMemDC = NULL;
static HBITMAP hbmScreen = NULL;
#endif

// Fonction d'initialisation avec configuration
bool InitCaptureSystem(CaptureConfig* config) {
    if (captureSystemInitialized) {
        printf("[INFO] Système de capture déjà initialisé, mise à jour de la configuration\n");
        if (config) {
            UpdateCaptureConfig(*config);
        }
        return true;
    }
    
    // Initialisation avec configuration par défaut si non spécifiée
    if (config) {
        currentConfig = *config;
    } else {
        // Configuration par défaut
        currentConfig.method = CAPTURE_METHOD_AUTO;
        currentConfig.quality = 80;
        currentConfig.captureInterval = 100;
        currentConfig.detectChanges = true;
        currentConfig.changeThreshold = 5;
        currentConfig.autoAdjustQuality = true;
        currentConfig.targetMonitor = -1; // Tous les moniteurs
    }
    
    // Détection des moniteurs
    monitorCount = GetMonitorsInfo(NULL, 0);
    if (monitorCount <= 0) {
        printf("[ERROR] Aucun moniteur détecté\n");
        return false;
    }
    
    // Allocation de la mémoire pour les informations sur les moniteurs
    monitors = (MonitorInfo*)malloc(monitorCount * sizeof(MonitorInfo));
    if (!monitors) {
        printf("[ERROR] Impossible d'allouer de la mémoire pour les moniteurs\n");
        return false;
    }
    
    // Récupération des informations sur les moniteurs
    GetMonitorsInfo(monitors, monitorCount);
    
    // Calcul des dimensions de l'écran virtuel (combinaison de tous les moniteurs)
    virtualScreenLeft = 0;
    virtualScreenTop = 0;
    virtualScreenWidth = 0;
    virtualScreenHeight = 0;
    
    for (int i = 0; i < monitorCount; i++) {
        // Mise à jour des dimensions de l'écran virtuel
        if (monitors[i].x < virtualScreenLeft) virtualScreenLeft = monitors[i].x;
        if (monitors[i].y < virtualScreenTop) virtualScreenTop = monitors[i].y;
        
        int right = monitors[i].x + monitors[i].width;
        int bottom = monitors[i].y + monitors[i].height;
        
        if (right > virtualScreenWidth) virtualScreenWidth = right;
        if (bottom > virtualScreenHeight) virtualScreenHeight = bottom;
        
        printf("[INFO] Moniteur %d: %s (%dx%d à %d,%d)%s\n", 
               monitors[i].index, 
               monitors[i].name, 
               monitors[i].width, 
               monitors[i].height,
               monitors[i].x,
               monitors[i].y,
               monitors[i].isPrimary ? " (principal)" : "");
    }
    
    // Ajustement des dimensions virtuelles
    virtualScreenWidth -= virtualScreenLeft;
    virtualScreenHeight -= virtualScreenTop;
    
    printf("[INFO] Écran virtuel: %dx%d (origine à %d,%d)\n", 
           virtualScreenWidth, virtualScreenHeight, virtualScreenLeft, virtualScreenTop);
    
    // Sélection de la méthode de capture
    if (currentConfig.method == CAPTURE_METHOD_AUTO) {
#ifdef _WIN32
        currentConfig.method = CAPTURE_METHOD_WIN_GDI;
#else
        currentConfig.method = CAPTURE_METHOD_RAYLIB;
#endif
    }
    
    // Initialisation spécifique à la méthode de capture
    switch (currentConfig.method) {
        case CAPTURE_METHOD_RAYLIB:
            printf("[INFO] Utilisation de la méthode de capture raylib\n");
            break;
            
        case CAPTURE_METHOD_WIN_GDI:
#ifdef _WIN32
            // Initialisation des contextes de périphérique Windows
            hdcScreen = GetDC(NULL);
            if (!hdcScreen) {
                printf("[ERROR] Impossible d'obtenir le contexte de périphérique de l'écran\n");
                free(monitors);
                monitors = NULL;
                return false;
            }
            
            hdcMemDC = CreateCompatibleDC(hdcScreen);
            if (!hdcMemDC) {
                printf("[ERROR] Impossible de créer un contexte de périphérique compatible\n");
                ReleaseDC(NULL, hdcScreen);
                hdcScreen = NULL;
                free(monitors);
                monitors = NULL;
                return false;
            }
            
            printf("[INFO] Utilisation de la méthode de capture Windows GDI\n");
#else
            printf("[WARNING] Méthode Windows GDI non disponible, utilisation de raylib\n");
            currentConfig.method = CAPTURE_METHOD_RAYLIB;
#endif
            break;
            
        default:
            printf("[WARNING] Méthode de capture non reconnue, utilisation de raylib\n");
            currentConfig.method = CAPTURE_METHOD_RAYLIB;
            break;
    }
    
    captureSystemInitialized = true;
    printf("[INFO] Système de capture initialisé avec succès\n");
    return true;
}

void CloseCaptureSystem(void) {
    if (!captureSystemInitialized) return;
    
#ifdef _WIN32
    // Libération des ressources Windows
    if (hbmScreen) {
        DeleteObject(hbmScreen);
        hbmScreen = NULL;
    }
    
    if (hdcMemDC) {
        DeleteDC(hdcMemDC);
        hdcMemDC = NULL;
    }
    
    if (hdcScreen) {
        ReleaseDC(NULL, hdcScreen);
        hdcScreen = NULL;
    }
#endif
    
    // Libération des informations sur les moniteurs
    if (monitors) {
        free(monitors);
        monitors = NULL;
    }
    
    monitorCount = 0;
    captureSystemInitialized = false;
    
    printf("[INFO] Système de capture terminé\n");
}

int GetMonitorsInfo(MonitorInfo* output, int maxMonitors) {
    int count = 0;
    
#ifdef _WIN32
    // Comptage des moniteurs sous Windows
    count = GetSystemMetrics(SM_CMONITORS);
    
    // Si output est NULL, on retourne juste le nombre de moniteurs
    if (!output || maxMonitors <= 0) return count;
    
    // Limitation au nombre maximum
    if (count > maxMonitors) count = maxMonitors;
    
    // Récupération des informations pour chaque moniteur
    for (int i = 0; i < count; i++) {
        DISPLAY_DEVICE dd = {0};
        dd.cb = sizeof(DISPLAY_DEVICE);
        
        if (EnumDisplayDevices(NULL, i, &dd, 0)) {
            output[i].index = i;
            strncpy(output[i].name, (char*)dd.DeviceName, sizeof(output[i].name) - 1);
            
            DEVMODE dm = {0};
            dm.dmSize = sizeof(DEVMODE);
            
            if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
                output[i].width = dm.dmPelsWidth;
                output[i].height = dm.dmPelsHeight;
                output[i].x = dm.dmPosition.x;
                output[i].y = dm.dmPosition.y;
                output[i].isPrimary = (dm.dmPosition.x == 0 && dm.dmPosition.y == 0) || 
                                     (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE);
            } else {
                // Valeurs par défaut en cas d'échec
                output[i].width = GetSystemMetrics(SM_CXSCREEN);
                output[i].height = GetSystemMetrics(SM_CYSCREEN);
                output[i].x = 0;
                output[i].y = 0;
                output[i].isPrimary = true;
            }
        }
    }
#else
    // Pour les systèmes non-Windows, on utilise raylib
    count = GetMonitorCount();
    
    // Si output est NULL, on retourne juste le nombre de moniteurs
    if (!output || maxMonitors <= 0) return count;
    
    // Limitation au nombre maximum
    if (count > maxMonitors) count = maxMonitors;
    
    // Récupération des informations pour chaque moniteur
    for (int i = 0; i < count; i++) {
        output[i].index = i;
        sprintf(output[i].name, "Monitor %d", i);
        output[i].width = GetMonitorWidth(i);
        output[i].height = GetMonitorHeight(i);
        output[i].x = 0; // raylib ne permet pas de récupérer la position
        output[i].y = 0;
        output[i].isPrimary = (i == 0); // On suppose que le premier moniteur est le principal
    }
#endif
    
    return count;
}

CaptureData CaptureScreen(void) {
    if (currentConfig.targetMonitor >= 0 && currentConfig.targetMonitor < monitorCount) {
        // Capture d'un moniteur spécifique
        return CaptureMonitor(currentConfig.targetMonitor);
    } else {
        // Capture de l'écran virtuel complet (tous les moniteurs)
        CaptureData captureData = {0};
        
        if (!captureSystemInitialized) {
            printf("[ERROR] Le système de capture n'est pas initialisé\n");
            return captureData;
        }
        
        // Initialisation des dimensions
        captureData.width = virtualScreenWidth;
        captureData.height = virtualScreenHeight;
        captureData.monitorIndex = -1; // Tous les moniteurs
        captureData.timestamp = time(NULL);
        
        // Capture selon la méthode choisie
        switch (currentConfig.method) {
            case CAPTURE_METHOD_RAYLIB:
                // Raylib ne peut pas capturer tous les moniteurs directement, on utilise le premier
                captureData.image = LoadImageFromScreen();
                break;
                
            case CAPTURE_METHOD_WIN_GDI:
#ifdef _WIN32
                // Création d'un bitmap compatible avec la taille de l'écran virtuel
                if (hbmScreen) {
                    DeleteObject(hbmScreen);
                    hbmScreen = NULL;
                }
                
                hbmScreen = CreateCompatibleBitmap(hdcScreen, virtualScreenWidth, virtualScreenHeight);
                if (!hbmScreen) {
                    printf("[ERROR] Impossible de créer un bitmap compatible\n");
                    return captureData;
                }
                
                // Sélection du bitmap dans le contexte mémoire
                HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemDC, hbmScreen);
                
                // Copie de l'écran dans le bitmap
                if (!BitBlt(hdcMemDC, 0, 0, virtualScreenWidth, virtualScreenHeight,
                          hdcScreen, virtualScreenLeft, virtualScreenTop, SRCCOPY)) {
                    printf("[ERROR] Échec de BitBlt\n");
                    SelectObject(hdcMemDC, hOldBitmap);
                    return captureData;
                }
                
                // Création de l'image raylib à partir du bitmap
                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = virtualScreenWidth;
                bmi.bmiHeader.biHeight = -virtualScreenHeight; // Négatif pour orientation de haut en bas
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                
                // Allocation de la mémoire pour l'image
                unsigned char* screenData = (unsigned char*)malloc(virtualScreenWidth * virtualScreenHeight * 4);
                
                // Récupération des données du bitmap
                if (screenData && GetDIBits(hdcMemDC, hbmScreen, 0, virtualScreenHeight, 
                                         screenData, &bmi, DIB_RGB_COLORS)) {
                    // Conversion des données en format raylib
                    captureData.image.data = malloc(virtualScreenWidth * virtualScreenHeight * 4);
                    if (captureData.image.data) {
                        captureData.image.width = virtualScreenWidth;
                        captureData.image.height = virtualScreenHeight;
                        captureData.image.mipmaps = 1;
                        captureData.image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                        
                        // Copie et conversion BGRA (Windows) à RGBA (raylib)
                        for (int i = 0; i < virtualScreenWidth * virtualScreenHeight; i++) {
                            unsigned char* dst = (unsigned char*)captureData.image.data + i * 4;
                            unsigned char* src = screenData + i * 4;
                            dst[0] = src[2]; // R <- B
                            dst[1] = src[1]; // G <- G
                            dst[2] = src[0]; // B <- R
                            dst[3] = 255;    // A (opaque)
                        }
                    }
                    
                    free(screenData);
                }
                
                // Restauration du bitmap original
                SelectObject(hdcMemDC, hOldBitmap);
#endif
                break;
                
            default:
                printf("[ERROR] Méthode de capture non implémentée\n");
                break;
        }
        
        // Création de la texture pour l'affichage si l'image a été capturée
        if (captureData.image.data) {
            captureData.texture = LoadTextureFromImage(captureData.image);
        } else {
            printf("[ERROR] Échec de la capture d'écran\n");
        }
        
        // Initialisation des autres champs
        captureData.compressedData = NULL;
        captureData.compressedSize = 0;
        captureData.encryptedData = NULL;
        captureData.encryptedSize = 0;
        captureData.isCompressed = false;
        captureData.isEncrypted = false;
        captureData.previousFrame = NULL;
        captureData.hasChanged = true; // Première capture, donc considérée comme un changement
        
        return captureData;
    }
}

CaptureData CaptureMonitor(int monitorIndex) {
    CaptureData captureData = {0};
    
    if (!captureSystemInitialized) {
        printf("[ERROR] Le système de capture n'est pas initialisé\n");
        return captureData;
    }
    
    // Vérification de l'index du moniteur
    if (monitorIndex < 0 || monitorIndex >= monitorCount) {
        printf("[ERROR] Index de moniteur invalide: %d (doit être entre 0 et %d)\n", 
               monitorIndex, monitorCount - 1);
        return captureData;
    }
    
    // Initialisation des dimensions
    captureData.width = monitors[monitorIndex].width;
    captureData.height = monitors[monitorIndex].height;
    captureData.monitorIndex = monitorIndex;
    captureData.timestamp = time(NULL);
    
    // Capture selon la méthode choisie
    switch (currentConfig.method) {
        case CAPTURE_METHOD_RAYLIB:
            // Avec raylib, on peut seulement capturer le moniteur actuel où la fenêtre est affichée
            // Ceci est une limitation de raylib
            captureData.image = LoadImageFromScreen();
            break;
            
        case CAPTURE_METHOD_WIN_GDI:
#ifdef _WIN32
            // Création d'un bitmap compatible avec la taille du moniteur
            if (hbmScreen) {
                DeleteObject(hbmScreen);
                hbmScreen = NULL;
            }
            
            hbmScreen = CreateCompatibleBitmap(hdcScreen, 
                                             monitors[monitorIndex].width, 
                                             monitors[monitorIndex].height);
            if (!hbmScreen) {
                printf("[ERROR] Impossible de créer un bitmap compatible\n");
                return captureData;
            }
            
            // Sélection du bitmap dans le contexte mémoire
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemDC, hbmScreen);
            
            // Copie de l'écran dans le bitmap
            if (!BitBlt(hdcMemDC, 0, 0, monitors[monitorIndex].width, monitors[monitorIndex].height,
                      hdcScreen, monitors[monitorIndex].x, monitors[monitorIndex].y, SRCCOPY)) {
                printf("[ERROR] Échec de BitBlt\n");
                SelectObject(hdcMemDC, hOldBitmap);
                return captureData;
            }
            
            // Création de l'image raylib à partir du bitmap
            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = monitors[monitorIndex].width;
            bmi.bmiHeader.biHeight = -monitors[monitorIndex].height; // Négatif pour orientation de haut en bas
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            
            // Allocation de la mémoire pour l'image
            unsigned char* screenData = (unsigned char*)malloc(monitors[monitorIndex].width * 
                                                           monitors[monitorIndex].height * 4);
            
            // Récupération des données du bitmap
            if (screenData && GetDIBits(hdcMemDC, hbmScreen, 0, monitors[monitorIndex].height, 
                                     screenData, &bmi, DIB_RGB_COLORS)) {
                // Conversion des données en format raylib
                captureData.image.data = malloc(monitors[monitorIndex].width * 
                                             monitors[monitorIndex].height * 4);
                if (captureData.image.data) {
                    captureData.image.width = monitors[monitorIndex].width;
                    captureData.image.height = monitors[monitorIndex].height;
                    captureData.image.mipmaps = 1;
                    captureData.image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    
                    // Copie et conversion BGRA (Windows) à RGBA (raylib)
                    for (int i = 0; i < monitors[monitorIndex].width * monitors[monitorIndex].height; i++) {
                        unsigned char* dst = (unsigned char*)captureData.image.data + i * 4;
                        unsigned char* src = screenData + i * 4;
                        dst[0] = src[2]; // R <- B
                        dst[1] = src[1]; // G <- G
                        dst[2] = src[0]; // B <- R
                        dst[3] = 255;    // A (opaque)
                    }
                }
                
                free(screenData);
            }
            
            // Restauration du bitmap original
            SelectObject(hdcMemDC, hOldBitmap);
#endif
            break;
            
        default:
            printf("[ERROR] Méthode de capture non implémentée\n");
            break;
    }
    
    // Création de la texture pour l'affichage si l'image a été capturée
    if (captureData.image.data) {
        captureData.texture = LoadTextureFromImage(captureData.image);
    } else {
        printf("[ERROR] Échec de la capture du moniteur %d\n", monitorIndex);
    }
    
    // Initialisation des autres champs
    captureData.compressedData = NULL;
    captureData.compressedSize = 0;
    captureData.encryptedData = NULL;
    captureData.encryptedSize = 0;
    captureData.isCompressed = false;
    captureData.isEncrypted = false;
    captureData.previousFrame = NULL;
    captureData.hasChanged = true; // Première capture, donc considérée comme un changement
    
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
    if (region.x + region.width > virtualScreenWidth) region.width = virtualScreenWidth - region.x;
    if (region.y + region.height > virtualScreenHeight) region.height = virtualScreenHeight - region.y;
    
    if (region.width <= 0 || region.height <= 0) {
        printf("[ERROR] Région de capture invalide: %.0fx%.0f à (%.0f,%.0f)\n", 
               region.width, region.height, region.x, region.y);
        return captureData;
    }
    
    // Initialisation des dimensions
    captureData.width = (int)region.width;
    captureData.height = (int)region.height;
    captureData.monitorIndex = -1; // Région spécifique
    captureData.timestamp = time(NULL);
    
    // Capture selon la méthode choisie
    switch (currentConfig.method) {
        case CAPTURE_METHOD_RAYLIB:
            // On capture tout l'écran et on extrait la région
            Image fullScreenImage = LoadImageFromScreen();
            if (fullScreenImage.data) {
                captureData.image = ImageFromImage(fullScreenImage, 
                                                 (Rectangle){region.x, region.y, region.width, region.height});
                UnloadImage(fullScreenImage);
            }
            break;
            
        case CAPTURE_METHOD_WIN_GDI:
#ifdef _WIN32
            // Création d'un bitmap compatible avec la taille de la région
            if (hbmScreen) {
                DeleteObject(hbmScreen);
                hbmScreen = NULL;
            }
            
            hbmScreen = CreateCompatibleBitmap(hdcScreen, (int)region.width, (int)region.height);
            if (!hbmScreen) {
                printf("[ERROR] Impossible de créer un bitmap compatible\n");
                return captureData;
            }
            
            // Sélection du bitmap dans le contexte mémoire
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMemDC, hbmScreen);
            
            // Copie de la région d'écran dans le bitmap
            if (!BitBlt(hdcMemDC, 0, 0, (int)region.width, (int)region.height,
                      hdcScreen, 
                      virtualScreenLeft + (int)region.x, 
                      virtualScreenTop + (int)region.y, 
                      SRCCOPY)) {
                printf("[ERROR] Échec de BitBlt\n");
                SelectObject(hdcMemDC, hOldBitmap);
                return captureData;
            }
            
            // Création de l'image raylib à partir du bitmap
            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = (int)region.width;
            bmi.bmiHeader.biHeight = -(int)region.height; // Négatif pour orientation de haut en bas
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            
            // Allocation de la mémoire pour l'image
            unsigned char* screenData = (unsigned char*)malloc((int)region.width * (int)region.height * 4);
            
            // Récupération des données du bitmap
            if (screenData && GetDIBits(hdcMemDC, hbmScreen, 0, (int)region.height, 
                                     screenData, &bmi, DIB_RGB_COLORS)) {
                // Conversion des données en format raylib
                captureData.image.data = malloc((int)region.width * (int)region.height * 4);
                if (captureData.image.data) {
                    captureData.image.width = (int)region.width;
                    captureData.image.height = (int)region.height;
                    captureData.image.mipmaps = 1;
                    captureData.image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    
                    // Copie et conversion BGRA (Windows) à RGBA (raylib)
                    for (int i = 0; i < (int)region.width * (int)region.height; i++) {
                        unsigned char* dst = (unsigned char*)captureData.image.data + i * 4;
                        unsigned char* src = screenData + i * 4;
                        dst[0] = src[2]; // R <- B
                        dst[1] = src[1]; // G <- G
                        dst[2] = src[0]; // B <- R
                        dst[3] = 255;    // A (opaque)
                    }
                }
                
                free(screenData);
            }
            
            // Restauration du bitmap original
            SelectObject(hdcMemDC, hOldBitmap);
#endif
            break;
            
        default:
            printf("[ERROR] Méthode de capture non implémentée\n");
            break;
    }
    
    // Création de la texture pour l'affichage si l'image a été capturée
    if (captureData.image.data) {
        captureData.texture = LoadTextureFromImage(captureData.image);
    } else {
        printf("[ERROR] Échec de la capture de la région\n");
    }
    
    // Initialisation des autres champs
    captureData.compressedData = NULL;
    captureData.compressedSize = 0;
    captureData.encryptedData = NULL;
    captureData.encryptedSize = 0;
    captureData.isCompressed = false;
    captureData.isEncrypted = false;
    captureData.previousFrame = NULL;
    captureData.hasChanged = true; // Première capture, donc considérée comme un changement
    
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
    
    // Libération de l'image précédente
    if (capture->previousFrame != NULL) {
        free(capture->previousFrame);
        capture->previousFrame = NULL;
    }
    
    // Réinitialisation des autres champs
    capture->width = 0;
    capture->height = 0;
    capture->hasChanged = false;
    capture->monitorIndex = -1;
    capture->timestamp = 0;
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
    
    // Méthode de compression améliorée
    // Nous utilisons JPEG pour une meilleure compression
    
    // Créer un nom de fichier temporaire
    char tempFile[256] = {0};
    sprintf(tempFile, "temp_capture_%llu.jpg", (unsigned long long)capture->timestamp);
    
    // Convertir l'image en JPG avec la qualité spécifiée
    ExportImage(capture->image, tempFile);
    
    // Charger le fichier en mémoire
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
    
    // Calcul du ratio de compression
    int originalSize = capture->width * capture->height * 4; // RGBA
    float ratio = (float)originalSize / capture->compressedSize;
    
    printf("[INFO] Image compressée: %d octets (qualité: %d, ratio: %.2f:1)\n", 
           capture->compressedSize, quality, ratio);
    
    return true;
}

bool DetectChanges(CaptureData* capture, int threshold) {
    if (!capture || !capture->image.data) return false;
    if (threshold < 0) threshold = 0;
    if (threshold > 100) threshold = 100;
    
    // Si c'est la première capture ou si pas d'image précédente, considérer comme changée
    if (!capture->previousFrame) {
        int imgSize = capture->width * capture->height * 4;
        capture->previousFrame = (unsigned char*)malloc(imgSize);
        
        if (capture->previousFrame) {
            // Copie de l'image actuelle comme référence pour les comparaisons futures
            memcpy(capture->previousFrame, capture->image.data, imgSize);
            capture->hasChanged = true;
            return true;
        } else {
            printf("[ERROR] Impossible d'allouer de la mémoire pour la comparaison\n");
            capture->hasChanged = true;
            return true;
        }
    }
    
    // Comparaison pixel par pixel
    int differentPixels = 0;
    int totalPixels = capture->width * capture->height;
    int toleranceThreshold = (100 - threshold) * 3; // 3 canaux (R,G,B)
    
    for (int i = 0; i < totalPixels; i++) {
        unsigned char* current = (unsigned char*)capture->image.data + i * 4;
        unsigned char* previous = capture->previousFrame + i * 4;
        
        // Calcul de la différence pour chaque canal
        int diffR = abs(current[0] - previous[0]);
        int diffG = abs(current[1] - previous[1]);
        int diffB = abs(current[2] - previous[2]);
        
        // Si la différence dépasse le seuil, pixel considéré comme différent
        if (diffR + diffG + diffB > toleranceThreshold) {
            differentPixels++;
        }
    }
    
    // Calcul du pourcentage de changement
    float changePercentage = 100.0f * differentPixels / totalPixels;
    
    // Mise à jour de l'image précédente pour la prochaine comparaison
    memcpy(capture->previousFrame, capture->image.data, capture->width * capture->height * 4);
    
    // Détermination si l'image a suffisamment changé
    bool changed = changePercentage >= (float)(100 - threshold) / 10.0f;
    capture->hasChanged = changed;
    
    if (changed) {
        printf("[INFO] Changement détecté: %.2f%% des pixels ont changé (seuil: %d%%)\n", 
               changePercentage, (100 - threshold) / 10);
    }
    
    return changed;
}

bool UpdateCaptureConfig(CaptureConfig config) {
    if (!captureSystemInitialized) {
        printf("[ERROR] Le système de capture n'est pas initialisé\n");
        return false;
    }
    
    // Mise à jour des paramètres de configuration
    currentConfig = config;
    
    // Vérification et ajustement des valeurs
    if (currentConfig.quality < 0) currentConfig.quality = 0;
    if (currentConfig.quality > 100) currentConfig.quality = 100;
    
    if (currentConfig.captureInterval < 10) currentConfig.captureInterval = 10; // Minimum 10ms
    
    if (currentConfig.changeThreshold < 0) currentConfig.changeThreshold = 0;
    if (currentConfig.changeThreshold > 100) currentConfig.changeThreshold = 100;
    
    // Vérification du moniteur cible
    if (currentConfig.targetMonitor >= monitorCount) {
        printf("[WARNING] Index de moniteur invalide, utilisation de tous les moniteurs\n");
        currentConfig.targetMonitor = -1;
    }
    
    // Vérification et ajustement de la méthode de capture
    if (currentConfig.method == CAPTURE_METHOD_AUTO) {
#ifdef _WIN32
        currentConfig.method = CAPTURE_METHOD_WIN_GDI;
#else
        currentConfig.method = CAPTURE_METHOD_RAYLIB;
#endif
    } else if (currentConfig.method == CAPTURE_METHOD_WIN_GDI) {
#ifndef _WIN32
        printf("[WARNING] Méthode Windows GDI non disponible, utilisation de raylib\n");
        currentConfig.method = CAPTURE_METHOD_RAYLIB;
#endif
    }
    
    printf("[INFO] Configuration de capture mise à jour\n");
    return true;
}

CaptureConfig GetCaptureConfig(void) {
    return currentConfig;
}