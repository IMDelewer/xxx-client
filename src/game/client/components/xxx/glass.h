#ifndef GAME_CLIENT_COMPONENTS_XXX_GLASS_H
#define GAME_CLIENT_COMPONENTS_XXX_GLASS_H

#include <base/color.h>

class IGraphics;

namespace Glass
{
ColorRGBA Accent();
void Draw(IGraphics *pGraphics, float x, float y, float w, float h, ColorRGBA Tint, float Hot, int Corners, float Rounding);
}

#endif
