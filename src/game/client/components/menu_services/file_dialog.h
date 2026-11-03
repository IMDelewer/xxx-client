#ifndef GAME_CLIENT_COMPONENTS_MENU_SERVICES_FILE_DIALOG_H
#define GAME_CLIENT_COMPONENTS_MENU_SERVICES_FILE_DIALOG_H

#include <string>

struct SImageFileSelection
{
	std::string m_Path;
	std::string m_Error;
};

SImageFileSelection SelectBackgroundImage();

#endif
