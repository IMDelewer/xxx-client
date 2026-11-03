#include "settings_internal.h"

#include <engine/client.h>
#include <engine/font_icons_phosphor.h>

#include <game/client/gameclient.h>
#include <game/localization.h>
#include <game/version.h>

using namespace Xui;

static constexpr const char *const CREDITS =
	"\n"
	"Help and code by eeeee, HMH, east, CookieMichal, Learath2, "
	"Savander, laxa, Tobii, BeaR, Wohoo, nuborn, timakro, Shiki, "
	"trml, Soreu, hi_leute_gll, Lady Saavik, Chairn, heinrich5991, "
	"swick, oy, necropotame, Ryozuki, Redix, d3fault, marcelherd, "
	"BannZay, ACTom, SiuFuWong, PathosEthosLogos, TsFreddie, "
	"Jupeyy, noby, ChillerDragon, ZombieToad, weez15, z6zzz, "
	"Piepow, QingGo, RafaelFF, sctt, jao, daverck, fokkonaut, "
	"Bojidar, FallenKN, ardadem, archimede67, sirius1242, Aerll, "
	"trafilaw, Zwelf, Patiga, Konsti, ElXreno, MikiGamer, "
	"Fireball, Banana090, axblk, yangfl, Kaffeine, Zodiac, "
	"c0d3d3v, GiuCcc, Ravie, Robyt3, simpygirl, Tater, Cellegen, "
	"srdante, Nouaa, Voxel, luk51, Vy0x2, Avolicious, louis, "
	"Marmare314, hus3h, ArijanJ, tarunsamanta2k20, Possseidon, "
	"+KZ, Teero, furo, dobrykafe, Moiman, JSaurusRex, "
	"Steinchen, ewancg, gerdoe-jr, melon, KebsCS, bencie, "
	"DynamoFox, MilkeeyCat, iMilchshake, SchrodingerZhu, "
	"catseyenebulous, Rei-Tw, Matodor, Emilcha, art0007i, SollyBunny, "
	"0xfaulty, AssassinTee, Pioooooo, ASKLL-STAR, K1nop1c0, "
	"Bamcane, qxdFox, ZerolAcqua, swarfeya, Scrumplex, 12944qwerty, "
	"Pointer31, ProfSapphire, 0xpixty, GlimmeR, horoni & others\n"
	"\n"
	"Visual features ported from T-Client by the TaterClient team.\n"
	"\n"
	"Based on DDRace by the DDRace developers,";

const char *SettingsCreditsText()
{
	return CREDITS;
}

void SettingsUi::BuildAbout(SSection &S)
{
	S.Block("xxx", []() {
		CXui &U = Ui();
		SBox Brand;
		Brand.m_W = Grow();
		Brand.m_Gap = 12.0f;
		Brand.m_Padding = SEdges(12.0f);
		Brand.m_Cross = EAlign::CENTER;
		U.Column(Brand, [&]() {
			SBox Logo;
			Logo.m_W = Logo.m_H = Px(80.0f);
			U.Paint(U.Row(Logo), [](CXui &Ui, const SRect &Rc) {
				Ui.DrawImage(Rc, Ui.ClientIcon(), vec2(256, 256), 18.0f);
			});
			STextStyle Version;
			Version.m_Size = 14.0f;
			Version.m_Color = XuiTheme::Color::TEXT2;
			U.Text(XXX_VERSION, Version);
			SBox Links;
			Links.m_Gap = 10.0f;
			Links.m_Main = EAlign::CENTER;
			Links.m_Wrap = true;
			U.Row(Links, [&]() {
				if(U.Button(Localize("Author"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::GITHUB_LOGO, .m_Height = 36.0f, .m_IconFill = true, .m_pKey = "about.author"}).m_Clicked)
					Game()->Client()->ViewLink("https://github.com/IMDelewer");
				if(U.Button(Localize("Repository"), {.m_Variant = EVariant::GHOST, .m_pIcon = Ph::GITHUB_LOGO, .m_Height = 36.0f, .m_IconFill = true, .m_pKey = "about.repository"}).m_Clicked)
					Game()->Client()->ViewLink("https://github.com/IMDelewer/xxx-client");
			});
		});
	});
	S.Header("Built with help from");
	S.Block("Projects", []() {
		STextStyle Body;
		Body.m_Size = 14.0f;
		Body.m_Color = XuiTheme::Color::TEXT2;
		Body.m_Wrap = true;
		Ui().Text(Localize("DDNet / DDRace / Teeworlds — game, networking, physics and editor."), Body, {.m_W = Grow()});
		Ui().Text(Localize("T-Client — adapted visual features and custom communities. Tater and contributors."), Body, {.m_W = Grow()});
		Ui().Text(Localize("BestClient — adapted client features and HUD editor ideas. RoflikBEST and BestProject contributors."), Body, {.m_W = Grow()});
	});
	S.Header("Licenses");
	S.Block("Licenses", []() {
		STextStyle Body;
		Body.m_Size = 13.0f;
		Body.m_Color = XuiTheme::Color::TEXT2;
		Body.m_Wrap = true;
		Ui().Text(Localize("DDNet and Teeworlds: zlib. BestClient contributions: MIT. Fonts, icons and other assets retain their own licenses. See licenses/DDNet.txt and licenses/README.md."), Body, {.m_W = Grow()});
		Ui().Text(Localize("Original interface design: XUI (xxx client), IMDelewer. When reusing the design, credit XUI (xxx client) in your README or in-game About page. See LICENSE."), Body, {.m_W = Grow()});
	});
	S.Header("DDNet contributors");
	S.Block("Contributors", []() {
		STextStyle Body;
		Body.m_Size = 12.0f;
		Body.m_Color = XuiTheme::Color::TEXT3;
		Body.m_Wrap = true;
		Ui().Text(SettingsCreditsText(), Body, {.m_W = Grow()});
	});
}
