#ifndef ENGINE_SERVER_XX_PROXY_H
#define ENGINE_SERVER_XX_PROXY_H

class CConfig;

int RunXxProxy(const CConfig *pConfig, bool (*pfnInterrupted)());

#endif
