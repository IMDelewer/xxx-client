#ifndef GAME_CLIENT_COMPONENTS_XXX_CUSTOM_COMMUNITIES_H
#define GAME_CLIENT_COMPONENTS_XXX_CUSTOM_COMMUNITIES_H

#include <engine/http.h>

#include <game/client/component.h>

#include <memory>

typedef struct _json_value json_value;

class CCustomCommunities : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnInit() override;
	void OnRender() override;
	void OnShutdown() override;

	void Download();

private:
	std::shared_ptr<IHttpRequest> m_pTask;
	json_value *m_pInfo = nullptr;

	void Load();
};

#endif
