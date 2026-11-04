#include <base/str.h>
#include <base/time.h>

#include <engine/font_icons_phosphor.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/editor/editor.h>
#include <game/localization.h>

#include <algorithm>

using namespace Xui;

namespace
{
	void Head(CXui &Ui, const char *pIcon, ColorRGBA IconColor, const char *pTitle, const char *pText)
	{
		SBox Top;
		Top.m_Gap = 12.0f;
		Top.m_Cross = EAlign::CENTER;
		Ui.Row(Top, [&]() {
			SBox IconBox;
			IconBox.m_W = Px(40.0f);
			IconBox.m_H = Px(40.0f);
			IconBox.m_Main = EAlign::CENTER;
			IconBox.m_Cross = EAlign::CENTER;
			const int Node = Ui.Row(IconBox, [&]() { Ui.Icon(pIcon, 22.0f, IconColor, true); });
			Ui.Paint(Node, [IconColor](CXui &U, const SRect &Rc) {
				U.DrawRect(Rc, CXui::SolidRect(CXui::WithAlpha(IconColor, 0.16f), 12.0f));
			});
			STextStyle Title;
			Title.m_Size = XuiTheme::Font::TITLE;
			Title.m_Font = EFont::BOLD;
			Title.m_Wrap = true;
			SBox TitleBox;
			TitleBox.m_W = Grow();
			TitleBox.m_Shrink = true;
			Ui.Text(pTitle, Title, TitleBox);
		});
		if(pText != nullptr && pText[0] != '\0')
		{
			STextStyle Body;
			Body.m_Size = 14.0f;
			Body.m_Color = XuiTheme::Color::TEXT2;
			Body.m_Wrap = true;
			SBox BodyBox;
			BodyBox.m_W = Grow();
			Ui.Text(pText, Body, BodyBox);
		}
	}

	void Buttons(CXui &Ui, const Xui::FBuild &Content)
	{
		SBox Bar;
		Bar.m_Gap = 10.0f;
		Bar.m_W = Grow();
		Bar.m_Padding = SEdges(8.0f, 0.0f, 0.0f, 0.0f);
		Ui.Row(Bar, Content);
	}

	bool Btn(CXui &Ui, const char *pLabel, bool Accent, const char *pKey, bool Disabled = false)
	{
		SButtonStyle Style;
		Style.m_Variant = Accent ? EVariant::ACCENT : EVariant::GHOST;
		Style.m_Disabled = Disabled;
		Style.m_Height = 40.0f;
		Style.m_FontSize = 14.0f;
		Style.m_W = Grow();
		Style.m_pKey = pKey;
		return Ui.Button(pLabel, Style).m_Clicked;
	}

	void LabeledRow(CXui &Ui, const char *pLabel, const Xui::FBuild &Control)
	{
		SBox R;
		R.m_W = Grow();
		R.m_Gap = 12.0f;
		R.m_Cross = EAlign::CENTER;
		Ui.Row(R, [&]() {
			STextStyle L;
			L.m_Size = 13.5f;
			L.m_Font = EFont::MEDIUM;
			L.m_Color = XuiTheme::Color::TEXT2;
			SBox LB;
			LB.m_W = Px(84.0f);
			Ui.Text(pLabel, L, LB);
			Control();
		});
	}
}

bool CXui::HandlesPopup(int Popup)
{
	switch(Popup)
	{
	case CMenus::POPUP_JOIN_TUTORIAL:
	case CMenus::POPUP_RENAME_DEMO:
	case CMenus::POPUP_RENDER_DEMO:
	case CMenus::POPUP_RENDER_DONE:
	case CMenus::POPUP_SAVE_SKIN:
	case CMenus::POPUP_MESSAGE:
	case CMenus::POPUP_CONFIRM:
	case CMenus::POPUP_DISCONNECTED:
	case CMenus::POPUP_PASSWORD:
	case CMenus::POPUP_QUIT:
	case CMenus::POPUP_RESTART:
	case CMenus::POPUP_FIRST_LAUNCH:
	case CMenus::POPUP_LANGUAGE:
	case CMenus::POPUP_OPTIONAL_FONTS:
	case CMenus::POPUP_POINTS:
	case CMenus::POPUP_WARNING:
		return true;
	default:
		return false;
	}
}

void CXui::RenderShellPopup(CMenus &M)
{
	COptionalFonts &Fonts = GameClient()->m_OptionalFonts;
	if(M.m_Popup == CMenus::POPUP_NONE && Fonts.Offer())
		M.m_Popup = CMenus::POPUP_OPTIONAL_FONTS;
	static bool s_Open = true;
	const int Popup = M.m_Popup;
	static int s_PreviousPopup = CMenus::POPUP_NONE;
	const bool Opened = Popup != s_PreviousPopup;
	s_PreviousPopup = Popup;
	if(!HandlesPopup(Popup))
	{
		s_Open = true;
		return;
	}
	const bool Cancel = !s_Open;
	s_Open = true;
	const bool Enter = KeyPressed(KEY_RETURN) || KeyPressed(KEY_KP_ENTER);
	char aBuf[1536];

	auto Choose = [&M](int Button) {
		M.m_Popup = M.m_aPopupButtons[Button].m_NextPopup;
		(M.*M.m_aPopupButtons[Button].m_pfnCallback)();
	};

	if(Cancel)
	{
		switch(Popup)
		{
		case CMenus::POPUP_MESSAGE: Choose(CMenus::BUTTON_CONFIRM); break;
		case CMenus::POPUP_CONFIRM: Choose(CMenus::BUTTON_CANCEL); break;
		case CMenus::POPUP_LANGUAGE: M.m_Popup = CMenus::POPUP_FIRST_LAUNCH; break;
		case CMenus::POPUP_OPTIONAL_FONTS:
			Fonts.Dismiss();
			M.m_Popup = CMenus::POPUP_NONE;
			break;
		case CMenus::POPUP_FIRST_LAUNCH:
			Client()->RequestDDNetInfo();
			M.m_JoinTutorial.m_Queued = false;
			M.m_Popup = g_Config.m_BrIndicateFinished ? CMenus::POPUP_POINTS : CMenus::POPUP_NONE;
			break;
		case CMenus::POPUP_POINTS:
			M.m_Popup = Client()->InfoState() == IClient::EInfoState::SUCCESS && Client()->Points() > 50 ? CMenus::POPUP_FIRST_LAUNCH : CMenus::POPUP_NONE;
			break;
		case CMenus::POPUP_DISCONNECTED:
			if(Client()->ReconnectTime() > 0)
				Client()->SetReconnectTime(0);
			M.m_Popup = CMenus::POPUP_NONE;
			break;
		default: M.m_Popup = CMenus::POPUP_NONE; break;
		}
		return;
	}

	Modal("shell.popup", &s_Open, [&]() {
		switch(Popup)
		{
		case CMenus::POPUP_MESSAGE:
			Head(*this, Ph::INFO, Accent(), M.m_aPopupTitle, M.m_aPopupMessage);
			Buttons(*this, [&]() {
				if(Btn(*this, M.m_aPopupButtons[CMenus::BUTTON_CONFIRM].m_aLabel, true, "ok") || Enter)
					Choose(CMenus::BUTTON_CONFIRM);
			});
			break;

		case CMenus::POPUP_CONFIRM:
			Head(*this, Ph::WARNING_CIRCLE, XuiTheme::Color::YELLOW, M.m_aPopupTitle, M.m_aPopupMessage);
			Buttons(*this, [&]() {
				if(Btn(*this, M.m_aPopupButtons[CMenus::BUTTON_CANCEL].m_aLabel, false, "cancel"))
					Choose(CMenus::BUTTON_CANCEL);
				else if(Btn(*this, M.m_aPopupButtons[CMenus::BUTTON_CONFIRM].m_aLabel, true, "confirm") || Enter)
					Choose(CMenus::BUTTON_CONFIRM);
			});
			break;

		case CMenus::POPUP_DISCONNECTED:
		{
			const bool Reconnect = Client()->ReconnectTime() > 0;
			if(Reconnect)
				str_format(aBuf, sizeof(aBuf), Localize("Reconnect in %d sec"), (int)((Client()->ReconnectTime() - time_get()) / time_freq()) + 1);
			Head(*this, Ph::WIFI_SLASH, XuiTheme::Color::RED_SOFT, Localize("Disconnected"), Reconnect ? aBuf : Client()->ErrorString());
			if(Reconnect)
			{
				STextStyle Err;
				Err.m_Size = 13.0f;
				Err.m_Color = XuiTheme::Color::TEXT3;
				Err.m_Wrap = true;
				SBox EB;
				EB.m_W = Grow();
				Text(Client()->ErrorString(), Err, EB);
			}
			Buttons(*this, [&]() {
				if(Btn(*this, Reconnect ? Localize("Abort") : Localize("Ok"), !Reconnect, "ok") || Enter)
				{
					if(Reconnect)
						Client()->SetReconnectTime(0);
					M.m_Popup = CMenus::POPUP_NONE;
				}
			});
			break;
		}

		case CMenus::POPUP_PASSWORD:
		{
			char aAddr[NETADDR_MAXSTRSIZE];
			net_addr_str(&Client()->ServerAddress(), aAddr, sizeof(aAddr), true);
			Head(*this, Ph::LOCK_KEY, Accent(), Localize("Password incorrect"), nullptr);
			const CServerBrowser::CServerEntry *pEntry = ServerBrowser()->Find(Client()->ServerAddress());
			if(pEntry != nullptr && pEntry->m_GotInfo)
			{
				LabeledRow(*this, Localize("Name"), [&]() {
					STextStyle V;
					V.m_Size = 13.5f;
					Text(pEntry->m_Info.m_aName, V);
				});
			}
			LabeledRow(*this, Localize("Address"), [&]() {
				STextStyle V;
				V.m_Size = 13.5f;
				V.m_Tabular = true;
				Text(aAddr, V);
			});
			LabeledRow(*this, Localize("Password"), [&]() {
				const SResult R = Field("password", g_Config.m_Password, sizeof(g_Config.m_Password), {.m_pPlaceholder = Localize("Password"), .m_pIcon = Ph::LOCK, .m_W = Grow()});
				if(R.m_Submitted)
					Client()->Connect(aAddr, g_Config.m_Password);
			});
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Abort"), false, "abort"))
					M.m_Popup = CMenus::POPUP_NONE;
				else if(Btn(*this, Localize("Try again"), true, "retry") || Enter)
					Client()->Connect(aAddr, g_Config.m_Password);
			});
			break;
		}

		case CMenus::POPUP_QUIT:
		case CMenus::POPUP_RESTART:
		{
			const bool Restart = Popup == CMenus::POPUP_RESTART;
			const char *pExtra = nullptr;
			if(GameClient()->Editor()->HasUnsavedData())
				pExtra = Localize("There's an unsaved map in the editor, you might want to save it.");
			else if(GameClient()->m_TouchControls.HasEditingChanges())
				pExtra = Localize("There's an unsaved change in the touch controls editor, you might want to save it.");
			if(pExtra != nullptr)
				str_format(aBuf, sizeof(aBuf), "%s %s", pExtra, Localize("Continue anyway?"));
			else
				str_copy(aBuf, Restart ? Localize("Are you sure that you want to restart?") : Localize("Are you sure that you want to quit?"));
			Head(*this, Restart ? Ph::ARROW_CLOCKWISE : Ph::POWER, XuiTheme::Color::RED_SOFT, Restart ? Localize("Restart") : Localize("Quit"), aBuf);
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("No"), false, "no"))
					M.m_Popup = CMenus::POPUP_NONE;
				else if(Btn(*this, Restart ? Localize("Restart") : Localize("Quit"), true, "yes") || Enter)
				{
					M.m_Popup = CMenus::POPUP_NONE;
					if(Restart)
						Client()->Restart();
					else
						Client()->Quit();
				}
			});
			break;
		}

		case CMenus::POPUP_LANGUAGE:
		{
			Head(*this, Ph::TRANSLATE, Accent(), Localize("Language"), nullptr);
			SBox ListBox;
			ListBox.m_W = Grow();
			ListBox.m_H = Px(340.0f);
			ListBox.m_Gap = 2.0f;
			Scroll(ListBox, [&]() {
				const auto &vLanguages = g_Localization.Languages();
				for(size_t i = 0; i < vLanguages.size(); i++)
				{
					const bool Active = str_comp(vLanguages[i].m_Filename.c_str(), g_Config.m_ClLanguagefile) == 0;
					PushId((uint32_t)i + 1);
					if(NavItem({Fonts.Installed() ? vLanguages[i].m_Name.c_str() : COptionalFonts::LanguageName(vLanguages[i].m_Filename.c_str(), vLanguages[i].m_Name.c_str()), Active ? Ph::CHECK : nullptr}, Active).m_Clicked && !Active)
					{
						Fonts.SelectLanguage(vLanguages[i].m_Filename.c_str());
					}
					PopId();
				}
			});
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Continue"), true, "ok") || Enter)
					M.m_Popup = CMenus::POPUP_FIRST_LAUNCH;
			});
			break;
		}

		case CMenus::POPUP_OPTIONAL_FONTS:
			Head(*this, Ph::TRANSLATE, Accent(), Localize("Additional languages"), Localize("Download fonts for Japanese, Korean and Chinese? The optional download is 29 MB. You can install it later in Settings."));
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Later"), false, "fonts.later"))
				{
					Fonts.Dismiss();
					M.m_Popup = CMenus::POPUP_NONE;
				}
				if(Btn(*this, Localize("Download"), true, "fonts.download") || Enter)
				{
					Fonts.Start();
					M.m_Popup = CMenus::POPUP_NONE;
				}
			});
			break;

		case CMenus::POPUP_FIRST_LAUNCH:
		{
			Head(*this, Ph::HAND_WAVING, Accent(), Localize("Welcome to DDNet"),
				Localize("DDraceNetwork is a cooperative online game where the goal is for you and your group of tees to reach the finish line of the map. As a newcomer you should start on Novice servers, which host the easiest maps. Consider the ping to choose a server close to you."));
			STextStyle Hint;
			Hint.m_Size = 13.0f;
			Hint.m_Color = XuiTheme::Color::TEXT3;
			Hint.m_Wrap = true;
			SBox HB;
			HB.m_W = Grow();
			Text(Localize("Use k key to kill (restart), q to pause and watch other players. See settings for other key binds."), Hint, HB);
			LabeledRow(*this, Localize("Nickname"), [&]() {
				Field("nick", g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName), {.m_pPlaceholder = Client()->PlayerName(), .m_pIcon = Ph::USER, .m_W = Grow()});
			});
			SBox Opt;
			Opt.m_W = Grow();
			Opt.m_Gap = 12.0f;
			Opt.m_Cross = EAlign::CENTER;
			Row(Opt, [&]() {
				SBox Col;
				Col.m_W = Grow();
				Col.m_Shrink = true;
				Col.m_Gap = 2.0f;
				Column(Col, [&]() {
					STextStyle T;
					T.m_Size = 13.5f;
					T.m_Font = EFont::MEDIUM;
					T.m_Wrap = true;
					SBox TB;
					TB.m_W = Grow();
					Text(Localize("Show DDNet map finishes in server browser"), T, TB);
					Text(Localize("transmits your player name to info.ddnet.org"), Hint, TB);
				});
				Switch("finishes", &g_Config.m_BrIndicateFinished);
			});
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Skip Tutorial"), false, "skip"))
				{
					Client()->RequestDDNetInfo();
					M.m_JoinTutorial.m_Queued = false;
					M.m_Popup = g_Config.m_BrIndicateFinished ? CMenus::POPUP_POINTS : CMenus::POPUP_NONE;
				}
				else if(Btn(*this, Localize("Join Tutorial Server"), true, "tutorial") || Enter)
				{
					Client()->RequestDDNetInfo();
					M.m_Popup = g_Config.m_BrIndicateFinished ? CMenus::POPUP_POINTS : CMenus::POPUP_NONE;
					M.JoinTutorial();
				}
			});
			break;
		}

		case CMenus::POPUP_POINTS:
		{
			const bool Known = Client()->InfoState() == IClient::EInfoState::SUCCESS && Client()->Points() > 50;
			if(Known)
				str_format(aBuf, sizeof(aBuf), Localize("Your nickname '%s' is already used (%d points). Do you still want to use it?"), Client()->PlayerName(), Client()->Points());
			Head(*this, Ph::USER_CIRCLE, Accent(), Localize("Existing Player"), Known ? aBuf : Localize("Checking for existing player with your name"));
			if(!Known)
				Spinner();
			Buttons(*this, [&]() {
				if(Known)
				{
					if(Btn(*this, Localize("No"), false, "no"))
						M.m_Popup = CMenus::POPUP_FIRST_LAUNCH;
					else if(Btn(*this, Localize("Yes"), true, "yes") || Enter)
						M.m_Popup = CMenus::POPUP_NONE;
				}
				else if(Btn(*this, Localize("Cancel"), false, "cancel") || Client()->InfoState() == IClient::EInfoState::SUCCESS)
					M.m_Popup = CMenus::POPUP_NONE;
			});
			if(!Known && Client()->InfoState() == IClient::EInfoState::ERROR)
				M.PopupMessage(Localize("Error checking player name"), Localize("Could not check for existing player with your name. Check your internet connection."), Localize("Ok"));
			break;
		}

		case CMenus::POPUP_RENAME_DEMO:
		{
			Head(*this, Ph::PENCIL_SIMPLE, Accent(), Localize("Rename demo"), "");
			static char s_aName[128];
			if(Opened)
				str_copy(s_aName, M.m_DemoRenameInput.GetString());
			if(Field("demo.rename", s_aName, sizeof(s_aName), {.m_W = Grow()}).m_Changed)
				M.m_DemoRenameInput.Set(s_aName);
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Cancel"), false, "cancel"))
					M.m_Popup = CMenus::POPUP_NONE;
				if(Btn(*this, Localize("Rename"), true, "rename", M.m_DemoRenameInput.IsEmpty()) || Enter)
					M.RenameDemo();
			});
			break;
		}
#if defined(CONF_VIDEORECORDER)
		case CMenus::POPUP_RENDER_DEMO:
		{
			Head(*this, Ph::FILM_SLATE, Accent(), Localize("Render demo"), "");
			static char s_aName[128];
			if(Opened)
				str_copy(s_aName, M.m_DemoRenderInput.GetString());
			if(Field("demo.render.name", s_aName, sizeof(s_aName), {.m_W = Grow()}).m_Changed)
				M.m_DemoRenderInput.Set(s_aName);
			SettingsRow(Localize("Show chat"), "", [&]() { Switch("video.chat", &g_Config.m_ClVideoShowChat); });
			SettingsRow(Localize("Use sounds"), "", [&]() { Switch("video.sound", &g_Config.m_ClVideoSndEnable); });
			SettingsRow(Localize("Show ingame HUD"), "", [&]() { Switch("video.hud", &g_Config.m_ClVideoShowhud); });
			int Paused = M.m_StartPaused;
			SettingsRow(Localize("Start paused"), "", [&]() {
				if(Switch("video.paused", &Paused).m_Changed)
					M.m_StartPaused = Paused != 0;
			});
			LabeledRow(*this, Localize("Speed"), [&]() {
				if(Button("-", {.m_pKey = "render.slower"}).m_Clicked)
					M.m_Speed = std::max(0, M.m_Speed - 1);
				char aSpeed[32];
				str_format(aSpeed, sizeof(aSpeed), "×%g", DEMO_SPEEDS[M.m_Speed]);
				Text(aSpeed);
				if(Button("+", {.m_pKey = "render.faster"}).m_Clicked)
					M.m_Speed = std::min((int)std::size(DEMO_SPEEDS) - 1, M.m_Speed + 1);
			});
			if(Client()->State() == IClient::STATE_ONLINE)
				Text(Localize("You will be disconnected from the server."));
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Cancel"), false, "cancel"))
					M.m_Popup = CMenus::POPUP_NONE;
				if(Btn(*this, Localize("Render demo"), true, "render", M.m_DemoRenderInput.IsEmpty()) || Enter)
					M.RequestDemoRender();
			});
			break;
		}
		case CMenus::POPUP_RENDER_DONE:
			Head(*this, Ph::FILM_SLATE, Accent(), Localize("Video rendered"), M.m_DemoRenderInput.GetString());
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Videos directory"), false, "folder"))
				{
					char aPath[IO_MAX_PATH_LENGTH];
					Storage()->GetCompletePath(IStorage::TYPE_SAVE, "videos", aPath, sizeof(aPath));
					Client()->ViewFile(aPath);
				}
				if(Btn(*this, Localize("Ok"), true, "ok") || Enter)
					M.m_Popup = CMenus::POPUP_NONE;
			});
			break;
#endif
		case CMenus::POPUP_SAVE_SKIN:
		{
			Head(*this, Ph::PALETTE, Accent(), Localize("Save skin"), "");
			static char s_aName[128];
			if(Opened)
				str_copy(s_aName, M.m_SkinNameInput.GetString());
			if(Field("skin.save.name", s_aName, sizeof(s_aName), {.m_W = Grow()}).m_Changed)
				M.m_SkinNameInput.Set(s_aName);
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Cancel"), false, "cancel"))
					M.m_Popup = CMenus::POPUP_NONE;
				if(Btn(*this, Localize("Save"), true, "save", M.m_SkinNameInput.IsEmpty()) || Enter)
				{
					if(!str_valid_filename(M.m_SkinNameInput.GetString()) || CSkins7::IsSpecialSkin(M.m_SkinNameInput.GetString()))
						M.PopupMessage(Localize("Error"), Localize("This name cannot be used for files and folders"), Localize("Ok"), CMenus::POPUP_SAVE_SKIN);
					else if(!GameClient()->m_Skins7.SaveSkinfile(M.m_SkinNameInput.GetString(), M.m_Dummy))
						M.PopupMessage(Localize("Error"), Localize("Unable to save the skin"), Localize("Ok"), CMenus::POPUP_SAVE_SKIN);
					else
						M.m_Popup = CMenus::POPUP_NONE;
				}
			});
			break;
		}
		case CMenus::POPUP_JOIN_TUTORIAL:
			Head(*this, Ph::INFO, Accent(), Localize("Join tutorial"), Localize("Looking for a tutorial server"));
			if(const char *pAddress = ServerBrowser()->GetTutorialServer())
				Client()->Connect(pAddress);
			Buttons(*this, [&]() {
				if(Btn(*this, Localize("Cancel"), false, "cancel"))
					M.m_Popup = CMenus::POPUP_NONE;
				if(Btn(*this, Localize("Refresh"), true, "refresh"))
				{
					Client()->RequestDDNetInfo();
					M.RefreshBrowserTab(true);
				}
			});
			break;

		case CMenus::POPUP_WARNING:
			Head(*this, Ph::WARNING, XuiTheme::Color::ORANGE, M.m_aMessageTopic, M.m_aMessageBody);
			Buttons(*this, [&]() {
				if(Btn(*this, M.m_aMessageButton, true, "ok") || Enter || (M.m_PopupWarningDuration > std::chrono::nanoseconds(0) && time_get_nanoseconds() - M.m_PopupWarningLastTime >= M.m_PopupWarningDuration))
				{
					M.m_Popup = CMenus::POPUP_NONE;
					M.SetActive(false);
				}
			});
			break;
		}
	});
}
