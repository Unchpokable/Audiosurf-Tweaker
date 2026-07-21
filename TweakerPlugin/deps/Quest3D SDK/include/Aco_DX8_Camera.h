#if !defined(AFX_DX8_CAMERA_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_DX8_CAMERA_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the FLOAT_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// FLOAT_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef DX8_CAMERA_EXPORTS
#define DX8_CAMERA_API __declspec(dllexport)
#else
#define DX8_CAMERA_API __declspec(dllimport)
#endif

#define		DX8_CAMERA_NAME			"Camera"
#define		DX8_CAMERA_VERSION		1
// {1118038E-554C-492c-8E03-928F76A7EEC0}
static const GUID DX8_CAMERA_GUID = { 0x1118038e, 0x554c, 0x492c, { 0x8e, 0x3, 0x92, 0x8f, 0x76, 0xa7, 0xee, 0xc0 } };

class Aco_DX8_DirectGraphicsChannel;
class Aco_FloatChannel;

struct D3DXMATRIX;

class DX8_CAMERA_API Aco_DX8_CameraChannel : public A3d_Channel  
{
public:
	Aco_DX8_CameraChannel();
	virtual ~Aco_DX8_CameraChannel();

	// use this function so that the camera will use itself as the current view matrix
	virtual void				CallChannel();

	// lets use this function to quickly get the float values
	virtual float				GetChannelValue(int nr);
	//SetChannelValue quick set of our float childeren
	virtual void				SetChannelValue(int nr, float newValue);
	// after the engine initializes the childeren it calls the OneTimeInitialize function 
	virtual void				OneTimeInitialize();
	// some information may also become a channel so that we can control it from outside :)
	virtual void				UpdateCameraInfo();

	// we can also set them by hand :)
	virtual void				SetUseFog(bool newValue);
	// get if we use fog
	virtual bool				GetUseFog();
	// GetFogMode
	virtual int					GetFogMode();
	// SetFogMode
	virtual void				SetFogMode(int forMode);
	// GetFogStart
	virtual float				GetFogStart();
	// SetFogStart
	virtual void				SetFogStart(float newValue);
	// GetFogEnd
	virtual float				GetFogEnd();
	// SetFogEnd
	virtual void				SetFogEnd(float newValue);
	// GetFogDensity
	virtual float				GetFogDensity();
	// SetFogDensity
	virtual void				SetFogDensity(float newValue);
	// GetFogColor
	virtual DWORD				GetFogColor();
	// windows uses rgb lets also be able to give that
	virtual DWORD				GetRGBFogColor();
	// SetFogColor
	virtual void				SetFogColor(DWORD newRGBValue);
	// GetZoomFactor
	virtual float				GetZoomFactor();
	// SetZoomFactor
	virtual void				SetZoomFactor(float newValue);
	// a channel must save itself with the filesaver class
	virtual	bool				SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool				LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);

	// Let this channel add it's dependecies to the given list
	virtual void				DoDependencyInit(A3d_List* currentDependList);

protected:
	// pointer back to direct graphics unique channel
	Aco_DX8_DirectGraphicsChannel*		directGraphics_;
	// matrix
	D3DXMATRIX							*matrix_;
	// do we use fog
	bool								useFog_;
	// fog mode
	int									fogMode_;
	// fog start distance
	float								fogStart_;
	// fog end distance
	float								fogEnd_;
	// fogDensity_
	float								fogDensity_;
	// fog color
	DWORD								fogColor_;
	// zoom factor
	float								zoomFactor_;
	// for settings and zoom
	float								cameraSettings_[10];
	// 	callCameras_ = (Aco_FloatChannel *
	Aco_FloatChannel *					noCameras_;
};

#define DX8_CAMERADLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_DX8_CameraChannel; \
} \
}  

#endif // !defined(AFX_DX8_CAMERA_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
