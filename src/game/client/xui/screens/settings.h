#ifndef GAME_CLIENT_XUI_SCREENS_SETTINGS_H
#define GAME_CLIENT_XUI_SCREENS_SETTINGS_H

class CXui;

void RenderSettings(CXui &Ui);
void SettingsSelectNav(int Index);
void SettingsSelectSection(const char *pId);
void SettingsShowCatalog();

#endif
