//////////////////////////////////////////////////////////////////////
// A3d_EngineInterface.h - Interface for Quest3D Channel Engine
//
// Version: 3.5.00
//
// (c) 2001-2006 Act-3D B.V.
// ALL RIGHTS RESERVED
// 
// This header files falls under the Quest3D SDK License Agreement
// and is only to be distributed with the Quest3D SDK.
//
// For more information and documentation please visit our website:
// http://www.quest3d.com/
//
// DO NOT ALTER THIS FILE, QUEST3D WILL FAIL WHEN CHANGED
//

#if !defined(AFX_ENGINEINTERFACE_H__07178C61_2D7E_11D4_A351_0050DAD61B65__INCLUDED_)
#define AFX_ENGINEINTERFACE_H__07178C61_2D7E_11D4_A351_0050DAD61B65__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// Needed header files
#include <mmsystem.h>
#include "enginelistener.h"

// DLL Exports
#ifdef DLLHIGHPOLY_EXPORTS
#define DLLHIGHPOLY_API __declspec(dllexport)
#else
#define DLLHIGHPOLY_API __declspec(dllimport)
#endif

// we define a global FLOAT_EQ define your own EPSILON for example 0.001
#define FLOAT_ISEQUAL(x,v) ((x==v)||(((v - EPSILON) < x) && (x <( v + EPSILON))))

// Class Prototyping
class Engine;
class A3d_ChannelGroup;
class A3d_Channel;
class Aco_FileLoaderChannel;
class Aco_FileSaverChannel;
class ChannelList;

//////////////////////////////////////////////////////////////////////
// ShowDebugMessage / ShowErrorMessage / ShowInfoMessage / ShowMessageBox / ShowDebug
//
// This is class is used by the EngineInterface to display all kinds
// debug information.
//
class DLLHIGHPOLY_API ShowDebugMessage
{
public:
	ShowDebugMessage();
	virtual ~ShowDebugMessage();

	void operator()(const char *);
	void operator()(const char *, const char *);
	void operator()(const char *, int);
	void operator()(const char *, float);
	void operator()(int);
	void operator()(DWORD);
	void operator()(float);
	void operator()(double);
	void operator()(GUID guid);

	// Show the message
	virtual void				ShowMessage(const char*);

	// pointer to our extended intern only interface
	EngineInterface*			engine;
};

class DLLHIGHPOLY_API ShowErrorMessage : public ShowDebugMessage {
public:
	// show message as error message
	virtual void				ShowMessage(const char*);
};

class DLLHIGHPOLY_API ShowInfoMessage : public ShowDebugMessage {
public:
	// show message as error message
	virtual void				ShowMessage(const char*);
};

class DLLHIGHPOLY_API ShowMessageBox : public ShowDebugMessage {
public:
	// show message as error message
	virtual void				ShowMessage(const char*);
};

// User message: Info
class DLLHIGHPOLY_API ShowUserInfoMessage : public ShowDebugMessage {
public:
	// show message as error message
	virtual void				ShowMessage(const char*);
};

// User message: Warning
class DLLHIGHPOLY_API ShowUserWarningMessage : public ShowDebugMessage {
public:
	// show message as error message
	virtual void				ShowMessage(const char*);
};

// User message: Error
class DLLHIGHPOLY_API ShowUserErrorMessage : public ShowDebugMessage {
public:
	// show message as error message
	virtual void				ShowMessage(const char*);
};

// debug msg
class DLLHIGHPOLY_API ShowDebug {
public:
	// use this function to show any information on your screen
	void operator()(const char *, int nr=1);
	void operator()(const char *, float ,int nr=1);
	void operator()(const char *, int ,int nr);
	void operator()(const char *, const char* ,int nr);
	void operator()(int, int nr=1);
	void operator()(unsigned int, int nr=1);
	void operator()(float, int nr=1);
	void operator()(double, int nr=1);	

	// pointer to our extended intern only interface
	EngineInterface*			engine;
};

// events
#define			EV_INCREASEENGINETICKCOUNT			0
#define			EV_WINDOWMESSAGE					1
#define			EV_STARTENGINEFRAME					2
#define			EV_ENDENGINEFRAME					3
#define			EV_REMOVINGPROJECTFILES				4

//////////////////////////////////////////////////////////////////////
// EngineInterface
//
// This class is created at Quest3D initialization and will control
// the flow of the channels. 
//
// Please note that "Engine" has nothing to do with a "3D Engine"
//
// ALL THIS DOCUMENTATION IS STILL UNDER CONSTRUCTION
//

class A3d_Tread;
class EngineInterfaceExt;
class EngineListener;

class DLLHIGHPOLY_API EngineInterface
{
public:
	EngineInterface();
	virtual ~EngineInterface();

	// Get the installation directory of the engine (location of the loaded highpoly.dll)
	// - Returns:			Pointer to a string containing the installation directory path
	virtual const char*			GetInstallDir();

	// - Returns:			the className of this engine
	virtual const char*			GetEngineClassName();

	// - Returns:			Returns the HINSTANCE of this engine
	virtual HINSTANCE			GetEngineInstance();

	// - Returns:			Returns the HWND of this engine
	virtual HWND				GetWindowHandle();
	
	// Retrieve the current tree count number
	// (This number is increased with every tree count)
	// - Returns:			Current tree count number
	virtual int					GetTreeCalculateCount();

	// DOCUMENTATION UNDER CONSTRUCTION
	// You will probably never need to use this function
	virtual void				SetTreeCalculateCount(int newValue);

	// Creates and returns a new channelgroup
	// * interfaceName		Name of the interface
	// * arrayGroup			If this group holds an array (pool) or is a single group
	// * forcePosition		If you want, you can create a group into a certain position (the loaded groups)
	// - Returns:			Pointer to the created channelgroup class
	virtual A3d_ChannelGroup*	CreateNewGroup(const char* interfaceName, bool arrayGroup=false, int forcePosition=-1);

	// Returns a channelgroup searched by number
	// * nr					Nr in the channelgroup array to return
	// - Returns:			Pointer to the selected channel group, NULL if none found
	virtual A3d_ChannelGroup*	GetChannelGroup(int nr);

	// Returns a channelgroup searched by filename
	// * fileNam			Filename of the loaded channelgroup
	// * instanceNr			Instance number of the group (in case of an array of groups)
	// - Returns:			Pointer to the channelgroup given, NULL if not found
	virtual A3d_ChannelGroup*	GetChannelGroup(const char* fileName, int instanceNr=0);

	// Returns a channelgroup searched by poolname
	// * poolName			Name of the pool of the loaded channelgroup
	// * instanceNr			Instance number in the pool to return (in case of an array)
	// - Returns:			Pointer to the channelgroup with the given pool name, NULL if not found
	virtual A3d_ChannelGroup*	GetPoolGroup(const char* poolName, int instanceNr=0);

	// Load a channelgroup from file
	// * fileName			Filename of the channengroup to load
	// * instanceGroupName	Pool name of the group
	// - Returns:			Pointer to the new channelgroup that was created from file, NULL if failed or not found
	virtual A3d_ChannelGroup*	LoadChannelGroup(const char* fileName, const char* instanceGroupName=NULL);

	// Get the highest instance number from a pool (does not have to 
	// be the number of loaded channel group, there can be empty spots in the list)
	// * instanceGroupName	Pool name to get the highest instance number from
	// - Returns:			Number of the last loaded channel group in an array of channel groups (pool)
	virtual int					GetHighInstance(const char* instanceGroupName);

	// Removes a channelgroup from memory
	// * index				Number of the channelgroup in the array
	virtual void				DeleteChannelGroup(int index);

	// - Returns: the number of channelgroups loaded	
	virtual int					GetChannelGroupCount();

	// Returns an new instance of channel
	// * type/guid			type of channel (GUID should be filled in)
	// * targetGroup		Channelgroup to load the channel into (optional)
	// * createChilderen	Control if you want the channel to autocreate channels (if then channel supports it)
	// - Returns:				Pointer to the just created and initialized channel, NULL if failed or not found
	virtual A3d_Channel*		InitChannelFromType(ChannelType type, A3d_ChannelGroup* targetGroup=NULL, bool createChilderen=false);
	virtual A3d_Channel*		InitChannelFromType(GUID guid, A3d_ChannelGroup* targetGroup=NULL, bool createChilderen=false);

	// Returns a (new) instance of an unique channel. This channel will be loaded into a special
	// memory segment where all given GUID (in combination with name) can only exist once
	// When this function is called for the first time, it will create the channel, and when used
	// for the second time, it will return the previously created channel. This allows you to
	// create "global" channels of any type.
	// * type / guid		Type of channel (guid must be filled in)
	// * uniqueName			The unique name of the loaded channel
	// Returns:				Pointer to the (created) unique channel
	virtual A3d_Channel*		GetUniqueChannel(ChannelType type, const char* uniqueName=NULL);
	virtual A3d_Channel*		GetUniqueChannel(GUID guid, const char* uniqueName=NULL);


	// DOCUMENTATION UNDER CONSTRUCTION
	// link a channel to a certain event. This channel is called when the event takes place
	virtual void				AddChannelToEvent(A3d_Channel* channel, int eventNr); 
	// DOCUMENTATION UNDER CONSTRUCTION
	// Remove a channel that was previously linked to an event
	virtual void				RemoveChannelFromEvent(A3d_Channel* channel, int eventNr); 

	// Are we running the graph?
	virtual bool				GetGraphRunning();

	//////////////////////////////////////////////////////////////////////
	// Debug information help function
	//////////////////////////////////////////////////////////////////////
	// Debug message (blue text in the debug window)
	ShowDebugMessage			DMsg;
	// Error message (red text in the debug window)
	ShowErrorMessage			EMsg;
	// Information Message (black text in the debug window)
	ShowInfoMessage				IMsg;
	// Show a MessageBox pop-up window
	ShowMessageBox				MMsg;
	// Show a user message
	ShowUserInfoMessage			UIMsg;
	// Show a user message
	ShowUserWarningMessage		UWMsg;
	// Show a user message
	ShowUserErrorMessage		UEMsg;
	// Show a line of text on the 3D screen
	ShowDebug					Debug;

	// external interface
	friend class				EngineInterfaceExt;

	EngineInterfaceExt*			engineInterfaceExt_;

	void						AddListener(EngineListener* engineListener);
	void						RemoveListener(EngineListener* engineListener);
	void						AboutToReleaseChannel(A3d_Channel* channel);
	// SetCurrentEvent
	virtual void				SetCurrentEvent(int eventNr, DWORD eventData=NULL);

protected:
	// This is the pointer to the "real" engine (the Quest3D core component)
	Engine*						engine_;
	// listener list
	A3d_List					listenerList_;
};

// Define a globalEngine
extern	DLLHIGHPOLY_API EngineInterface*		globalEngine;
// Define a globalEngine
extern	DLLHIGHPOLY_API HINSTANCE				globalDllInstance;

// These definitions will make life easier for you will not need to use the engine-> every time you want to add a debug message
#define DB				engine->Debug
#define dMsg			engine->DMsg
#define eMsg			engine->EMsg
#define iMsg			engine->IMsg
#define mMsg			engine->MMsg
#define uiMsg			engine->UIMsg
#define uwMsg			engine->UWMsg
#define ueMsg			engine->UEMsg

#define gmMsg			globalEngine->MMsg
#define geMsg			globalEngine->EMsg

//////////////////////////////////////////////////////////////////////
// A3dFileLoader
//
// This is the main loader class of Quest3D. This class will take care
// of correct loading of channelgroups that are saved in trees.
// It is highly recommended to always use this class (that is passed
// to every channel) when saving data in channels.
//
class DLLHIGHPOLY_API A3dFileLoader  {
public:
	A3dFileLoader();
	A3dFileLoader(EngineInterface*);
	virtual ~A3dFileLoader();

	// Load a file into the buffer
	// * fileName				Filename to load
	// - Returns:				True if file could be opened
	bool		LoadFile(const char* fileName);

	// Set the block size for loading
	// * blockSize				Blocksize for loading a file
	void		SetBlockSize(DWORD	blockSize);

	// You can also set a buffer in stead of loading a file and let the class operate on the buffer
	// * buffer					Pointer to the buffer that holds the data
	// * bufferSize				Size of that buffer
	// * manageBuffer			If true, the loader will copy the buffer into it's own and manage that
	void		SetBuffer(char* buffer, int bufferSize, bool manageBuffer=false);

	// - Returns:				Pointer to the buffer containing the data following the last Tag
	const char*	GetTagData();

	// This function will copy the data following the last tag into the given pointer
	// NOTE: Please make sure given pointer will be able to handle to amount of data
	// that will be copied!
	// * void*					Pointer to a variable that is able to hold the information
	void		CopyTagDataToPointer(void*);

	// Peeks at the tag that is currently at the buffer pointer
	// * tag					Tag to compare with the current pointer
	// * changeBufferPos		If true and tag is found, the buffer pointer will be set forward 4 positions
	// - Returns:				True if the header is the same as the given tag.
	bool		CheckHeader(char tag[4], bool changeBufferPos=true);

	// OBSOLETE FUNCTION, DON'T USE
	bool		GotoTag(char tag[4], int count=0);

	// * tag					Tag to check
	// - Returns:				True if the given tag is found at the current buffer postion and forwards 4 chars
	bool		GetInfoIfTag(char tag[4]);

	// * updateBufferPointer	If true, the buffer pointer will be forwarded to after this size indication
	// - Returns:				The size of the datablock after the tag (and this size info) and forwards to the beginning of that data
	DWORD		GetSizeDataBlock(bool updateBufferPointer=true);

	// Reset the buffer pointer to start of the buffer
	void		ResetPosition();

	// Returns:					Pointer to the current position of the buffer the loader is using
	const char*	GetBuffer();

	// - Returns:				The size of the buffer the loader is using
	int			GetBufferSize();

	// Move the buffer pointer forward
	// * addValue				The number of bytes to move the pointer forward
	void		AddBufferPos(int addValue);

	// - Returns:				True if the end of the file has been reached
	bool		GetIfEndOfFile();

	// When a tag is found that is not needed this function skips the tag to the next tag
	// - Returns:				True if the skip was succesfull or no new tag was found
	bool		SkipCurrentTag();

	// Set a URL (for internet loading), you will not need this function, 
	// internet loading is done automatically when using an URL as filename
	// * char					 URL
	void		SetURL(const char*);

	// Returns if the filename is already in the Quest3D Cache directory
	// You will not need to use this functions
	// * fileName				Filename
	// - Returns:				True if the file is already cached
	bool		CheckULRCache(const char* fileName);

	// Puts the filename in the Quest3D cache directory
	// You will not need to use this functions
	// * fileName				Filename
	void		SaveURLToCache(const char* fileName);

	// Change a character in the buffer used by the loader
	// * charNr					which char in the buffer will be changed
	// * newChar				the new char value
	void		SetBufferChar(int charNr, char newChar);
	
	// - Returns: The filename that would be used to save the data from the given url into
	const char*	GetChacheFileNameFromURL(const char* urlFile);	

	// Stop loading, used for quiting multi-threaded loading applications
	void		StopLoading();

	// - Returns: The CRC32 of the current buffer
	int			GetCRC();

	// - Returns: The attributes of the loaded file (0 if nothing is loaded (yet))
	DWORD		GetAttributes();
	
	// if file loader runs in a thread use this function to silence engine acces
	void		RunThreaded(DWORD* progressData);

protected:
	// File loader pointer to channel that is actually used for all loading
	Aco_FileLoaderChannel* fileLoader_;
	// pointer to the core Quest3D component, often called engine
	EngineInterface*	engine;
};

//////////////////////////////////////////////////////////////////////
// A3dFileSaver
//
// This is the main saver class of Quest3D. All saving and loading is
// done through this class. When saving channelgroups each channel in
// the group is saved with this function so you can add your own tags
// and data to be saved in the channelgroup. We recommend you always
// use this class when saving data in channels (see our SDK for more
// information and an example how to do this).
//
class DLLHIGHPOLY_API A3dFileSaver  {
public:
	A3dFileSaver();
	A3dFileSaver(EngineInterface*);
	virtual ~A3dFileSaver();

	// Creates a file to save to
	// * fileName				Filename to save to (can only be disk)
	// * appendFile				If data be appended to the given file set to true
	// - Returns:					True if the file was opened
	bool		OpenSaveFile(const char* fileName, bool appendFile=false);

	// Save a tag (4 characters!)
	// * tag					Pointer to a char[4]
	// - Returns:					True if the tag could be saved
	BOOL		SaveTag(const char* tag);

	// Save the a blocksize (needed before saving data with SaveDataToFile())
	// * size					Size of the coming data block
	// - Returns:				True if the blocksize was saved
	BOOL		SaveBlockSize(DWORD size);

	// Save data (first use SaveBlockSize())
	// * data					Pointer to a the data to save
	// * size					Number of bytes to get from the buffer and save in the file
	// - Returns:				True if the data could be added to the file
	BOOL		SaveDataToFile(const void* data, DWORD size);

	// Close the file that was opened for saving
	void		CloseSaveFile();

	// Returns the filename the saver is currently being used for saving into
	const char*	GetSaveingFileName();

	// Memory functions
	// OpenMemory
	virtual void		OpenMemory();
	// Close the memory
	virtual void		CloseMemory();
	// Retrieve the buffer
	virtual char*		GetBuffer();
	// Retrieve the buffer size
	virtual DWORD		GetBufferSize();


protected:
	// Pointer to a file saver channel that is actually used in this class
	Aco_FileSaverChannel*  fileSaver_;
	// pointer to the core Quest3D component, often called engine
	EngineInterface*	engine;
};

#endif // !defined(AFX_ENGINEINTERFACE_H__07178C61_2D7E_11D4_A351_0050DAD61B65__INCLUDED_)
