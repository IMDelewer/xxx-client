#include "settings_internal.h"

#include <base/str.h>

#include <engine/shared/config.h>

namespace SettingsUi
{
bool ConfigInt(const void *pVar, long long &Default, long long &Min, long long &Max)
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, MinV, MaxV, Flags, Desc) \
	if(pVar == (const void *)&g_Config.m_##Name) \
	{ \
		Default = Def; \
		Min = MinV; \
		Max = MaxV; \
		return true; \
	}
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) \
	if(pVar == (const void *)&g_Config.m_##Name) \
	{ \
		Default = (long long)(unsigned)(Def); \
		Min = 0; \
		Max = 0xFFFFFFFFll; \
		return true; \
	}
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc)
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
	return false;
}

bool DefaultString(const char *pVar, std::string &Default)
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc)
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	if(pVar == (const char *)g_Config.m_##Name) \
	{ \
		Default = Def; \
		return true; \
	}
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
	return false;
}
}
