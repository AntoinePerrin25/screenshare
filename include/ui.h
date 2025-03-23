#ifndef UI_H
#define UI_H

#include <raylib.h>
#include <stdbool.h>

/**
 * @brief Définition des différentes pages de l'interface
 */
typedef enum {
    UI_PAGE_MAIN,       // Page principale
    UI_PAGE_SETTINGS,   // Page des paramètres
    UI_PAGE_CONNECT,    // Page de connexion
    UI_PAGE_ABOUT       // Page à propos
} UIPage;

/**
 * @brief Initialise l'interface utilisateur
 * @return true si l'initialisation réussit, false sinon
 */
bool InitUI(void);

/**
 * @brief Initialise l'icône de la zone de notification
 * @return true si l'initialisation réussit, false sinon
 */
bool InitSystemTray(void);

/**
 * @brief Met à jour l'interface utilisateur
 */
void UpdateUI(void);

/**
 * @brief Dessine l'interface utilisateur
 */
void DrawUI(void);

/**
 * @brief Gère les événements de la zone de notification
 * @return true si un événement a été traité, false sinon
 */
bool HandleSystemTrayEvents(void);

/**
 * @brief Réduit l'application dans la zone de notification
 */
void MinimizeToSystemTray(void);

/**
 * @brief Restaure l'application depuis la zone de notification
 */
void RestoreFromSystemTray(void);

/**
 * @brief Libère les ressources associées à l'interface utilisateur
 */
void CloseUI(void);

/**
 * @brief Change la page courante de l'interface
 * @param page Nouvelle page à afficher
 */
void SetUIPage(UIPage page);

/**
 * @brief Vérifie si l'application est actuellement minimisée dans la zone de notification
 * @return true si l'application est minimisée, false sinon
 */
bool IsMinimizedToSystemTray(void);

/**
 * @brief Affiche une boîte de dialogue de saisie de texte
 * @param bounds Rectangle définissant la position de la boîte                                      
 */
const char* TextInputBox(Rectangle bounds, const char* title, const char* message, const char* defaultText);

#endif // UI_H