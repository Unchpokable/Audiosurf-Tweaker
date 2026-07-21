#pragma once

class DLLHIGHPOLY_API EngineListener
{
public:
	EngineListener(void);
	virtual ~EngineListener(void);

	virtual void		OnAboutToReleaseChannel(A3d_Channel* channel);
};
