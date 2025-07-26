
#if !defined(AFX_OBJECTDATA_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_OBJECTDATA_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

#ifdef OBJECTDATA_EXPORTS
#define OBJECTDATA_API __declspec(dllexport)
#else
#define OBJECTDATA_API __declspec(dllimport)
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9math.h>

//disable nasty warning about DLL interface
#pragma warning( disable : 4251)  

#define		OBJECTDATA_CHANNEL_NAME			"3D ObjectData"
#define		OBJECTDATA_CHANNEL_VERSION		1
// {21A8923D-B908-4104-AE88-B6718D8A8678}
static const GUID OBJECTDATA_CHANNEL_GUID = { 0x21a8923d, 0xb908, 0x4104, { 0xae, 0x88, 0xb6, 0x71, 0x8d, 0x8a, 0x86, 0x78 } };

class Sphere;

struct VertTUVOffset {
	VertTUVOffset() {
		tu=0;
		tv=0;
		extraOffsetX = 0;
		extraOffsetY = 0;
		scallingsetX = 1;
		scallingsetY = 1;
	}
	VertTUVOffset(float tu_, float tv_) {
		tu=tu_;
		tv=tv_;
	}

	float   tu;
	float   tv;
	float   extraOffsetX;
	float   extraOffsetY;

	float   scallingsetX;
	float   scallingsetY;
};

class Aco_DX8_D3DDeviceUse;

struct BoundingBox {
	// minbox
	D3DXVECTOR3		minBox;
	// maxbox
	D3DXVECTOR3		maxBox;
};

struct CustomDataChannel {
	// we have this channel for ourselves! we also delete it!
	A3d_Channel* channel;
};

class OBJECTDATA_API Aco_DX8_ObjectDataChannel : public Aco_DX8_D3DDeviceUse  
{
public:
	Aco_DX8_ObjectDataChannel();
	virtual ~Aco_DX8_ObjectDataChannel();

	// release all data
	virtual void		Release();
	// release all data
	virtual void		ReleaseVertexData();
	// we can also call this channel so that we can stream itself by a queue :)
	virtual void		CallChannel();
	// other channels can call this to make sure we are updated without rendering
	virtual void		UpdateData();
	// a channel must save itself with the filesaver class
	virtual	bool	    SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool		LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);

	// invalidate data 
	virtual void		InvalidateDeviceObjects();

	// stream our vertex data
	virtual void		StreamVertexData();
	virtual void		StreamTextureMatrices();

	// acces to the vertex and poly data etc
	// create a vertex buffer from the current information
	virtual bool		CreateVertexBuffer();
	// set the vertex count
	virtual void		SetVertexCount(int newCount);
	// GetVertexCount
	virtual int			GetVertexCount();

	// set the index count
	virtual void		SetIndexCount(int newCount);
	// get the index count
	virtual int			GetIndexCount();

	// input: array of vertex positions with lenght of vertexcount
	virtual bool		SetIndex(DWORD indexes, int indexNr);
	// set and get the vertex info
	virtual DWORD		GetIndexes(DWORD nr);

	// SetVertexPosition
	virtual bool		SetVertexPosition(D3DXVECTOR3 vertPos, int vertexNr); 
	// GetVertexPositionDx
	virtual D3DXVECTOR3	GetVertexPosition(DWORD nr);

	// we need the D3DXVECTOR3 version which is handy to use
	virtual bool		SetVertexNormal(D3DXVECTOR3 VertNormal, int vertexNr);
	// set and get the vertex info
	virtual D3DXVECTOR3	GetVertexNormals(DWORD nr);

	// set and get the vertex info
	// input: array of vertex tus with lenght of vertexcount
	virtual bool		SetVertexTUCoord(int textureLvl, D3DXVECTOR2 vertTUV, int vertexNr);
	// set and get the vertex info
	virtual D3DXVECTOR2	GetVertexTUV(int textureLvl, DWORD nr);

	// input: array of vertex tus with lenght of vertexcount
	virtual bool		SetVertexColor(DWORD vertColor, int vertexNr);
	// set and get the vertex info
	virtual DWORD		GetVertexColor(DWORD nr);

	// set the polygon type triangles lines or points
	virtual void		SetPolygonType(int type);
	// get the polygon type triangles lines or points
	virtual int			GetPolygonType();

	// we can use this function to change the UV coords
	virtual void		MapTexture(int level, int mode, bool resetOffSet=true);
	// we can use this function to change the UV coords
	virtual int			GetTextureMapping(int level);
	// MakeSphereTexture
	virtual void		MakeSphereTexture(int level);
	// MakeCylinderTexture
	virtual void		MakeCylinderTexture(int level);
	// CubicTexture
	virtual void		CubicTexture(int level, bool planarMap=false);
	// PlanarTexture
	virtual void		PlanarTexture(int level, int direction);
	// MakeObjectSizeBoundrys
	virtual void		MakeObjectSizeBoundrys(D3DXVECTOR3 &minVals, D3DXVECTOR3 &maxVals);

	// get info
	// do we use the vertex positions in the vertexbuffer creation
	virtual bool		GetVertexPositionUse();  
	// do we use the vertex positions in the vertexbuffer creation
	virtual bool		GetVertexNormalUse();
	// do we use the vertex positions in the vertexbuffer creation
	virtual bool		GetVertexTUUse(int level); 
	// do we use the vertex positions in the vertexbuffer creation
	virtual bool		GetVertexColorUse(); 

	// get FVF
	virtual DWORD		GetFVFlags();
	// GetVertexSize
	virtual DWORD		GetVertexSize();

	// normalize UV
	virtual void		NormalizeTexture(int level);
	// RemapTexture
	virtual void		RemapTexture();
	// calculate texture coords
	virtual void		CalculateTextureCoords(int level);
	// get VertTUVOffset
	virtual VertTUVOffset GetTUVOffSet(int level);
	// get VertTUVOffset
	virtual void		SetTUVOffSet(int level, VertTUVOffset vertOffset);

	// bounding box
	virtual D3DXVECTOR3 GetBoundingBoxMin();
	// bounding box
	virtual D3DXVECTOR3 GetBoundingBoxMax();
	// CalculateBoundingBox
	virtual void		CalculateBoundingBox();
	// GetIfObjectIsVisible
	virtual bool		GetIfObjectIsVisible();
	// very handy to get the bounding box of this object!
	virtual BoundingBox	GetObjectTransformedMinMaxBox();
	// very handy to get the bounding box of this object!
	virtual BoundingBox	GetObjectTransformedMinMaxBox(D3DXMATRIX worldMatrix);

	// support for streaming
	virtual bool		SetDataToStream(int streamNr);
	
	// the render states that need to be set by derived versions of this class
	virtual void		DoCustomPreRenderStates();	

	// get the channel depenency
	virtual void		DoDependencyInit(A3d_List* currentDependList);

	// what is the world matrix our object used
	virtual void		SetWorldMatrix(D3DXMATRIX matrix);
	// GetWorldMatrix
	virtual D3DXMATRIX	GetWorldMatrix();	
	// set texture transform mode
	virtual void		SetTextureTransform(int textureStage, int type);
	// get the polygon type triangles lines or points
	virtual int			GetTextureTransform(int textureStage);
	// render buffer for vertex shader
	virtual void		RenderForShader();
	// add channel that creates custom data
	virtual void		AddCustomDataChannel(A3d_Channel* customData);
	//GetCustomDataChannel
	virtual A3d_Channel* GetCustomDataChannel(int nr);
	//GetCustomDataChannel
	virtual int			GetCustomDataChannelCount();
	// get vertexBuffer_
	virtual LPDIRECT3DVERTEXBUFFER9 GetVertexBuffer();
	// get vertexBuffer_
	virtual LPDIRECT3DINDEXBUFFER9 GetIndexBuffer();
	// what is the world matrix our object used
	virtual void		SetDisableObjectClipping(bool newValue);

protected:
	// pointer to our vertex buffer
	LPDIRECT3DVERTEXBUFFER9			vertexBuffer_;
	// pointer to the index buffer
	LPDIRECT3DINDEXBUFFER9			indexBuffer_;
	// buffer for our polygons
	WORD*							polygon_;
	// buffer for our polygons
	DWORD*							polygon32_;
	// poly count
	DWORD							polyCount_;
	// vertex count
	DWORD							vertexCount_;
	// from different vertex information we can build different vertex buffers
	D3DXVECTOR3						*vertPos_;
	// normal of the vertex
	D3DXVECTOR3						*vertNormal_;
	// first texture set of the vertex
	D3DXVECTOR2						*vertTUV_[3];
	// color
	DWORD							*vertColor_;
	// remember the size of the vertex buffer
	int								vertexSize_;
	// the vertex FVF flags
	DWORD							vertexBufferFlags_;
	// did we initialize the buffers
	bool							buffersReady_;
	// polygon type
	int								polyType_;
	// we remember uv offsets
	VertTUVOffset					*uvOffset_;
	// the way we applied the texture
	int								textureMappingMode_[3];
	// bounding box
	D3DXVECTOR3						boundingBoxMin_;
	// bounding box
	D3DXVECTOR3						boundingBoxMax_;
	// timestamp of creation of vertex data
	bool							validBoundingBox_;
	// old worlmatrix
	D3DXMATRIX						oldWorldMatrix_;
	// current world matrix
	D3DXMATRIX						worldMatrix_;
	// first time old world check
	bool							firstOldMatrixCheck_;
	// bounding box
	D3DXVECTOR3						transformedBoundingBoxMin_;
	// bounding box
	D3DXVECTOR3						transformedBoundingBoxMax_;
	// which buffer format do we use
	DWORD							bufferFormat_;
	// did we change the texture matrixes?
	bool							textureMatrixesChanged_;
	// matrix set
	bool							matrixIsSet_;
	// texture transform
	int								textureTransformMode_[3];
	//customDataChannelList_
	A3d_List						customDataChannelList_;
	// DisableObjectClipping
	bool							disableObjectClipping_;
};

#define OBJECTDATA_DLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_DX8_ObjectDataChannel; \
} \
}  

#endif // !defined(AFX_OBJECTDATA_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
