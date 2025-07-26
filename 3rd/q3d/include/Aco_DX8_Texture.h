#if !defined(AFX_DX8_TEXTURE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_DX8_TEXTURE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

// TODO: Put DX8_TEXTURE_EXPORTS into your preprocessor definitions
#ifdef DX8_TEXTURE_EXPORTS
#define DX8_TEXTURE_API __declspec(dllexport)
#else
#define DX8_TEXTURE_API __declspec(dllimport)
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9math.h>

// TODO: Your number should come here
#define DX8_TEXTURE_CHANNEL_NAME    "Texture"
#define DX8_TEXTURE_CHANNEL_VERSION 1
// {BC052C38-2D5D-4f0c-A0CA-654D0AFC584A}
static const GUID DX8_TEXTURE_CHANNEL_GUID = { 0xbc052c38, 0x2d5d, 0x4f0c, { 0xa0, 0xca, 0x65, 0x4d, 0xa, 0xfc, 0x58, 0x4a } };

class Aco_DX8_TextureManager;
class Aco_DX8_DirectGraphicsChannel;

class DX8_TEXTURE_API Aco_DX8_Texture : public Aco_DX8_D3DDeviceUse
{
public:
	Aco_DX8_Texture();
	virtual ~Aco_DX8_Texture();

	// release all created information
	virtual void			Release();
	// a channel must save itself with the filesaver class
	virtual	bool			SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool			LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);
	// the baseclass will call this function when the device is reset or destroyed
	virtual void			InvalidateDeviceObjects();

	// the internal IDirect3DTexture9* texture if available will be set at the stage 
	virtual void			SetTexture(int stage);

	// loading functions
	// load a texture from a file on disk
	virtual bool			LoadTextureFromFile(char* fileName);
	// load a texture from a loaded file through a buffer
	virtual bool			LoadTextureFromMemory(char* buffer, int bufferSize);
	// load a texture from a loaded file through a buffer and remember the buffer when saved
	virtual bool			LoadTextureFromMemoryAndCopyBuffer(char* buffer, int bufferSize);
	
	// mipmap levels used on our texture
	virtual void			SetMipMapLevels(int newCount);
	// mipmap levels used on our texture
	virtual int				GetMipMapLevels();

	// Get texture description from our texture and specify which mipmap level to get 
	// the information from (mipmap is actualy several textures in one)
	virtual D3DSURFACE_DESC GetTextureDescription(int lvl);
	// Lock the texture so you can acces its information/alter information
	// specify which mipmap level to lock
	virtual HRESULT			LockTexture(int level, D3DLOCKED_RECT& pLockedRect);
	// unlock texture which mipmap level to unlock
	virtual void			UnlockTexture(int level);
	// get if the texture is valid
	virtual bool			GetIfValidTexture();
	// SetDesiredHeight the texture will be rescaled to this size if the driver allow is
	// else to the first available size possible
	virtual void			SetDesiredHeight(int newValue);
	// SetDesiredWidth the texture will be rescaled to this size if the driver allow is
	// else to the first available size possible
	virtual void			SetDesiredWidth(int newValue);
	// GetDesiredHeight
	virtual int				GetDesiredHeight();
	// GetDesiredWidth
	virtual int				GetDesiredWidth();
	// When invalidated this function will be called to restore the texture
	virtual void			RestoreTexture();

	// Create a new texture channel that is a small representation of our bigger texture
	// specify the requested size this is used in Quest3D to create tumbnails
	virtual Aco_DX8_Texture* CreateTumbNailTexture(int newSizeX, int newSizeY);
	// AddAlphaTexture this function is used internaly to use this texture as a alpha map on
	// our current texture this is not a good method to create alpha using TGA files
	// with alpha maps in it is a better and faster method
	virtual bool			AddAlphaTexture(Aco_DX8_Texture* texture);

	// CopyOurselvesIntoTexture is able to copy the current texture into a give texture channel
	virtual bool			CopyOurselvesIntoTexture(Aco_DX8_Texture* texture, bool copyBuffer=false);
	// Checks if there is an alpha texture child change and restores the alpha in the texture
	virtual void			CheckAplhaRefresh();
	// Textures can be used that are not displayed in the Quest3D surface editor
	// texture that are used internaly return false
	virtual bool			GetIfPublicTexture();
	// Textures can be used that are not displayed in the Quest3D surface editor
	// texture that are used internaly are false
	virtual void			SetIfPublicTexture(bool value);
	// This flag makes sure the texture format tried is D3DFMT_A8R8G8B8
	virtual void			SetForceARGB(bool newValue);
	// Get the directx texture pointer if texture is not yet ready initialisation will be done
	virtual IDirect3DTexture9* GetTexture();

	// Normal map creation when restoring texture (bumpmap)
	virtual void			SetIsNormalMap(bool useNormalMap);
	// Normal map creation when restoring texture (bumpmap)
	virtual bool			GetIsNormalMap();
	// Normal map creation type when restoring texture (bumpmap)
	// 0 is for enviromental bumpmapping
	// 1 is for pixel shader bumpmapping
	virtual void			SetNormalMapType(int normalType);
	// Normal map creation type when restoring texture (bumpmap)
	// 0 is for enviromental bumpmapping
	// 1 is for pixel shader bumpmapping
	virtual int				GetNormalMapType();
	// CreateNormalMap internal function that creates the normal map from original texture
	virtual void			CreateNormalMap();

	// When a texture is loaded from file or buffer this is the buffer used 
	// if you save this buffer to disk you get the original file back (like a JPG)
	virtual char*			GetTextureBuffer();
	// When a texture is loaded from file or buffer this is the buffer size used 
	// if you save this buffer to disk you get the original file back (like a JPG)
	virtual int				GetBufferSize();

	// DoDependencyInit publish function to make sure all channels types need for this
	// channel are used (internal texture manager)
	virtual void			DoDependencyInit(A3d_List* currentDependList);

	// Set compression of texture when created will try to create a compressed texture
	// if the hardware allows it
	virtual void			SetUseTextureCompression(bool newValue);
	// Set compression of texture when created will try to create a compressed texture
	// if the hardware allows it
	virtual bool			GetUseTextureCompression();

	// Instead of linking an alpha channel this version of the alpha can be loaded as a texture
	// from disk this way the child link can be avoided while providing an easy method to
	// create an alpha texture TGA is still prefered for setup time reasons
	virtual bool			LoadALphaFromFile(char* fileName);
	// Instead of linking an alpha channel this version of the alpha can be loaded as a texture
	// from memory this way the child link can be avoided while providing an easy method to
	// create an alpha texture TGA is still prefered for setup time reasons
	virtual bool			AddAlphaFromInternalBuffer(char* alphaFileBuffer=NULL, int alphaBufferSize=0);
	// Get if we use texture alpha from a buffer
	virtual bool			GetIfUseBufferAlpha();
	// ClearUseBufferAlpha clears the internal buffer used for alpha
	virtual void			ClearUseBufferAlpha();
	// SetUseAlphaOnMipmaps when a texture is created with mipmaps and alpha buffers are used
	// this function will make sure the alpha is created on all mipmap levels
	virtual void			SetUseAlphaOnMipmap(bool newValue);
	// GetUseAlphaOnMipmap get if we use alpha on mipmap creation
	virtual bool			GetUseAlphaOnMipmap();
	// CheckBufferUpdate
	virtual void			CheckBufferUpdate();
	// D3DXIMAGE_INFO
	virtual D3DXIMAGE_INFO	GetOriginalImageInfo();
	// reload texture
	virtual void			ReloadTexture();

protected:
	// which texture are we in the texture manager
	int						referenceNr_;
	// or we are a texture ourselves
	IDirect3DTexture9*		texture_;
	// or we are a texture ourselves
	IDirect3DTexture9*		bumpMapTexture_;
	// pointer back to direct graphics unique channel
	Aco_DX8_DirectGraphicsChannel*		directGraphics_;
	// texture manager
	Aco_DX8_TextureManager*	textureManager_;
	// buffer 
	char*					fileBuffer_;
	// buffer siz
	int						bufferSize_;
	// buffer 
	char*					alphaFileBuffer_;
	// buffer siz
	int						alphaBufferSize_;
	// how many mipmap levels do we want
	int						mipMapLvL_;
	// we can create a tumbnail from ourselves
	Aco_DX8_Texture*		tumbNailTexture_;
	// desired texture height
	int						desiredTextureHeight_;
	// desired texture width
	int						desiredTextureWidth_;
	// are we a public texture!
	bool					publicTexture_;
	// if we get an alpha we reload the texture
	bool					newAlpha_;
	// if we get an alpha we reload the texture
	bool					forceARGBTexture_;
	// we are a normal map
	int						normalMapType_;
	// are we a normal map
	bool					normalMap_;
	// use conpression
	bool					useCompression_;
	// bufferTimeStamp_
	DWORD					bufferTimeStamp_;
	// use alpha on mipmap
	bool					useAlphaOnMipMap_;
	// refresh alpha timestamp
	DWORD					refreshAlphaTime_;
	// error loading object
	bool					errorLoadingTexture_;
	// D3DXIMAGE_INFO info;
	D3DXIMAGE_INFO			originalFileInfo_;
	// texture file name
	char					originalTextureFileName_[262];
	// 	forceManaged_ = false;
	bool					forceManaged_;
};

#define DX8_TEXTUREDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_DX8_Texture; \
} \
}

#endif // !defined(AFX_DX8_TEXTURE_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
