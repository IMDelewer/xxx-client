#include "xx_netdump.h"

#include <base/io.h>
#include <base/log.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/shared/protocol.h>
#include <engine/shared/protocol7.h>
#include <engine/shared/protocol_ex.h>
#include <engine/shared/uuid_manager.h>
#include <engine/storage.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <iterator>

static const char *SysMsgName(int Msg, bool Sixup)
{
	static const char *const s_apNames[] = {"EX", "INFO", "MAP_CHANGE", "MAP_DATA", "CON_READY", "SNAP", "SNAPEMPTY", "SNAPSINGLE", "SNAPSMALL", "INPUTTIMING", "RCON_AUTH_STATUS", "RCON_LINE", "UNUSED1", "UNUSED2", "READY", "ENTERGAME", "INPUT", "RCON_CMD", "RCON_AUTH", "REQUEST_MAP_DATA", "UNUSED3", "UNUSED4", "PING", "PING_REPLY", "UNUSED5", "RCON_CMD_ADD", "RCON_CMD_REM"};
	static const char *const s_apNames7[] = {"EX", "INFO", "MAP_CHANGE", "MAP_DATA", "SERVERINFO", "CON_READY", "SNAP", "SNAPEMPTY", "SNAPSINGLE", "SNAPSMALL", "INPUTTIMING", "RCON_AUTH_ON", "RCON_AUTH_OFF", "RCON_LINE", "RCON_CMD_ADD", "RCON_CMD_REM", "UNUSED1", "UNUSED2", "READY", "ENTERGAME", "INPUT", "RCON_CMD", "RCON_AUTH", "REQUEST_MAP_DATA", "UNUSED3", "UNUSED4", "PING", "PING_REPLY", "UNUSED5", "MAPLIST_ENTRY_ADD", "MAPLIST_ENTRY_REM"};
	if(Sixup)
		return Msg >= 0 && Msg < (int)std::size(s_apNames7) ? s_apNames7[Msg] : "?";
	if(Msg >= 0 && Msg < (int)std::size(s_apNames))
		return s_apNames[Msg];
	return "?";
}

static bool Readable(const char *pRun, int Len)
{
	if(Len < 3 || !str_utf8_check(pRun))
		return false;
	int Plain = 0;
	for(int i = 0; i < Len; i++)
		Plain += (pRun[i] >= 'a' && pRun[i] <= 'z') || (pRun[i] >= 'A' && pRun[i] <= 'Z') || (pRun[i] >= '0' && pRun[i] <= '9') || pRun[i] == ' ' || (unsigned char)pRun[i] >= 0x80;
	return Plain * 10 >= Len * 7;
}

void XxNetDump(IStorage *pStorage, int Level, const char *pDir, const char *pWho, bool Sys, int Msg, const unsigned char *pData, int Size, bool Vital, bool Sixup)
{
	if(Level <= 0)
		return;
	const bool NoisySys = Sixup ?
				      (Msg == protocol7::NETMSG_SNAP || Msg == protocol7::NETMSG_SNAPEMPTY || Msg == protocol7::NETMSG_SNAPSINGLE || Msg == protocol7::NETMSG_SNAPSMALL || Msg == protocol7::NETMSG_INPUT || Msg == protocol7::NETMSG_INPUTTIMING || Msg == protocol7::NETMSG_PING || Msg == protocol7::NETMSG_PING_REPLY || Msg == protocol7::NETMSG_MAP_DATA || Msg == protocol7::NETMSG_REQUEST_MAP_DATA || Msg == protocol7::NETMSG_RCON_CMD_ADD || Msg == protocol7::NETMSG_RCON_CMD_REM) :
				      (Msg == NETMSG_SNAP || Msg == NETMSG_SNAPEMPTY || Msg == NETMSG_SNAPSINGLE || Msg == NETMSG_SNAPSMALL || Msg == NETMSG_INPUT || Msg == NETMSG_INPUTTIMING || Msg == NETMSG_PING || Msg == NETMSG_PING_REPLY || Msg == NETMSG_MAP_DATA || Msg == NETMSG_REQUEST_MAP_DATA || Msg == NETMSG_RCON_CMD_ADD || Msg == NETMSG_RCON_CMD_REM);
	const bool NoisyGame = Msg == NETMSGTYPE_SV_PREINPUT || Msg == NETMSGTYPE_SV_COMMANDINFO || Msg == NETMSGTYPE_SV_COMMANDINFOREMOVE || Msg == NETMSGTYPE_SV_COMMANDINFOGROUPSTART || Msg == NETMSGTYPE_SV_COMMANDINFOGROUPEND ||
			       (Sixup && (Msg == protocol7::NETMSGTYPE_SV_COMMANDINFO || Msg == protocol7::NETMSGTYPE_SV_COMMANDINFOREMOVE));
	if(Level < 2 && (Sys ? NoisySys : NoisyGame))
		return;
	static CNetObjHandler s_NetObjHandler;
	static protocol7::CNetObjHandler s_NetObjHandler7;
	const char *pName;
	if(Msg >= OFFSET_UUID)
	{
		pName = g_UuidManager.GetName(Msg);
		if(pName == nullptr)
			pName = "unknown-uuid";
	}
	else
		pName = Msg == UUID_UNKNOWN ? "unknown-uuid" : (Sys ? SysMsgName(Msg, Sixup) : (Sixup ? s_NetObjHandler7.GetMsgName(Msg) : s_NetObjHandler.GetMsgName(Msg)));

	char aStrings[600] = "";
	char aRun[256];
	int RunLen = 0;
	for(int i = 0; i <= Size; i++)
	{
		const unsigned char c = i < Size ? pData[i] : 0;
		if(c >= 32 && c != 127 && RunLen < (int)sizeof(aRun) - 1)
			aRun[RunLen++] = (char)c;
		else
		{
			aRun[RunLen] = '\0';
			if(Readable(aRun, RunLen))
			{
				if(aStrings[0] != '\0')
					str_append(aStrings, " | ");
				str_append(aStrings, "\"");
				str_append(aStrings, aRun);
				str_append(aStrings, "\"");
			}
			RunLen = 0;
		}
	}
	char aHex[3 * 48 + 8] = "";
	for(int i = 0; i < Size && i < 48; i++)
	{
		char aByte[4];
		str_format(aByte, sizeof(aByte), "%02x ", pData[i]);
		str_append(aHex, aByte);
	}
	if(Size > 48)
		str_append(aHex, "...");
	char aLine[1500];
	str_format(aLine, sizeof(aLine), "%s %s %s %s (%d) %s%d bytes%s%s | %s", pDir, pWho, Sys ? "sys" : "game", pName, Msg, Vital ? "vital " : "", Size, aStrings[0] ? " | " : "", aStrings, aHex);
	log_info("net", "%s", aLine);
	if(pStorage == nullptr)
		return;
	IOHANDLE File = pStorage->OpenFile("xxx_net_dump.txt", IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char aTime[64];
	str_timestamp(aTime, sizeof(aTime));
	char aOut[1600];
	str_format(aOut, sizeof(aOut), "[%s] %s", aTime, aLine);
	io_write(File, aOut, str_length(aOut));
	io_write_newline(File);
	io_close(File);
}
