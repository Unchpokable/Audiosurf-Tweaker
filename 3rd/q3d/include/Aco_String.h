#if !defined(AFX_STRING_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
#define AFX_STRING_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_

#ifdef STRING_EXPORTS
#define STRING_API __declspec(dllexport)
#else
#define STRING_API __declspec(dllimport)
#endif

#define		STRING_NAME			"Text"
#define		STRING_VERSION		1
// {6E6FB247-4627-4fbe-8973-48344F23881E}
static const GUID STRING_GUID = { 0x6e6fb247, 0x4627, 0x4fbe, { 0x89, 0x73, 0x48, 0x34, 0x4f, 0x23, 0x88, 0x1e } };

class A3d_String;

class STRING_API Aco_StringChannel : public A3d_Channel  
{
public:
	Aco_StringChannel();
	virtual ~Aco_StringChannel();

	virtual const char*	GetString();
	virtual void		SetString(const char* newString);

	// Loading and saving the channel value
	virtual bool		SaveChannel(A3dFileSaver& saver);
	virtual bool		LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);

	// SetSingeLine
	virtual void		SetSingeLine(bool newValue);
	// GetSingeLine
	virtual bool		GetSingeLine();
	// SetMaxCars
	virtual void		SetMaxCars(int max);
	// GetTimeStamp
	virtual DWORD		GetTimeStamp();
	// uses WCHAR
	virtual void		SetIfUseWChar(bool newValue);
	// uses WCHAR
	virtual bool		GetIfUseWChar();
	// GetWString
	virtual const WCHAR*	GetWString();
	// SetWString
	virtual void			SetWString(const WCHAR* newString);
	// AS2WS
	virtual void			AS2WS( WCHAR* wstrDestination, const CHAR* strSource);
	// WS2AS
	virtual void			WS2AS( char* tstrDestination, const WCHAR* wstrSource);
	// GetLastRect
	virtual RECT			GetLastRect();
	// SetLastRect
	virtual void			SetLastRect(RECT);

protected:
	// This is our string to remember
	A3d_String*			channelString_;
	// do we want to be single line
	bool				singleLine_;
	// wchar
	WCHAR				*wideString_;
	// use wchar
	bool				useWchar_;
	// noWUpdate_
	bool				WUpdate_;
	bool				AUpdate_;
	// last rect
	RECT				lastRect_;
};

#define STRINGDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_StringChannel; \
} \
}  

#endif // !defined(AFX_STRING_H__7B5A0C82_7D5D_4E4E_831E_8E595B582DEA__INCLUDED_)
