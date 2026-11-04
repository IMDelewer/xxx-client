#ifndef GAME_CLIENT_COMPONENTS_XXX_TRANSITION_H
#define GAME_CLIENT_COMPONENTS_XXX_TRANSITION_H

#include <game/client/component.h>

class CTransition : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnStateChange(int NewState, int OldState) override;
};

#endif
