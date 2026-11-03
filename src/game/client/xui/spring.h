#ifndef GAME_CLIENT_XUI_SPRING_H
#define GAME_CLIENT_XUI_SPRING_H

#include "theme.h"

#include <cmath>

class CSpring
{
public:
	float m_Value = 0.0f;
	float m_Velocity = 0.0f;
	float m_Target = 0.0f;

	void Snap(float Value)
	{
		m_Value = m_Target = Value;
		m_Velocity = 0.0f;
	}

	float Update(float Target, float Dt, XuiTheme::Motion::SSpring Spec)
	{
		m_Target = Target;
		Dt = std::fmin(Dt, 1.0f / 20.0f);
		const int Steps = (int)std::ceil(Dt / (1.0f / 240.0f));
		const float h = Dt / Steps;
		for(int i = 0; i < Steps; i++)
		{
			const float Accel = Spec.m_Stiffness * (m_Target - m_Value) - Spec.m_Damping * m_Velocity;
			m_Velocity += Accel * h;
			m_Value += m_Velocity * h;
		}
		if(std::fabs(m_Target - m_Value) < 0.0005f && std::fabs(m_Velocity) < 0.005f)
			Snap(m_Target);
		return m_Value;
	}

	bool Resting() const { return m_Value == m_Target && m_Velocity == 0.0f; }
};

#endif
