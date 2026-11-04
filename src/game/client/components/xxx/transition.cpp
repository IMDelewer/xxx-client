#include "transition.h"

#include <engine/client.h>
#include <engine/font_icons.h>

#include <game/client/gameclient.h>
#include <game/client/xui/theme.h>
#include <game/localization.h>

void CTransition::OnStateChange(int NewState, int OldState)
{
	if(NewState != IClient::STATE_ONLINE || (OldState != IClient::STATE_CONNECTING && OldState != IClient::STATE_LOADING))
		return;

	CIsland::SNotification Notification;
	Notification.m_pIcon = FontIcon::NETWORK_WIRED;
	Notification.m_Title = Localize("Connected");
	Notification.m_Subtitle = Client()->ConnectAddressString();
	Notification.m_Color = XuiTheme::Color::GREEN;
	GameClient()->m_Island.Push(Notification);
}
