#if !defined(AFX_SERIALPLUGIN_H__5D761630_5F27_46A6_9F36_08B461BE61C6__INCLUDED_)
#define AFX_SERIALPLUGIN_H__5D761630_5F27_46A6_9F36_08B461BE61C6__INCLUDED_

#ifdef SERIALPLUGIN_EXPORTS
#define SERIALPLUGIN_API __declspec(dllexport)
#else
#define SERIALPLUGIN_API __declspec(dllimport)
#endif

#define SERIALPLUGIN_CHANNEL_NAME		"SerialPlugin"
#define SERIALPLUGIN_CHANNEL_VERSION	1				

// {ACEDD856-2521-4F91-B470-41F3D3A4F54D} 
static const GUID SERIALPLUGIN_CHANNEL_GUID = { 0xacedd856, 0x2521, 0x4f91, { 0xb4, 0x70, 0x41, 0xf3, 0xd3, 0xa4, 0xf5, 0x4d } };

struct SerialPluginDataPackage {
	char*		data;
	int			dataLength;
};

class SERIALPLUGIN_API Aco_SerialPlugin : public A3d_Channel  
{
public:
	Aco_SerialPlugin();
	virtual ~Aco_SerialPlugin();

	// Check the in/output
	virtual SerialPluginDataPackage*	TranslateInput(SerialPluginDataPackage* dataPackage);
	virtual SerialPluginDataPackage*	TranslateOutput(SerialPluginDataPackage* dataPackage);

protected:

	// Our local data package
	SerialPluginDataPackage				inputPackage_;
	SerialPluginDataPackage				outputPackage_;

};

// Leave rest of header file intact!
#define SERIALPLUGINDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_SerialPlugin; \
} \
}

#endif // !defined(AFX_SERIALPLUGIN_H__5D761630_5F27_46A6_9F36_08B461BE61C6__INCLUDED_)
