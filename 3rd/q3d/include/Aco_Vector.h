#if !defined(AFX_VECTOR_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_VECTOR_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

#ifdef VECTOR_EXPORTS
#define VECTOR_API __declspec(dllexport)
#else
#define VECTOR_API __declspec(dllimport)
#endif

#define		VECTOR_NAME			"Value Vector"

// {9D045960-EAC2-4c40-9BBF-10F32F7FA305}
static const GUID VECTOR_GUID = { 0x9d045960, 0xeac2, 0x4c40, { 0x9b, 0xbf, 0x10, 0xf3, 0x2f, 0x7f, 0xa3, 0x5 } };

class VECTOR_API Aco_VectorChannel : public A3d_Channel  
{
public:
	Aco_VectorChannel();
	virtual ~Aco_VectorChannel();

	// channel update function
	virtual void		CallChannel();

	// Function to get the value from the channel
	virtual D3DXVECTOR3 GetVector();
	// we can also set the vector !!
	virtual void		SetVector(D3DXVECTOR3);
	// set one of our childeren
	virtual bool		SetFloat(int childId, float newValue);
	// a channel must save itself with the filesaver class
	virtual	bool	    SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool		LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);
	// GetFeedbackClass
	virtual ChannelFeedback* GetFeedbackClass();

protected:
	// Our channel value
	D3DVECTOR			vector_;
	// channelFeedback_
	ChannelFeedback*    channelFeedback_;

};

#define DLL_EXPORTS_VECTOR extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_VectorChannel; \
} \
}

#endif // !defined(AFX_VECTOR_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
