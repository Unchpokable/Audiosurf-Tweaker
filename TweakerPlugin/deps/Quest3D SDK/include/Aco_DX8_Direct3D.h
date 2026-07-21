#if !defined(AFX_DX8_DIRECT3D_H__880EBC2C_BBDF_41A7_A9CE_8DB002D80C03__INCLUDED_)
#define AFX_DX8_DIRECT3D_H__880EBC2C_BBDF_41A7_A9CE_8DB002D80C03__INCLUDED_

#ifdef DX8_DIRECT3D_EXPORTS
#define DX8_DIRECT3D_API __declspec(dllexport)
#else
#define DX8_DIRECT3D_API __declspec(dllimport)
#endif

#define DX8_DIRECT3D_CHANNEL_NAME		"Direct3D"

// {D30F7991-36AC-47CF-9879-781759131388} 
static const GUID DX8_DIRECT3D_CHANNEL_GUID = { 0xd30f7991, 0x36ac, 0x47cf, { 0x98, 0x79, 0x78, 0x17, 0x59, 0x13, 0x13, 0x88 } };

class Aco_DX8_DirectGraphicsChannel;

class DX8_DIRECT3D_API Aco_DX8_Direct3D : public A3d_Channel  
{
public:
	Aco_DX8_Direct3D();
	virtual ~Aco_DX8_Direct3D();

	// Get the direct3d8 pointer that is used by quest3d
	virtual LPDIRECT3D9			GetDirect3d();
	// get the direct3d device8 pointer created trough the direct 3d interface
	// be carefull not to set any textures or render states directly since Quest3d will
	// then not know the current settings!
	virtual LPDIRECT3DDEVICE9	GetDirect3dDevice();
	// SetTextureStageState to change texture stage properties
	// this function will check if the state being set is not already used
	// please use this function to change a state
	virtual void				SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value);
	// set a render state of this device
	// this function will check if the state being set is not already used
	// please use this function to change a state
	virtual void				SetRenderState(DWORD type, DWORD newValue);
	// this function allows you to get the current window the Quest3D 3d engine is presenting its 3d result in
	virtual HWND				GetPresentWindow();

protected:
	// pointer back to direct graphics unique channel
	Aco_DX8_DirectGraphicsChannel*	directGraphics_;
};

// Leave rest of header file intact!
#define DX8_DIRECT3DDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_DX8_Direct3D; \
} \
}

#endif // !defined(AFX_DX8_DIRECT3D_H__880EBC2C_BBDF_41A7_A9CE_8DB002D80C03__INCLUDED_)
