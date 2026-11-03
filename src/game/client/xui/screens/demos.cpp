#include <base/fs.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/font_icons_phosphor.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/xui/xui.h>
#include <game/localization.h>

using namespace Xui;

namespace
{
constexpr int MAX_CARDS = 240;
constexpr int COLUMNS = 4;

ColorRGBA CardColor(const char *pName)
{
	static const ColorRGBA s_aColors[] = {
		XuiTheme::Rgb(0x2B4C7E), XuiTheme::Rgb(0x7E2B45), XuiTheme::Rgb(0x6B5A1A), XuiTheme::Rgb(0x1F6B45), XuiTheme::Rgb(0x4B2B7E), XuiTheme::Rgb(0x7E4B2B)};
	unsigned Hash = 5381;
	for(const char *p = pName; *p; p++)
		Hash = Hash * 33 + (unsigned char)*p;
	return s_aColors[Hash % std::size(s_aColors)];
}

void FormatSize(int64_t Size, char *pBuf, int BufSize)
{
	if(Size >= 1024 * 1024)
		str_format(pBuf, BufSize, "%.1f MB", Size / (1024.0 * 1024.0));
	else
		str_format(pBuf, BufSize, "%d KB", (int)(Size / 1024));
}
}

void CXui::RenderDemos(CMenus &M)
{
	if(!M.m_DemoBrowserListInitialized)
	{
		M.DemolistPopulate();
		M.DemolistOnUpdate(true);
		M.m_DemoBrowserListInitialized = true;
	}

	static char s_aSearch[64] = "";
	static bool s_CtxOpen = false;
	static int s_CtxIndex = -1;
	static vec2 s_CtxAt;
	if(str_comp(s_aSearch, M.m_DemoSearchInput.GetString()) != 0 && FocusId() == 0)
		str_copy(s_aSearch, M.m_DemoSearchInput.GetString());

	auto Activate = [&](int Index) {
		if(Index < 0 || Index >= (int)M.m_vpFilteredDemos.size())
			return;
		M.m_DemolistSelectedIndex = Index;
		const CMenus::CDemoItem *pItem = M.m_vpFilteredDemos[Index];
		if(pItem->m_IsDir)
		{
			M.m_DemoSearchInput.Clear();
			s_aSearch[0] = '\0';
			const bool Parent = str_comp(pItem->m_aFilename, "..") == 0;
			if(Parent)
			{
				str_copy(M.m_aCurrentDemoSelectionName, fs_filename(M.m_aCurrentDemoFolder));
				str_append(M.m_aCurrentDemoSelectionName, "/");
				if(fs_parent_dir(M.m_aCurrentDemoFolder))
				{
					M.m_aCurrentDemoFolder[0] = '\0';
					if(M.m_DemolistStorageType == IStorage::TYPE_ALL)
						M.m_aCurrentDemoSelectionName[0] = '\0';
					else
					{
						Storage()->GetCompletePath(M.m_DemolistStorageType, "demos", M.m_aCurrentDemoSelectionName, sizeof(M.m_aCurrentDemoSelectionName));
						str_append(M.m_aCurrentDemoSelectionName, "/");
					}
				}
			}
			else
			{
				if(M.m_aCurrentDemoFolder[0] != '\0')
					str_append(M.m_aCurrentDemoFolder, "/");
				else
					M.m_DemolistStorageType = pItem->m_StorageType;
				str_append(M.m_aCurrentDemoFolder, pItem->m_aFilename);
			}
			M.DemolistPopulate();
			M.DemolistOnUpdate(!Parent);
		}
		else if(GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmDisconnectTime && g_Config.m_ClConfirmDisconnectTime >= 0)
			M.PopupConfirm(Localize("Disconnect"), Localize("Are you sure that you want to disconnect and play this demo?"), Localize("Yes"), Localize("No"), &CMenus::PopupConfirmPlayDemo);
		else
			M.PopupConfirmPlayDemo();
	};

	auto Delete = [&](int Index) {
		M.m_DemolistSelectedIndex = Index;
		const CMenus::CDemoItem *pItem = M.m_vpFilteredDemos[Index];
		char aBuf[128 + IO_MAX_PATH_LENGTH];
		str_format(aBuf, sizeof(aBuf), pItem->m_IsDir ? Localize("Are you sure that you want to delete the folder '%s'?") : Localize("Are you sure that you want to delete the demo '%s'?"), pItem->m_aFilename);
		M.PopupConfirm(pItem->m_IsDir ? Localize("Delete folder") : Localize("Delete demo"), aBuf, Localize("Yes"), Localize("No"), pItem->m_IsDir ? &CMenus::PopupConfirmDeleteFolder : &CMenus::PopupConfirmDeleteDemo);
	};

	const int Num = (int)M.m_vpFilteredDemos.size();
	int Activated = -1;

	SBox PageBox;
	PageBox.m_W = Grow();
	PageBox.m_H = Grow();
	PageBox.m_Gap = 16.0f;
	Column(PageBox, [&]() {
		SBox Head;
		Head.m_W = Grow();
		Head.m_Gap = 14.0f;
		Head.m_Cross = EAlign::CENTER;
		Head.m_Padding = SEdges(0.0f, 6.0f);
		Row(Head, [&]() {
			if(M.m_aCurrentDemoFolder[0] != '\0' && str_comp(M.m_aCurrentDemoFolder, "demos") != 0)
			{
				if(IconButton(Ph::ARROW_LEFT, {.m_Variant = EVariant::GLASS, .m_pKey = "dm.up"}).m_Clicked)
				{
					for(int i = 0; i < Num; i++)
						if(str_comp(M.m_vpFilteredDemos[i]->m_aFilename, "..") == 0)
							Activated = i;
				}
			}
			STextStyle Title;
			Title.m_Size = 34.0f;
			Title.m_Font = EFont::EXTRABOLD;
			Title.m_Tracking = -0.035f;
			Text(Localize("Clips"), Title);
			STextStyle Sub;
			Sub.m_Size = 14.0f;
			Sub.m_Color = XuiTheme::Color::TEXT2;
			Sub.m_Wrap = true;
			Sub.m_MaxLines = 1;
			SBox SubBox;
			SubBox.m_W = Grow();
			SubBox.m_Shrink = true;
			Text(M.m_aCurrentDemoFolder[0] != '\0' ? M.m_aCurrentDemoFolder : Localize("Demos, replays and auto recordings"), Sub, SubBox);
			if(SearchField("dm.search", s_aSearch, sizeof(s_aSearch), Localize("Search"), Px(260.0f)).m_Changed)
			{
				M.m_DemoSearchInput.Set(s_aSearch);
				M.RefreshFilteredDemos();
				M.DemolistOnUpdate(false);
			}
			if(IconButton(Ph::ARROW_CLOCKWISE, {.m_Variant = EVariant::GLASS, .m_pKey = "dm.refresh"}).m_Clicked)
			{
				M.DemolistPopulate();
				M.DemolistOnUpdate(false);
			}
			if(IconButton(Ph::FOLDER, {.m_Variant = EVariant::GLASS, .m_pKey = "dm.open"}).m_Clicked)
			{
				char aPath[IO_MAX_PATH_LENGTH];
				Storage()->GetCompletePath(M.m_DemolistSelectedIndex >= 0 ? M.m_vpFilteredDemos[M.m_DemolistSelectedIndex]->m_StorageType : IStorage::TYPE_SAVE, M.m_aCurrentDemoFolder[0] != '\0' ? M.m_aCurrentDemoFolder : "demos", aPath, sizeof(aPath));
				Client()->ViewFile(aPath);
			}
		});

		SBox ScrollBox;
		ScrollBox.m_W = Grow();
		ScrollBox.m_H = Grow();
		ScrollBox.m_Gap = 16.0f;
		ScrollBox.m_Padding = SEdges(0.0f, 0.0f, 8.0f, 0.0f);
		Scroll(ScrollBox, [&]() {
			if(Num == 0)
			{
				SBox Empty;
				Empty.m_W = Grow();
				Empty.m_Padding = SEdges(80.0f, 0.0f);
				Empty.m_Main = EAlign::CENTER;
				Empty.m_Cross = EAlign::CENTER;
				Empty.m_Gap = 10.0f;
				Column(Empty, [&]() {
					Icon(Ph::FILM_SLATE, 40.0f, XuiTheme::Color::TEXT3);
					STextStyle S;
					S.m_Size = 14.0f;
					S.m_Color = XuiTheme::Color::TEXT3;
					Text(s_aSearch[0] != '\0' ? Localize("No demos match your search") : Localize("No demos yet. Record one with the demo button in game."), S);
				});
				return;
			}
			const int Shown = std::min(Num, MAX_CARDS);
			for(int RowStart = 0; RowStart < Shown; RowStart += COLUMNS)
			{
				SBox RowBox;
				RowBox.m_W = Grow();
				RowBox.m_Gap = 16.0f;
				Row(RowBox, [&]() {
					for(int c = 0; c < COLUMNS; c++)
					{
						const int i = RowStart + c;
						if(i >= Shown)
						{
							SBox Pad;
							Pad.m_W = Grow();
							Row(Pad);
							continue;
						}
						CMenus::CDemoItem *pItem = M.m_vpFilteredDemos[i];
						const bool Parent = str_comp(pItem->m_aFilename, "..") == 0;
						const bool Selected = i == M.m_DemolistSelectedIndex;
						char aKey[16];
						str_format(aKey, sizeof(aKey), "dm%d", i);
						const uint32_t Id = MakeId(aKey);
						const SResult R = Interact(Id);
						SState &St = State(Id);
						const float Hover = St.m_Hover.Update(R.m_Hovered ? 1.0f : 0.0f, m_Dt, XuiTheme::Motion::HOVER);
						const float Scale = St.m_Scale.Update(R.m_Pressed ? XuiTheme::Motion::PRESS_SCALE : (R.m_Hovered ? 1.015f : 1.0f), m_Dt, XuiTheme::Motion::PRESS);
						if(R.m_Clicked)
						{
							if(St.m_Flag && m_Time - St.m_aScratch[0] < 0.4f)
								Activated = i;
							St.m_Flag = true;
							St.m_aScratch[0] = m_Time;
							M.m_DemolistSelectedIndex = i;
							if(!pItem->m_IsDir && !pItem->m_InfosLoaded)
								M.FetchHeader(*pItem);
						}
						if(R.m_RightClicked && !Parent)
						{
							M.m_DemolistSelectedIndex = i;
							s_CtxIndex = i;
							s_CtxOpen = true;
							s_CtxAt = Mouse();
						}

						SBox Card;
						Card.m_W = Grow();
						Card.m_Shrink = true;
						Card.m_Padding = SEdges(8.0f);
						Card.m_Gap = 10.0f;
						Card.m_Hit = EHit::INTERACTIVE;
						const int CardNode = BoxId(Id, EDir::COLUMN, Card, [&]() {
							SBox Thumb;
							Thumb.m_W = Grow();
							Thumb.m_H = Px(132.0f);
							Thumb.m_Main = EAlign::CENTER;
							Thumb.m_Cross = EAlign::CENTER;
							const int ThumbNode = Row(Thumb, [&]() {
								SBox Circle;
								Circle.m_W = Px(52.0f);
								Circle.m_H = Px(52.0f);
								Circle.m_Main = EAlign::CENTER;
								Circle.m_Cross = EAlign::CENTER;
								const int CircleNode = Row(Circle, [&]() {
									Icon(pItem->m_IsDir ? (Parent ? Ph::ARROW_LEFT : Ph::FOLDER) : Ph::PLAY, 22.0f, XuiTheme::Color::TEXT, true);
								});
								Paint(CircleNode, [](CXui &U, const SRect &Rc) {
									U.DrawRect(Rc, GlassRect(Rc.m_H * 0.5f));
									U.DrawRect(Rc, GlassHairline(Rc.m_H * 0.5f));
								});
								SBox Tag;
								Tag.m_Position = EPos::ABSOLUTE;
								Tag.m_Left = 8.0f;
								Tag.m_Top = 8.0f;
								Tag.m_H = Px(24.0f);
								Tag.m_Padding = SEdges(0.0f, 10.0f);
								Tag.m_Cross = EAlign::CENTER;
								const char *pTag = pItem->m_IsDir ? Localize("Folder") : (str_find_nocase(M.m_aCurrentDemoFolder, "replays") || str_find_nocase(pItem->m_aFilename, "replay") ? Localize("Replay") : (str_find_nocase(M.m_aCurrentDemoFolder, "auto") ? Localize("Auto") : Localize("Demo")));
								const int TagNode = Row(Tag, [&]() {
									STextStyle T;
									T.m_Size = 11.5f;
									T.m_Font = EFont::BOLD;
									Text(pTag, T);
								});
								Paint(TagNode, [](CXui &U, const SRect &Rc) {
									U.DrawRect(Rc, GlassRect(Rc.m_H * 0.5f));
								});
							});
							const ColorRGBA Color = CardColor(pItem->m_aName);
							Paint(ThumbNode, [Color](CXui &U, const SRect &Rc) {
								IGraphics::SUiRect Bg = SolidRect(Mix(Color, ColorRGBA(1, 1, 1, 1), 0.08f), 20.0f);
								Bg.m_Fill2 = Mix(Color, ColorRGBA(0, 0, 0, 1), 0.45f);
								Bg.m_GradientAngle = 135.0f;
								U.DrawRect(Rc, Bg);
							});

							SBox Info;
							Info.m_W = Grow();
							Info.m_Gap = 2.0f;
							Info.m_Padding = SEdges(0.0f, 10.0f, 8.0f, 10.0f);
							Column(Info, [&]() {
								STextStyle Name;
								Name.m_Size = 15.0f;
								Name.m_Font = EFont::BOLD;
								Name.m_Wrap = true;
								Name.m_MaxLines = 1;
								SBox NB;
								NB.m_W = Grow();
								char aName[IO_MAX_PATH_LENGTH];
								str_copy(aName, Parent ? Localize("Parent folder") : pItem->m_aName);
								const int Len = str_length(aName);
								if(Len > 1 && aName[Len - 1] == '/')
									aName[Len - 1] = '\0';
								Text(aName, Name, NB);
								char aMeta[128] = "";
								if(!pItem->m_IsDir)
								{
									char aDate[64];
									str_timestamp_ex(pItem->m_Date, aDate, sizeof(aDate), "%Y-%m-%d %H:%M");
									char aSize[32];
									FormatSize(pItem->m_Size, aSize, sizeof(aSize));
									if(pItem->m_InfosLoaded && pItem->m_Valid)
									{
										const int Length = pItem->Length();
										str_format(aMeta, sizeof(aMeta), "%s · %d:%02d · %s", pItem->m_Info.m_aMapName, Length / 60, Length % 60, aSize);
									}
									else
										str_format(aMeta, sizeof(aMeta), "%s · %s", aDate, aSize);
								}
								STextStyle Meta;
								Meta.m_Size = 12.0f;
								Meta.m_Color = XuiTheme::Color::TEXT3;
								Meta.m_Wrap = true;
								Meta.m_MaxLines = 1;
								Text(aMeta[0] != '\0' ? aMeta : " ", Meta, NB);
							});
						});
						SetTransform(CardNode, Scale, 1.0f);
						const ColorRGBA Accent = m_Accent;
						Paint(CardNode, [Hover, Selected, Accent](CXui &U, const SRect &Rc) {
							U.DrawRect(Rc, GlassRect(20.0f, XuiTheme::Glass::PANEL_ALPHA + 0.04f * Hover));
							U.DrawRect(Rc, GlassHairline(20.0f));
							if(Selected)
							{
								IGraphics::SUiRect Ring{};
								Ring.m_Radii = Rad(20.0f);
								Ring.m_BorderWidth = 2.0f;
								Ring.m_Border = WithAlpha(Accent, 0.8f);
								U.DrawRect(Rc, Ring);
							}
						});
					}
				});
			}
			if(Num > MAX_CARDS)
			{
				STextStyle S;
				S.m_Size = 12.5f;
				S.m_Color = XuiTheme::Color::TEXT3;
				char aBuf[96];
				str_format(aBuf, sizeof(aBuf), Localize("Showing %d of %d, use search to find more"), MAX_CARDS, Num);
				Text(aBuf, S);
			}
		});
	});

	if(s_CtxIndex >= 0 && s_CtxIndex < Num)
	{
		const CMenus::CDemoItem *pItem = M.m_vpFilteredDemos[s_CtxIndex];
		const SMenuItem aItems[] = {
			{pItem->m_IsDir ? Localize("Open") : Localize("Play"), pItem->m_IsDir ? Ph::FOLDER : Ph::PLAY},
			{Localize("Delete"), Ph::TRASH, true, pItem->m_StorageType != IStorage::TYPE_SAVE},
		};
		int Chosen = -1;
		ContextMenu("dm.ctx", pItem->m_aName, aItems, (int)std::size(aItems), &s_CtxOpen, s_CtxAt, &Chosen);
		if(Chosen == 0)
			Activated = s_CtxIndex;
		else if(Chosen == 1)
			Delete(s_CtxIndex);
		if(!s_CtxOpen)
			s_CtxIndex = -1;
	}

	if(M.m_Popup == CMenus::POPUP_NONE && FocusId() == 0 && M.m_DemolistSelectedIndex >= 0)
	{
		if(KeyPressed(KEY_RETURN) || KeyPressed(KEY_KP_ENTER))
			Activated = M.m_DemolistSelectedIndex;
		else if(KeyPressed(KEY_DELETE) && M.m_vpFilteredDemos[M.m_DemolistSelectedIndex]->m_StorageType == IStorage::TYPE_SAVE && str_comp(M.m_vpFilteredDemos[M.m_DemolistSelectedIndex]->m_aFilename, "..") != 0)
			Delete(M.m_DemolistSelectedIndex);
	}

	if(Activated >= 0)
		Activate(Activated);
}
