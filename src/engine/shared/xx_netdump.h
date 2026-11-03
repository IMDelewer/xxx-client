#ifndef ENGINE_SHARED_XX_NETDUMP_H
#define ENGINE_SHARED_XX_NETDUMP_H

class IStorage;

void XxNetDump(IStorage *pStorage, int Level, const char *pDir, const char *pWho, bool Sys, int Msg, const unsigned char *pData, int Size, bool Vital, bool Sixup = false);

#endif
