#ifndef GAME_CLIENT_COMPONENTS_XXX_FRAME_METRICS_H
#define GAME_CLIENT_COMPONENTS_XXX_FRAME_METRICS_H

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

struct SFrameMetrics
{
	int m_Frames = 0;
	double m_Fps = 0;
	double m_Mean = 0;
	double m_P50 = 0;
	double m_P95 = 0;
	double m_P99 = 0;
	double m_Max = 0;
	int m_Over8 = 0;
	int m_Over16 = 0;

	static SFrameMetrics Compute(std::vector<double> Times)
	{
		SFrameMetrics Result;
		Times.erase(std::remove_if(Times.begin(), Times.end(), [](double Time) { return !std::isfinite(Time) || Time < 0; }), Times.end());
		if(Times.empty())
			return Result;
		std::sort(Times.begin(), Times.end());
		Result.m_Frames = Times.size();
		Result.m_Mean = std::accumulate(Times.begin(), Times.end(), 0.0) / Times.size();
		Result.m_Fps = Result.m_Mean > 0 ? 1000.0 / Result.m_Mean : 0;
		auto Percentile = [&](double Fraction) { return Times[std::max(0, (int)std::ceil(Times.size() * Fraction) - 1)]; };
		Result.m_P50 = Percentile(0.50);
		Result.m_P95 = Percentile(0.95);
		Result.m_P99 = Percentile(0.99);
		Result.m_Max = Times.back();
		Result.m_Over8 = std::count_if(Times.begin(), Times.end(), [](double Time) { return Time > 8.33; });
		Result.m_Over16 = std::count_if(Times.begin(), Times.end(), [](double Time) { return Time > 16.67; });
		return Result;
	}
};

#endif
