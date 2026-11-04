// This file can be included several times.
// Config variables of the xxx client. All of them use the `xx_` prefix.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) ;
#endif

// ui
MACRO_CONFIG_INT(XxFontPrompt, xx_font_prompt, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Offer the optional language fonts on first launch")
MACRO_CONFIG_INT(XxSettingsTab, xx_settings_tab, 0, 0, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Selected tab of the xxx settings page")
MACRO_CONFIG_COL(XxUiAccent, xx_ui_accent, 0xBDFFB6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Accent color of the user interface")
#if defined(DDEV_XX)
MACRO_CONFIG_INT(XxUiDebug, xx_ui_debug, 0, 0, 1, CFGFLAG_CLIENT, "Show the number of ui rects, ui draw calls and the glass blur and ui rect GPU times")
#endif
#if defined(DDEV_XX)
MACRO_CONFIG_INT(XxGlassTest, xx_glass_test, 0, 0, 1, CFGFLAG_CLIENT, "Render the glass test scene over the screen")
#endif
#if defined(DDEV_XX)
MACRO_CONFIG_INT(XxUiGallery, xx_ui_gallery, 0, 0, 9, CFGFLAG_CLIENT, "Open the XUI widget gallery (1-5 selects the page, 6 context menu, 7 modal, 8 toast, 9 popover)")
#endif
MACRO_CONFIG_INT(XxUiSettingDescriptions, xx_ui_setting_descriptions, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show setting descriptions inline instead of in help tooltips")
MACRO_CONFIG_STR(XxCustomCommunitiesUrl, xx_custom_communities_url, 256, "https://raw.githubusercontent.com/TaterClient/ddnet-custom-communities/refs/heads/main/custom-communities-ddnet-info.json", CFGFLAG_CLIENT | CFGFLAG_SAVE, "URL to fetch custom communities from (https), empty to disable")
MACRO_CONFIG_INT(XxUiScale, xx_ui_scale, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Scale of the XUI interface in percent")
MACRO_CONFIG_INT(XxUiTheme, xx_ui_theme, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "XUI theme: 0=dark, 1=light")
MACRO_CONFIG_INT(XxUiGlass, xx_ui_glass, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Liquid glass interface panels")
MACRO_CONFIG_INT(XxUiTransparency, xx_ui_transparency, 26, 0, 90, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Transparency of interface panels in percent")
MACRO_CONFIG_INT(XxUiBlur, xx_ui_blur, 100, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Backdrop blur strength in percent")
MACRO_CONFIG_STR(XxUiBackground, xx_ui_background, 1024, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom PNG menu background; empty uses the bundled background")

MACRO_CONFIG_STR(XxLastServerAddress, xx_last_server_address, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Address of the last successfully joined server")
MACRO_CONFIG_STR(XxLastServerName, xx_last_server_name, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cached name of the last joined server")
MACRO_CONFIG_STR(XxLastServerMap, xx_last_server_map, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cached map of the last joined server")

// island
MACRO_CONFIG_INT(XxIsland, xx_island, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the dynamic island")
MACRO_CONFIG_INT(XxIslandIdle, xx_island_idle, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the collapsed island when there is nothing to report")
MACRO_CONFIG_INT(XxIslandScale, xx_island_scale, 100, 50, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the dynamic island in percent")
MACRO_CONFIG_INT(XxIslandOffset, xx_island_offset, 14, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Distance of the dynamic island from the top of the screen")
MACRO_CONFIG_INT(XxIslandDuration, xx_island_duration, 100, 25, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long notifications stay in the island in percent")
MACRO_CONFIG_INT(XxIslandSounds, xx_island_sounds, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a sound when the island shows a notification")
MACRO_CONFIG_INT(XxIslandSrcMusic, xx_island_src_music, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show now playing in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcSplits, xx_island_src_splits, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show split deltas in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcClips, xx_island_src_clips, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clip events in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcAchievements, xx_island_src_achievements, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show achievements in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcBlock, xx_island_src_block, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show block events in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcFriends, xx_island_src_friends, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show friend events in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcChat, xx_island_src_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat highlights in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcVotes, xx_island_src_votes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show votes in the dynamic island")
MACRO_CONFIG_INT(XxIslandSrcDownload, xx_island_src_download, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show map downloads in the dynamic island")

// visual (tclient port)
MACRO_CONFIG_INT(XxNameplatePingCircle, xx_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a circle to indicate ping in the nameplate")
MACRO_CONFIG_INT(XxNameplateCountry, xx_nameplate_country, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows the country flag in the nameplate")
MACRO_CONFIG_INT(XxNameplateSkins, xx_nameplate_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows skin names in nameplates, good for finding missing skins")
MACRO_CONFIG_INT(XxHammerRotatesWithCursor, xx_hammer_rotates_with_cursor, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow your hammer to rotate like other weapons")
MACRO_CONFIG_INT(XxColorFreeze, xx_color_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use skin colors for frozen tees")
MACRO_CONFIG_INT(XxColorFreezeDarken, xx_color_freeze_darken, 90, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes color of tees darker when in freeze (0-100)")
MACRO_CONFIG_INT(XxColorFreezeFeet, xx_color_freeze_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also use color for frozen tee feet")
MACRO_CONFIG_INT(XxWhiteFeet, xx_white_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render all feet as perfectly white base color")
MACRO_CONFIG_STR(XxWhiteFeetSkin, xx_white_feet_skin, 255, "x_ninja", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Base skin for white feet")
MACRO_CONFIG_INT(XxRenderWeaponsAsGun, xx_render_weapons_as_gun, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders weapons as the gun sprite instead of the weapon, with the exception of hammer and ninja (1 = with hue, 2 = without hue)")
MACRO_CONFIG_INT(XxRenderCursorSpec, xx_cursor_in_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render your gun cursor when spectating in freeview")
MACRO_CONFIG_INT(XxRenderCursorSpecAlpha, xx_cursor_in_spec_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of cursor in freeview")
MACRO_CONFIG_INT(XxTinyTees, xx_tiny_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render tees smaller")
MACRO_CONFIG_INT(XxTinyTeeSize, xx_tiny_tees_size, 100, 85, 115, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Define the Size of the Tiny Tee")
MACRO_CONFIG_INT(XxTinyTeesOthers, xx_tiny_tees_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render other tees smaller")
MACRO_CONFIG_INT(XxCursorStyle, xx_cursor_style, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "In game cursor style (0 game skin, 1 crosshair)")
MACRO_CONFIG_INT(XxCursorScale, xx_cursor_scale, 100, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Percentage to scale the in game cursor by as a percentage (50 = half, 200 = double)")
MACRO_CONFIG_INT(XxRainbowTees, xx_rainbow_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side")
MACRO_CONFIG_INT(XxRainbowHook, xx_rainbow_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow hook")
MACRO_CONFIG_INT(XxRainbowWeapon, xx_rainbow_weapon, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow Weapons")
MACRO_CONFIG_INT(XxRainbowOthers, xx_rainbow_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side for others")
MACRO_CONFIG_INT(XxRainbowMode, xx_rainbow_mode, 1, 1, 4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow mode (1: rainbow, 2: pulse, 3: darkness, 4: random)")
MACRO_CONFIG_INT(XxRainbowSpeed, xx_rainbow_speed, 100, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow speed as a percentage (50 = half speed, 200 = double speed)")
MACRO_CONFIG_INT(XxTeeTrail, xx_tee_trail, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Tee trails")
MACRO_CONFIG_INT(XxTeeTrailOthers, xx_tee_trail_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tee trails for other players")
MACRO_CONFIG_INT(XxTeeTrailWidth, xx_tee_trail_width, 15, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail width")
MACRO_CONFIG_INT(XxTeeTrailLength, xx_tee_trail_length, 25, 5, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail length")
MACRO_CONFIG_INT(XxTeeTrailAlpha, xx_tee_trail_alpha, 80, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail alpha")
MACRO_CONFIG_COL(XxTeeTrailColor, xx_tee_trail_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color")
MACRO_CONFIG_INT(XxTeeTrailTaper, xx_tee_trail_taper, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Taper tee trail over length")
MACRO_CONFIG_INT(XxTeeTrailFade, xx_tee_trail_fade, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade trail alpha over length")
MACRO_CONFIG_INT(XxTeeTrailColorMode, xx_tee_trail_color_mode, 1, 1, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color mode (1: Solid color, 2: Current Tee color, 3: Rainbow, 4: Color based on Tee speed, 5: Random)")

// gameplay (tclient port)

// block

MACRO_CONFIG_STR(XxHudLayout, xx_hud_layout, 512, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Positions of HUD modules")
MACRO_CONFIG_INT(XxChatWidth, xx_chat_width, 520, 300, 900, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Chat width")
MACRO_CONFIG_INT(XxFocus, xx_focus, 0, 0, 1, CFGFLAG_CLIENT, "Focus mode: hide chat, pills and other players' names (bind: toggle xx_focus 0 1)")
MACRO_CONFIG_INT(XxHudVote, xx_hud_vote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the vote card in the HUD")
MACRO_CONFIG_INT(XxIslandDock, xx_island_dock, 0, 0, 1023, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD pills shown inside the island (bitmask)")
MACRO_CONFIG_INT(XxHudPing, xx_hud_ping, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ping in the HUD")
MACRO_CONFIG_INT(XxHudAngle, xx_hud_angle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the aim angle in the HUD (0 = right, counter-clockwise)")
// camera
MACRO_CONFIG_INT(XxCameraSmooth, xx_camera_smooth, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera follow smoothing (0 off)")
MACRO_CONFIG_INT(XxCameraLead, xx_camera_lead, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Camera looks ahead in the direction you move")
// race
#if defined(DDEV_XX)
MACRO_CONFIG_INT(XxDevFakePlayers, xx_dev_fake_players, 0, 0, 127, CFGFLAG_CLIENT, "Duplicate scoreboard rows for layout testing")
#endif

MACRO_CONFIG_INT(XxParentPid, xx_parent_pid, 0, 0, 2147483647, CFGFLAG_SERVER, "Shut the server down when the process with this id exits (local server of the client)")

MACRO_CONFIG_INT(XxNetDump, xx_net_dump, 0, 0, 2, CFGFLAG_SERVER, "Log network messages to console and xxx_net_dump.txt (1 without snapshots/input/preinput/command lists, 2 everything)")
MACRO_CONFIG_STR(XxProxyDst, xx_proxy_dst, 256, "", CFGFLAG_SERVER, "UDP proxy destination, hostname or IP with optional port (default 8303; set before startup)")
MACRO_CONFIG_INT(XxProxyNet, xx_proxy_net, 0, 0, 2, CFGFLAG_SERVER, "Run a UDP proxy and dump to console (0 off, 1 without snapshots/input/preinput/command lists, 2 everything; set before startup)")


// social

// sound
