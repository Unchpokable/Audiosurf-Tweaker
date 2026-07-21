#if !defined(AFX_DX8_D3DDEVICEUSE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_DX8_D3DDEVICEUSE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

// TODO: Put DX8_D3DDEVICEUSE_EXPORTS into your preprocessor definitions
#ifdef DX8_D3DDEVICEUSE_EXPORTS
#define DX8_D3DDEVICEUSE_API __declspec(dllexport)
#else
#define DX8_D3DDEVICEUSE_API __declspec(dllimport)
#endif

// TODO: Your number should come here
#define DX8_D3DDEVICEUSE_CHANNEL_NAME		"D3DDeviceUse"
#define DX8_D3DDEVICEUSE_CHANNEL_VERSION	1

// TODO: Use GUIDGEN to copy your own static const here!
static const GUID DX8_D3DDEVICEUSE_CHANNEL_GUID = { 0x85642ff9, 0x3940, 0x4196, { 0x95, 0x96, 0x90, 0x40, 0x9a, 0xf1, 0xcd, 0xb4 } };

class Aco_DX8_DirectGraphicsChannel;

class DX8_D3DDEVICEUSE_API Aco_DX8_D3DDeviceUse : public A3d_Channel  
{
public:
	Aco_DX8_D3DDeviceUse();
	virtual ~Aco_DX8_D3DDeviceUse();

	// invalidate device state
	virtual void			InvalidateDeviceObjects();
	// when direct graphics deletes itself and we still exist lets not call direct graphics anymore!
	virtual void			InvalidateDirectGraphics();

	// Let this channel add it's dependecies to the given list
	virtual void			DoDependencyInit(A3d_List* currentDependList);

protected:
	// pointer back to direct graphics unique channel
	Aco_DX8_DirectGraphicsChannel*	directGraphics_;
	// we are child of graphics channel
	int								graphicsIndex_;
};

#define DX8_D3DDEVICEUSEDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_DX8_D3DDeviceUse; \
} \
}

#endif // !defined(AFX_DX8_D3DDEVICEUSE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
