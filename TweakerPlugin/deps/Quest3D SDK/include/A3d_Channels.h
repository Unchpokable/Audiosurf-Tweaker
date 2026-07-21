// A3d_Channels.h - Interface for Quest3D Channels base class
//
// Version: 3.00.03
//
// (c) 2001-2005 Act-3D B.V.
// ALL RIGHTS RESERVED
// 
// This header files falls under the Quest3D SDK License Agreement
// and is only to be distributed with the Quest3D SDK.
//
// For more information and documentation please visit our website:
// http://www.quest3d.com/
//
// DO NOT ALTER THIS FILE, CHANNELS WILL FAIL TO WORK WHEN CHANGED
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_A3D_CHANNELS_H__0E5E0941_2B0D_11D4_A351_0050DAD61B65__INCLUDED_)
#define AFX_A3D_CHANNELS_H__0E5E0941_2B0D_11D4_A351_0050DAD61B65__INCLUDED_

#include <strsafe.h>
#include "Act_New.h"

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//disable nasty warning about DLL interface
#pragma warning( disable : 4251)  

#ifdef DLLHIGHPOLY_EXPORTS
#define DLLHIGHPOLY_API __declspec(dllexport)
#else
#define DLLHIGHPOLY_API __declspec(dllimport)
#endif

// Get a channel the normal way
#define			ACO_NORMAL					0
// This channel can be referenced to by another channel grouo
#define			ACO_PUBLIC					1
// This channel is the reference to a channel in another group
#define			ACO_PUBLICCALL				2
// This channel will be the parameter of a public call channel if the public call channel is used
#define			ACO_PARAMETER				3
// This channel will get the real channel in its own way
#define			ACO_CUSTOMCHANNELGET		4
// This channel will get the real channel in its own way
#define			ACO_PUBLICCUSTOMCHANNELGET	5

// 
#define			TREE_NOINCREASE				0
#define			TREE_INCREASEPARAMETER		1
#define			TREE_INCREASEALWAYS			2

#define			Scc							StringCbCopyA				
#define			Sca							StringCbCatA					

// Invalid and normal channel GUID definitions
static const GUID ACO_INVALID_GUID = { 0x0, 0x0, 0x0, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };
static const GUID ACO_CHANNEL_GUID = { 0x11111111, 0x1111, 0x1111, { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11 } };

// Class prototypes
class A3dFileSaver;
class A3dFileLoader;
class A3d_ChannelGroup;
class A3d_Channel;



// You can make the channel hidden by changing the pluginType variable 
// when the channel is initialized into PLUGINTYPE_HIDDEN
#define			PLUGINTYPE_STANDARD			0
#define			PLUGINTYPE_HIDDEN			1
#define			PLUGINTYPE_QUEST3DINTERFACE	2
#define			PLUGINTYPE_SECUTIRYISSUE	4
#define			PLUGINTYPE_REGISTRATION		8

// Edition level definitions
#define			EDITION_LEVEL_ALL			 0		// Lite Edition
#define			EDITION_LEVEL_1				10
#define			EDITION_LEVEL_2				20		// Professional Edition
#define			EDITION_LEVEL_3				30
#define			EDITION_LEVEL_4				40		// Enterprise Edition
#define			EDITION_LEVEL_5				50
#define			EDITION_LEVEL_6				60		// VR Edition

// current engine version
#define		QUEST3D_ENGINEVERSION			61

class DLLHIGHPOLY_API ChannelType {
public:
	ChannelType() {
		pluginType = PLUGINTYPE_STANDARD;

		mversion = 1;	// Major version, DO NOT CHANGE
		miversion = QUEST3D_ENGINEVERSION;	// Minor version, change not needed
		version = 1;	// Increase this number every time the channel is released
		guid=ACO_INVALID_GUID;
		minimumEdition = EDITION_LEVEL_ALL;
	}

	// name of the channel
	char			name[80];
	// GUID of the channel
	GUID			guid;
	// base guid
	GUID			baseguid;
	// major version
	int				mversion;
	// minor version
	int				miversion;
	// build version
	int				version;
	// what type of channel are we
	DWORD			pluginType;
	// registration code
	int				minimumEdition;
};

class DLLHIGHPOLY_API ChannelDependency {
public:
	ChannelDependency();
	~ChannelDependency();

	// GUID of the channel
	GUID			guid;
	// list of depend guids
	A3d_List		dllList;
};

//////////////////////////////////////////////////////////////////////
// DllInterface
//
// All DLLs used by Quest3D are derived from this class, making engine
// integration easy.
//
// Please note that the A3d_Channel class is also derived from this
// class, so all channels also have this interface. You will not need
// to use these functions during normal channel development
//
class EngineInterface;

class DLLHIGHPOLY_API DllInterface {
public:
	DllInterface();
	virtual ~DllInterface();

	// Instance of the loaded dll
	HINSTANCE			dllInstance_;
	// Pointer to the main Quest3D controlling engine
	EngineInterface*	engine;
};

//////////////////////////////////////////////////////////////////////
// ChildCreation
//
// This struct is used when creating child link positions in the channel.
// This struct is filled in the constructor of every channel and can be
// used several times as input for the SetChildCreateType function
//

#define			DIR_DOWN		0
#define			DIR_UP			1
#define			DIR_BOTH		2

struct ChildInfo;

struct ChildCreation {
	ChildCreation() {
		requestLink=-1;
		linked=false;
		channelType.guid = ACO_CHANNEL_GUID;
		channelType.baseguid = ACO_CHANNEL_GUID;
		initialize=false;
		name[0]='\0';
		channelUpgradeGUID=ACO_INVALID_GUID;
		childInfo=NULL;

		dataDirection = DIR_UP;
	}
	// What type should the child be
	ChannelType			channelType;
	// On what place should the link be placed
	int					requestLink;
	// If the link is ready
	bool				linked;
	// When set to true, the child channel will be created together with the parent
	bool				initialize;
	// What name should the channel get
	char				name[100];
	// Place a GUID here that you want to have initialized in stead of the
	// set type in channelType. Make sure the base type of this channel
	// is the same as the channelType.GUID	
	GUID				channelUpgradeGUID;
	// A pointer to the child information we belong to
	ChildInfo*			childInfo;
	// dataDirection
	int					dataDirection;
};

// for finding PublicCall channels (every channel can become one)
struct PublicCallInfo {
	PublicCallInfo() {
		Scc(publicName, 79, "No Name");
		Scc(poolName, 79, "No Pool");
		groupFileName[0] = '\0';
		requestedInstanceNr_=0;
		useAutoLoad_=true;
		useDynamicLoad_=false;
	}

	// what is the name of the public channel we are searching for
	char		publicName[80];
	// we need to remember the channel group poolname
	char		poolName[80];
	// this we only use for autoloading we need the filename of the group!
	char		groupFileName[260];
	// if we are a public caller which instance do we want
	int			requestedInstanceNr_;
	// use auto load
	bool		useAutoLoad_;
	// use dynamic load
	DWORD		useDynamicLoad_;
	// 	static ChildCreation newChild;
	ChildCreation	newChild_;
};

struct ParameterInfo {
	// if we use a Parameter channel which channel nr do we want then :)
	int				parameterInfoNr;
};

struct PublicChannelInterface {
	GUID			targetGroupGuid;
	A3d_List		interfaceItems;
};

struct LinkRequest {
	// which group link do we want?
	int						groupLink;
	// at which position
	int						linkPos;
	// foundRequest
	bool					foundRequest;
};


//////////////////////////////////////////////////////////////////////
// ChildInfo
//
// More information on children linked to the channel
// You will not need to use this struct during normal channel development
//
struct ChildInfo {
	ChildInfo() {
		child=NULL;
		linkPos=-1;
		growLink=false;
		creationNode=false;
		nodeOffset = 0;
		inputIndex=0;
		userData=NULL;
		poolCount=0;
		poolNr = -1;
	}

	// the channel we hold
	A3d_Channel*	child;
	// the position it was linked to
	int				linkPos;
	// this information can also mean we have extra information about this position
	bool			creationNode;
	// are we a childinfo that grows
	bool			growLink;
	// what is the ofset we have inside of the original list
	// for a growing link this is beginning at 0 the ofset for this child 
	// for this link so the second light on a render is 1
	int				nodeOffset;
	// at which index in the input list are we 
	// the inputIndex is remember so the channel knows at which child nr we
	// will insert new childeren
	// the light on a render is 4 for the creation node
	int				inputIndex;
	// this may be very important to create valid child link information etc
	DWORD			userData;
	// if this child is of a pool link lets remember the link count
	int				poolCount;
	// if this child is of a pool link lets remember the nr of the pool this child is
	int				poolNr;
};

//////////////////////////////////////////////////////////////////////
// A3d_Channel
//
// This is the base class of all channels. Most functions are virtual
// so they can be derived or just used in a channel.
//

class ChannelInternal;
class ChannelFeedback;

class DLLHIGHPOLY_API A3d_Channel : public DllInterface
{
public:
	A3d_Channel();
	virtual ~A3d_Channel();

	// friend classes
	friend class				ChannelInternal;

	// This function is called when the channel is called
	// Derive this function to get an entry point in your channel
	virtual void				CallChannel();

	// DOCUMENTATION UNDER CONSTRUCTION	
	// get interface type
	int							GetChannelInterfaceType();

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Child Management Funcions - You probably not need these functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Return a pointer to a channel from the selected child
	// Please do not derive this function, you are likely to run into trouble!
	// - Returns:		Pointer to a channel (can be overloaded to the correct type)
	// * nr				Child number to get pointer from (starting at zero)	
	A3d_Channel*				GetChild(int nr);

	// When multiple types of channels are linked to the channel you can
	// use this function to quickly fetch the correct pointer
	// Please do not derive this function, you are likely to run into trouble!
	// - Returns:			Pointer to a channel (can be overloaded to the correct type)
	// * linkPosition:		What (growing) link position 
	// * childPositionNr:	What child link number from the (growing) linkPosition
	A3d_Channel*				GetChildFromLinkPosition(int linkPosition, int childPositionNr);

	// Return the pointer from a child that searched by GUID
	// Easy to use when multiple types of children are linked, but function is slower
	// Please do not derive this function, you are likely to run into trouble!
	// - Returns:			Pointer to a channel (can be overloaded to the correct type)
	// * channelGUID:		GUID of the channel pointer to retrieve
	// * childTypeNr:		Retrieve the pointer of the n-th child with the selected GUID	
	A3d_Channel*				GetChildFromLinkGuid(GUID channelType, int childTypeNr);

	// Return the number of children that is curently linked to the channel
	// and also count the empty link positions
	// Please do not derive this function, you are likely to run into trouble!
	// - Returns:			The number of link positions on the channel
	int							GetChildCount();

	// DOCUMENTATION UNDER CONSTRUCTION
	// Please do not derive this function, you are likely to run into trouble!
	int							GetChildLinkPositionCount(int linkPosition);

	// Count the number of children linked with the given GUID
	// - Returns:			The number of link positions on the channel
	// * channelGUID:		GUID of the channel type to count
	int							GetChildTypeCount(GUID channelType);

	// Link a new child to this channel
	// Please do not derive this function, you are likely to run into trouble!
	// - Returns:			Child number linked
	// * A3d_Channel:		Pointer to channel to link
	// * targetChannel:		Target (growing) link number
	// * targetChildTypeNr:	Target child number of the (growing) link selected
	int							SetChild(A3d_Channel*, int targetChannel, int targetChildTypeNr);

	// Remove the link to a child from this channel (we are then no longer its parent)
	// Please do not derive this function, you are likely to run into trouble!
	// * nr:		The link to remove
	void						RemoveChild(int nr);

	// Remove all links of children but keeps the child link positions intact
	// Please do not derive this function, you are likely to run into trouble!
	void						RemoveChildren();

	// Set the number of children that is going to be created
	// Currently this is only used to reset children if they are already set
	// Please do not derive this function, you are likely to run into trouble!
	// * nr:				Number of children that is going to be created
	void						SetChildCreationCount(int nr);

	// Create a child link position
	// Please do not derive this function, you are likely to run into trouble!
	// * child:				Struct filled with the child link position information
	// * createNr:			On what child link
	void						SetChildCreateType(ChildCreation child, int createNr);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Channel State Functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Once the Quest3D core engine is initialized, this function is called
	// You can derive from this function to prepare your channel before it
	// is actually called, and after the constructor
	virtual void				OneTimeInitialize();


	// DOCUMENTATION UNDER CONSTRUCTION
	virtual void				DoEvent(int eventId, DWORD eventInfo);

	// Retrieve our channel type struct
	// Please do not derive this function, you are likely to run into trouble!
	// - Returns:		Struct with channel type properties
	virtual ChannelType			GetChannelType();

	// This function is called by Quest3D when the channel group is saved
	// You are able to derive this function to add information in the channel group file
	// that is being saved. Do not forget to call the base channels' save function
	// Please note that this is only only correct way to store data in the channel group!
	// - Returns:		True if no error occurred during save
	// * saver:			Pointer to a A3dFileSaver class that is initialized by Quest3D
	//					and is add data to the file that is being saved						
	virtual	bool				SaveChannel(A3dFileSaver& saver);

	// This function is used to load data that was previously saved in the channel
	// group by SaveChannel. Please note this is the only correct way to load data
	// from the channel group. Derive this function if you have also derived SaveChannel
	// and want to retrieve the saved data. Do not forget to call the base class
	// LoadChannel function.
	// - Returns:		True if load was succesful.
	// * loader:		Loader class that is reading from a file
	// * group:			Pointer to the channel group this is being created
	virtual bool				LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);

	// When a channel is used in a channel group that channel group sets which index is used in the channel
	int							GetChannelIDIndexNr();

	// Return the channel group that this channel is part of
	// - Returns:	A pointer to the channelgroup class this channel is part of
	A3d_ChannelGroup*			GetChannelGroup();

	// Change the name of the channel
	// * newName:		The new name of the channel
	void						SetChannelName(const char* newName);

	// Retrieve the name of the current channel
	// - Returns:		Name of this channel
	const char*					GetChannelName();

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Interface Functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	// DOCUMENTATION UNDER CONSTRUCTION	
	// set interface type
	virtual void				SetChannelInterfaceType(int interfaceType);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Other Channel Functions
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// DOCUMENTATION UNDER CONSTRUCTION	
	// for new channels to define new ways to get the channel
	virtual A3d_Channel*		GetChannelFromChannel(A3d_Channel* sourceChannel=NULL, ChildInfo* childInfo=NULL);

	// Check the last time this channel was time stamped, a channel can be time stamped withe the SetTimeStamp function
	// - Returns:		Timestamp of last time SetTimeStamp was called
	virtual DWORD				GetTimeStamp();
	// Time stamp this channel, this can be read with the GetTimeStamp function
	virtual void				SetTimeStamp();

	// DOCUMENTATION UNDER CONSTRUCTION	
	// DoDependencyInit
	virtual void				DoDependencyInit(A3d_List* currentDependList);
	// DOCUMENTATION UNDER CONSTRUCTION	
	// when the project deletes an index all channels need to check for that id
	virtual void				CheckIndexNrRemoval(int nr);
	// DOCUMENTATION UNDER CONSTRUCTION	
	// add depends 
	void						AddChannelGuidDepend(GUID guid, A3d_List* currentDependList);
	// DOCUMENTATION UNDER CONSTRUCTION	
	// add dlls 
	void						AddDLLDepend(const char* dllName, A3d_List* currentDependList);
	// DOCUMENTATION UNDER CONSTRUCTION	
	// getchild fails if the channel is not here or there is no parameter this forces the channel that points to another to give itself
	virtual  A3d_Channel*		GetInternalChild(int childNr); 
	// Read the child information of the given child number
	// Please do not derive this function, you are likely to run into trouble!
	// - Returns:			Info to struct holding the child information
	// * childNr:			The child number to retrieve the information from
	ChildInfo*					GetChildInfo(int childNr);
	// DOCUMENTATION UNDER CONSTRUCTION	
	// if we delete a public channel instance we check for all public callers
	virtual void				CheckPublicChannelInstanceRemoval(A3d_Channel* publicChannel, int poolNr);
	// AddMetaData
	virtual void				AddMetaData(const char* fileName);
	// Set the channel group this channel is part of
	virtual void				SetChannelGroup(A3d_ChannelGroup*);
	// GetFeedbackClass
	virtual ChannelFeedback*	GetFeedbackClass();

protected:
	// Check if this channel has already been used this tree count
	// If this channel is true this channel is used for the first time this tree count
	// you should then calculate or do what you want to do this frame. If you do not use
	// this function, this might results in an endless loop or, you might do the same
	// calculation more than once in the same tree count, and thus waisting time.
	// - Returns:		False if this channel is already used this tree count, true if not
	bool						CheckRenderCount();

	// Struct with information on our channel type and base type
	ChannelType*				channelTypeP_;

private:

	// DOCUMENTATION UNDER CONSTRUCTION	
	// we want to make sure we don't loop these things so we only calculate them once per tree loop
	int							channelCalculatedAtCount_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// this channel may be used in a project and if so this is its channel id index nr
	int							channelIDIndexNr_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// the interfacetype type of this channel (ACO_NORMAL : ACO_PUBLIC : ACO_PUBLICCALL : ACO_PARAMETER : ACO_CUSTOMCHANNELGET)
	int							interfaceType_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// if it is an public caller channel we also fill this information
	PublicCallInfo*				publicCallerInfo_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// if we are an parameter this is the parameter info
	ParameterInfo*				parameterInfo_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// the channel childeren list
	A3d_List					inputChannelsList_;
	// child creation list
	A3d_List					childCreationList_;
	// when we are loaded we add a request for a channel from our own group
	A3d_List*					linkRequest_;
	// when we point to a public channel and can't find it we add a request for that channel!
	A3d_List*					requestList_;
	// if we are a public caller channel we have a list of instances of that channel type from different groups
	A3d_List*					channelInstances_;

	// Our channel name, set by SetChannelName() and return by GetChannelName()
	char						*channelName_;

	// Pointer to the channel group class that this channel belongs to
	A3d_ChannelGroup*			ourChannelGroup_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// if an interface was created without the group that we point to was found we need to delete the first interface
	bool						noGroupInterfaceCreated_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// this value is used to track channel calling (when a channel uses getchild is more accurate
	int							tickCount_;

	// The actual ignore tree count setting variable
	// Get by GetIgnoreTreeCount() and set by SetIgnoreTreeCount()
	int							ingoreTreeCountState_;

	// The actual time stamp value setting
	DWORD						timeStampValue_;

	// DOCUMENTATION UNDER CONSTRUCTION	
	// dynamic load started
	bool						startedDynamicLoading_;
	// treecount mode
	BYTE						treeCountMode_;
	// recalculate count
	BYTE						recalculateCount_;
	// PublicChannelInterface
	PublicChannelInterface*		publicInterface_;
	// public interface translation list
	A3d_List					*publicTranslationList_;
};

class DLLHIGHPOLY_API GChannel {
public:
	GChannel();

	//SetChannel
	virtual void				SetChannel(A3d_Channel* channel);
	// get channel
	virtual  A3d_Channel*		GetChannel();

protected:
	int					groupId_;
	int					channelId_;
	A3d_Channel*		channel_;
	EngineInterface*	engine;
};

#endif // !defined(AFX_A3D_CHANNELS_H__0E5E0941_2B0D_11D4_A351_0050DAD61B65__INCLUDED_)
