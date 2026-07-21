#if !defined(AFX_FLOAT_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_FLOAT_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the FLOAT_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// FLOAT_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef FLOAT_EXPORTS
#define FLOAT_API __declspec(dllexport)
#else
#define FLOAT_API __declspec(dllimport)
#endif

#define		FLOAT_CHANNEL_NAME			"Value"
#define		FLOAT_CHANNEL_VERSION		1
// {BE69CCC4-CFC1-4362-AC81-767D199BBFC3}
static const GUID FLOAT_CHANNEL_GUID = { 0xbe69ccc4, 0xcfc1, 0x4362, { 0xac, 0x81, 0x76, 0x7d, 0x19, 0x9b, 0xbf, 0xc3 } };

class FLOAT_API Aco_FloatChannel : public A3d_Channel  
{
public:
	Aco_FloatChannel();
	virtual ~Aco_FloatChannel();

	// update on callchannel
	virtual void		CallChannel();
	// function to get the float value
	virtual float		GetFloat();
	// function to get the float value
	virtual float		GetOldFloat();
	// and set it
	virtual void		SetFloat(float newValue);

	// Retrieve Our default float
	virtual float		GetDefaultFloat();
	// Change our deafult float
	virtual void		SetDefaultFloat(float newValue);
	// Retrieve our default float setting
	virtual bool		GetDefaultFloatSetting();
	// Change our default float setting
	virtual void		SetDefaultFloatSetting(bool newValue);

	// a channel must save itself with the filesaver class
	virtual	bool	    SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool		LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);
	// GetFeedbackClass
	virtual ChannelFeedback* GetFeedbackClass();

protected:
	// this is the value that this channel type can remember
	float				channelFloat_;
	// Does the value have a default setting
	bool				default_;
	// The actual default value
	float				defaultFloat_;
	// channelFeedback_
	ChannelFeedback*    channelFeedback_;
};

#define DLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_FloatChannel; \
} \
} 

#endif // !defined(AFX_FLOAT_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
