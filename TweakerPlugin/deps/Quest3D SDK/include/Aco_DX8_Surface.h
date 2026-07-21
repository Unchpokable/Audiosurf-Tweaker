#if !defined(AFX_SURFACE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_SURFACE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

#ifdef SURFACE_EXPORTS
#define SURFACE_API __declspec(dllexport)
#else
#define SURFACE_API __declspec(dllimport)
#endif


#define		SURFACE_CHANNEL_NAME		"Surface"
#define		SURFACE_CHANNEL_VERSION		1

class Aco_DX8_ObjectDataChannel;
class Aco_DX8_DirectGraphicsChannel;
class Aco_DX8_Texture;
class Aco_DX8_SmartSurfaceUnique;

// {3237CF29-DB73-47d8-B4B9-A6CE2E1E60F1}
static const GUID SURFACE_CHANNEL_GUID = { 0x3237cf29, 0xdb73, 0x47d8, { 0xb4, 0xb9, 0xa6, 0xce, 0x2e, 0x1e, 0x60, 0xf1 } };			  
class SURFACE_API Aco_DX8_SurfaceChannel : public Aco_DX8_D3DDeviceUse 
{
public:
	Aco_DX8_SurfaceChannel();
	virtual ~Aco_DX8_SurfaceChannel();

	// call channel
	virtual void		CallChannel();
	// a channel must save itself with the filesaver class
	virtual	bool	    SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool		LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);

	// draw non transparent surface
	virtual void		DrawSurface();
	// draw surface
	virtual void		DoDrawSurface(bool resetStates=true);
	// draw transparent surface
	virtual void		DrawTransparentSurface();

	// set alpha reject
	virtual void		SetAlphaRejectValue(DWORD newValue);
	// get alpha reject
	virtual DWORD		GetAlphaRejectValue();

	// set alpha reject
	virtual void		SetAlphaRejectTest(bool newValue);
	// get alpha reject
	virtual bool		GetAlphaRejectTest();
	// set transparent surface
	virtual void		SetTransparentSurface(bool newValue);
	// get transparent surface
	virtual bool		GetTransparentSurface();
	// set transparent surface
	virtual void		SetVisible(bool newValue);
	// get transparent surface
	virtual bool		GetVisible();
	// SetSourceBlend
	virtual void		SetSourceBlend(DWORD newValue);
	// GetSourceBlend
	virtual DWORD		GetSourceBlend();
	// SetDestBlend
	virtual void		SetDestBlend(DWORD newValue);
	// GetDestBlend
	virtual DWORD		GetDestBlend();
	// get color op
	virtual DWORD		GetColorOp(int textLvL);
	// set color op
	virtual void		SetColorOp(DWORD newValue, int textLvL);
	// GetAplhaOp
	virtual DWORD		GetAplhaOp(int textLvL);
	// SetAlphaOp
	virtual void		SetAplhaOp(DWORD newValue, int textLvL);
	// get color op
	virtual DWORD		GetCoordIndex(int textLvL);
	// get color op
	virtual void		SetCoordIndex(DWORD newValue, int textLvL);
	// GetTextureWrap
	virtual DWORD		GetTextureWrap(int textLvL);
	// SetTextureWrap
	virtual void		SetTextureWrap(DWORD newValue, int textLvL);
	// SetWriteDepthBuffer
	virtual void		SetWriteDepthBuffer(bool newValue);
	// GetWriteDepthBuffer
	virtual bool		GetWriteDepthBuffer();
	// SetCheckDepthBuffer
	virtual void		SetCheckDepthBuffer(bool newValue);
	// GetCheckDepthBuffer
	virtual bool		GetCheckDepthBuffer();
	// SetTillingMode
	virtual void		SetTillingMode(DWORD newMode, int textLvL);
	// GetTillingMode
	virtual DWORD		GetTillingMode(int textLvL);

	// GetColorArg1
	virtual DWORD		GetColorArg1(int textLvL);
	// SetColorArg1
	virtual void		SetColorArg1(DWORD newValue, int textLvL);

	// GetColorArg2
	virtual DWORD		GetColorArg2(int textLvL);
	// SetColorArg2
	virtual void		SetColorArg2(DWORD newValue, int textLvL);

	// GetAlphaArg1
	virtual DWORD		GetAlphaArg1(int textLvL);
	// SetAlphaArg1
	virtual void		SetAlphaArg1(DWORD newValue, int textLvL);

	// GetAlphaArg2
	virtual DWORD		GetAlphaArg2(int textLvL);
	// SetAlphaArg2
	virtual void		SetAlphaArg2(DWORD newValue, int textLvL);
	
	// GetZBias
	virtual float		GetZBias();
	// SetZBias
	virtual void		SetZBias(float newValue);
	//get the object data
	virtual Aco_DX8_ObjectDataChannel* GetObjectData();

	// Let this channel add it's dependecies to the given list
	virtual void				DoDependencyInit(A3d_List* currentDependList);

	// what is the world matrix our object used
	virtual void		SetWorldMatrix(D3DXMATRIX matrix);
	// GetWorldMatrix
	virtual D3DXMATRIX	GetWorldMatrix();	
	// SetNoLightsForPreLight
	virtual void		SetNoLightsForPreLight(bool newValue);
	// GetNoLightsForPreLight
	virtual bool		GetNoLightsForPreLight();
	// set and get the material values trough the surface itself
	virtual void		SetMaterialValue(int id, float newValue);
	// set and get the material values trough the surface itself
	virtual float		GetMaterialValue(int id);
	// GetTextureFromStageNr
	virtual	Aco_DX8_Texture* GetTextureFromStageNr(int stageNr);
	// SetTextureAsStage
	virtual void		SetTextureAsStage(Aco_DX8_Texture* texture, int stageNr);
	// DrawSmartSurface
	virtual bool		DrawSmartSurface();
	// SetSmartSurface
	virtual void		SetSmartSurface(GUID newSurface);
	// GetSmartSurfaceChannel
	virtual Aco_DX8_SurfaceChannel*		GetResourceSurfaceChannel();
	// GetSmartSurface
	virtual GUID		GetSmartSurface();
	// GetSmartSurface
	virtual int			GetSmartSurfaceNr();
	// SetSmartSurfaceNr
	virtual void		SetSmartSurfaceNr(int newNr);
	// GetIfSmartSurface
	virtual bool		GetIfSmartSurface();
	// ClearTextureStage
	virtual void		ClearTextureStage(int stageNr);
	// SetSurfaceIsSmartSurfaceResource
	virtual void		SetSurfaceIsSmartSurfaceResource(bool newValue);


protected:
	// is this surface transparent
	bool				transparentSurface_;
	// do we want to draw this surface at all!
	bool				drawSurface_;
	// do we check the zbuffer or not
	bool				useDepthBufferCheck_;
	// useDepthBufferWrite_
	bool				useDepthBufferWrite_;
	// mode for sourceblend
	DWORD				sourceBlend_;
	// mode for destblend
	DWORD				destBlend_;
	// lets remember all settings specific for the texture
	DWORD				aplhaColorOp_[8];
	// textureColorArg1_
	DWORD				textureColorArg1_[8];
	// textureColorArg2_
	DWORD				textureColorArg2_[8];
	// aplhaColorArg1_
	DWORD				aplhaColorArg1_[8];
	// aplhaColorArg2_
	DWORD				aplhaColorArg2_[8];
	// lets remember all settings specific for the texture
	DWORD				textureColorOp_[8];
	DWORD				textureCoordIndx_[8];
	// texture wraping modes
	DWORD				textureWraping_[8];
	// what tilling do we use
	DWORD				tillingMode_[8];
	// zbias to use for this surface
	float				zBias_;
	// worldMatrix_
	D3DXMATRIX			worldMatrix_;
	// use alpha reject test
	bool				alphaRejectTest_;
	// zbias to use for this surface
	DWORD				alphaRefValue_;
	// do we turn off lights for prelit objects
	bool				noLightsForPreLight_;
	// guid of the template we are using
	GUID				surfaceGUID_;
	// Aco_DX8_SmartSurfaceUnique
	Aco_DX8_SmartSurfaceUnique* ssUnique_;
	// smart surface id
	int					surfaceGuidID_;
	// did we add a reference
	bool				addReference_;
	// use smartsurface
	bool				useSmartSurface_;
	// are we a resource? then we behave differently!
	bool				surfaceIsSmartSurfaceResource_;
};

#define DLL_EXPORTS_SURFACE extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_DX8_SurfaceChannel; \
} \
}  

#endif // !defined(AFX_SURFACE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
