#if !defined(AFX_GLOVE_DRIVER_H__1E770B0E_0D23_4FB2_B6B2_421524591AC4__INCLUDED_)
#define AFX_GLOVE_DRIVER_H__1E770B0E_0D23_4FB2_B6B2_421524591AC4__INCLUDED_

#ifdef GLOVE_DRIVER_EXPORTS
#define GLOVE_DRIVER_API __declspec(dllexport)
#else
#define GLOVE_DRIVER_API __declspec(dllimport)
#endif

#define GLOVE_DRIVER_UNKNOWN_HANDED		0
#define GLOVE_DRIVER_RIGHT_HANDED		1
#define GLOVE_DRIVER_LEFT_HANDED		2

#define GLOVE_DRIVER_CHANNEL_NAME		"Glove Driver"
#define GLOVE_DRIVER_CHANNEL_VERSION	1				

// {A71DF48A-13BC-44C6-ABD4-8AAF0F8A1638} 
static const GUID GLOVE_DRIVER_CHANNEL_GUID = { 0xa71df48a, 0x13bc, 0x44c6, { 0xab, 0xd4, 0x8a, 0xaf, 0xf, 0x8a, 0x16, 0x38 } };

class GLOVE_DRIVER_API Aco_Glove_Driver : public A3d_Channel  
{
public:
	Aco_Glove_Driver();
	virtual ~Aco_Glove_Driver();

	// Get the number of connection possibilities
	// Usually return 1 for a single glove system
	// For overloading, will return false if not overloaded
	virtual int				GetConnectionCount();

	// Get the names of the connection possibilities
	// For overloading, will return false if not overloaded
	virtual char*			GetConnectionName(int connectionNumber);

	// Set the connection number, used different for each driver, normally port number
	// For overloading, will return false if not overloaded
	virtual bool			SetConnection(int connectionNumber);

	// Get the currently selected connection number
	// For overloading, will return -1 if not overloaded
	virtual int				GetConnnection();

	// Connect the glove using the settings (saved or configured)
	// This function is called when the glove source is called
	// For overloading, will return false if not overloaded
	// SetConnection needs to be used first
	virtual bool			Connect();

	// Disconnect the glove, connection should be possible again
	// For overloading, will return false if not overloaded
	// SetConnection needs to be used first
	virtual bool			Disconnect();

	// Get the handedness of the glove (right or left)
	// For overloading, will return unknown if not overloaded
	virtual int				GetHandedness();

	// Retrieve the number of sensors
	// For overloading, will return zero is not overloaded
	// SetConnection needs to be used first
	virtual int				GetSensorCount();

	// Retrieve the name of the sensor
	// For overloading, will return NULL if not overloaded
	// SetConnection needs to be used first
	virtual char*			GetSensorName(int sensorNumber);

	// Retrieve the name of the device
	// For overloading, will return nothing if not overloaded
	// SetConnection needs to be used first
	virtual char*			GetGloveName();

	// Retrieve the value of a sensor
	// For overloading, will return zero if not overloaded
	// SetConnection needs to be used first
	virtual float			GetSensorValue(int sensorNr);

	// Retrieve the connection status
	// SetConnection needs to be used first
	// Can be overloaded
	virtual bool			GetConnectionStatus();

	// Retrieve the debug status
	// Will return the bool value of the protected member
	// Does not need to be overloaded
	virtual bool			GetDebug();

	// Change the debug statis
	// Will change the bool value of the protected member
	// Does not need to be overloaded
	virtual void			SetDebug(bool newValue);

protected:

	// Connection status
	bool					connected_;
	// Connection
	int						connection_;
	// Debug flag, is turned on/off in source
	// member can be used directly or through the GetDebug() function
	// This is not and should not be saved
	bool					debug_;

};

// Leave rest of header file intact!
#define GLOVE_DRIVERDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_Glove_Driver; \
} \
}

#endif // !defined(AFX_GLOVE_DRIVER_H__1E770B0E_0D23_4FB2_B6B2_421524591AC4__INCLUDED_)
