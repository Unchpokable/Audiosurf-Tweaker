#if !defined(AFX_MATRIX_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_MATRIX_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

#ifdef MATRIX_EXPORTS
#define MATRIX_API __declspec(dllexport)
#else
#define MATRIX_API __declspec(dllimport)
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9math.h>

#define		MATRIX_CHANNEL_NAME			"Matrix"
#define		MATRIX_CHANNEL_VERSION		1
// {2F605354-314D-4775-86E4-1F733550B227}
static const GUID MATRIX_CHANNEL_GUID = { 0x2f605354, 0x314d, 0x4775, { 0x86, 0xe4, 0x1f, 0x73, 0x35, 0x50, 0xb2, 0x27 } };

const float pi = 3.141592654f;

class Aco_DX8_DirectGraphicsChannel;

class MATRIX_API Aco_MatrixChannel : public A3d_Channel  
{
public:
	Aco_MatrixChannel();
	virtual ~Aco_MatrixChannel();

	// if you call a matrix then it will set itself on the world matrix for dx8
	virtual void		CallChannel();
	// a channel must save itself with the filesaver class
	virtual	bool	    SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool		LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);

	// get old matrix 
	virtual D3DXMATRIX	GetOldMatrix();
	// Function to get the Matrix out of the channel
	virtual D3DXMATRIX	GetMatrix();
	// temp
	virtual void		SetMatrix(D3DXMATRIX matrix);

	// Let this channel add it's dependecies to the given list
	virtual void		DoDependencyInit(A3d_List* currentDependList);

protected:
	// pointer back to direct graphics unique channel
	Aco_DX8_DirectGraphicsChannel*		directGraphics_;
	// Lets create the primary variable to hold te matrix
	D3DXMATRIX*			matrix_;
};

#define MATRIXDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_MatrixChannel; \
} \
}

#endif // !defined(AFX_MATRIX_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
