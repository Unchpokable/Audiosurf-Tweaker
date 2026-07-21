#if !defined(AFX_TRACKER_DRIVER_H__91957315_8497_472F_9D19_EFAD99B2A2DA__INCLUDED_)
#define AFX_TRACKER_DRIVER_H__91957315_8497_472F_9D19_EFAD99B2A2DA__INCLUDED_

#ifdef TRACKER_DRIVER_EXPORTS
#define TRACKER_DRIVER_API __declspec(dllexport)
#else
#define TRACKER_DRIVER_API __declspec(dllimport)
#endif

#define TRACKER_DRIVER_CHANNEL_NAME		"Tracker Driver"
#define TRACKER_DRIVER_CHANNEL_VERSION	1				

// {D8EF64C0-7599-4F74-BBD5-0097CCA7AE3C} 
static const GUID TRACKER_DRIVER_CHANNEL_GUID = { 0xd8ef64c0, 0x7599, 0x4f74, { 0xbb, 0xd5, 0x0, 0x97, 0xcc, 0xa7, 0xae, 0x3c } };

struct D3DXVECTOR3;

class TRACKER_DRIVER_API Aco_Tracker_Driver : public A3d_Channel  
{
public:
	Aco_Tracker_Driver();
	virtual ~Aco_Tracker_Driver();

	// CallChannel overloaded function
	virtual void		CallChannel();

	// Try to connect to the tracker 
	virtual bool		Connect();

	// Disconnect the tracker
	virtual bool		Disconnect();

	// Retrieve the number of trackers
	virtual int			GetTrackerCount();

	// Retrieve a position vector
	virtual D3DXVECTOR3	GetPositionVector(int trackerNum);

	// Retrieve the rotation vector
	virtual D3DXVECTOR3	GetRotationVector(int trackerNum);

	// Retrieve the connection station of the tracker
	virtual bool		GetConnection();

protected:

	// Should be set to true when the tracker is connected and initialized
	bool				connected_;

};

// Leave rest of header file intact!
#define TRACKER_DRIVERDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_Tracker_Driver; \
} \
}

#endif // !defined(AFX_TRACKER_DRIVER_H__91957315_8497_472F_9D19_EFAD99B2A2DA__INCLUDED_)
