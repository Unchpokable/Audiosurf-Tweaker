// A3d_ChannelGroup.h: interface for the A3d_ChannelGroup class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_A3D_CHANNELGROUP_H__2B9380A1_2CA5_11D4_A351_0050DAD61B65__INCLUDED_)
#define AFX_A3D_CHANNELGROUP_H__2B9380A1_2CA5_11D4_A351_0050DAD61B65__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct ChannelRequestInfo {
	// channel number that is requested
	int						indexFrom;
	// at which link do we want this index
	int						indexAt;
	// instance of channel that requests info
	A3d_Channel*			channel;
	// where did the user link this channel!
	int						userLinkPos;
};

struct ParameterInterfaceInfo {
	// the name of this Parameters
	char			parameterName[40];
	// the type of this Parameters
	GUID			parameterType;
};

class SmartChannel;
class A3d_String;

class DLLHIGHPOLY_API A3d_ChannelGroup
{
public:
	A3d_ChannelGroup();
	A3d_ChannelGroup(EngineInterface*);
	virtual ~A3d_ChannelGroup();

	// release fundtion to delete all channels
	virtual void				Release();
	// I/O we want to be able to save channel groups to disk!!
	virtual bool				SaveChannelGroup(const char* fileName);
	// I/O we want to be able to save channel groups to disk!!
	virtual bool				SaveChannelGroupInMemory(A3dFileSaver &saverGrou);
	// SaveChannelGroup
	virtual bool				SaveChannelGroupAndZip(A3dFileSaver &saverGroup);
	// and load one from disk
	virtual bool				LoadChannelGroupFromFile(const char* fileName, bool loadNewPoolName=true);
	// and load one from buffer
	virtual bool				LoadChannelGroupFromBuffer(const char* fileName, int bufferSize, char* buffer, bool loadNewPoolName=true);
	// load a channel group from a loader
	virtual bool				LoadChannelGroup(A3dFileLoader &loaderGroup, bool loadNewPoolName=true);

	// we have an engine index
	virtual int					GetGroupIndex();
	// set the index only the engine must do this!!!
	virtual void				SetGroupIndex(int index);

	// when creating all channels we may need to relink some sub channels trough requests!
	virtual void				AddChannelRequest(ChannelRequestInfo reqInfo);
	//CheckPublicChannels
	virtual void				CheckPublicChannels();
	// check the whole request list for this channel
	virtual void				CheckRequestListForIndex(int validIndex);

	// add a channel returns the channel position!
	virtual int					AddChannel(A3d_Channel* newChannel);
	// remove a channel
	virtual bool				RemoveChannel(int channel);
	// remove a channel
	virtual bool				RemoveChannel(A3d_Channel* channel);
	// we can also transfer channels between groups :)
	virtual int					TransferChannel(int nr, A3d_ChannelGroup* newgroup);

	// get the current channel count
	virtual int					GetChannelCount();

	// get a channel
	virtual A3d_Channel*		GetChannel(int channelIndex);
	// we can also get a channel by name!
	virtual A3d_Channel*		GetChannel(const char*);
	// you can also use this function to manunaly manage the pointers DON'T USE THIS UNLESS THERE IS NO OTHER WAY!!!!
	virtual void				SetChannel(A3d_Channel*, int pos);

	// get the filename of the channel group
	virtual const char*			GetChannelGroupFileName();

	// we can also link to other channel groups with special external information!!
	// check if a channel is in this group
	virtual bool				GetIfChannelInGroup(A3d_Channel* channel);
	// we can request an public channel from this group
	virtual A3d_Channel*		GetPublicChannel(PublicCallInfo* publicCallInfo);

	// if we delete a public channel we have a problem we need to check ALL channels if we have this channel
	virtual void				CheckPublicChannelRemoval(A3d_Channel* channel, int poolNr);

	// if you call this function the group will call the channel that is the start channel
	virtual void				CallStartChannel();
	// set start channel
	virtual void				SetStartChannel(int newChannel);
	// get start channel
	virtual int					GetStartChannel();
	// channel groups can now also have multiple instances in a pool:)
	virtual int					GetPoolNr();
	// set
	virtual void				SetPoolNr(int poolNr);
	// GetPoolName
	virtual const char*			GetPoolName();
	// set
	virtual void				SetPoolName(const char*);
	// channels can speak to other groups trough an Parameters :)
	virtual A3d_Channel*		GetParameterChannel(int parametersNr);
	// channels can speak to other groups trough an Parameters :)
	virtual void				SetParameterChannel(A3d_Channel*);
	// a channel group can have an Parameters specified
	virtual int					GetParameterCount();
	// get Parameter info for a specific place
	virtual ParameterInterfaceInfo		GetParameterInfo(int index);
	// set Parameter info for a specific place
	virtual void				SetParameterInfo(ParameterInterfaceInfo parametersInfo, int indexNr);
	// add Parameter info for a specific place
	virtual int					AddParameterInfo(ParameterInterfaceInfo parametersInfo);
	// RemoveParameterInfo
	virtual void				RemoveParameterInfo(int index);
	// public channels shouldn't have the same name!!
	virtual bool				CheckDuplicatePublicChannelName(A3d_Channel* nameChannel);
	// in between each render call we can now update parent motions
	int							GetTreeCalculateCount();
	// increase render count 
	void						IncreaseTreeCount();
	// an interface can be changed if this is the case all channels need to reset their parameter interface
	virtual void				CheckInterfaceChange(const char* poolName, A3d_Channel* sourceChannel=NULL);
	// unique guid for this group
	virtual GUID				GetUniqueGroupGuid();
	// unique guid that is created when the group is created and is always unique also for files loaded from the same group
	virtual GUID				GetInstanceGuid();
	// this is makes some copying functionality possible!
	virtual void				SetChannelGroupFileName(const char* newFileName);
	// is this group loaded as an array ?
	virtual bool				GetIfArrayGroup();
	// SetIfArrayGroup
	virtual void				SetIfArrayGroup(bool newValue);
	// it might be important to know what the parent window is for our group
	virtual HWND				GetParentWindow();
	// it might be important to know what the parent window is for our group
	virtual void				SetParentWindow(HWND hwnd);
	// Retrieve the read only setting of the channel group
	virtual bool				GetAlwaysRecalc();
	// Change the read only setting of a channel group
	virtual void				SetAlwaysRecalc(bool newSetting);
	// Retrieve the read only setting of the channel group
	virtual bool				GetReadOnly();
	// Change the read only setting of a channel group
	virtual void				SetReadOnly(bool newSetting);
	// SetChannelTypeChangeSave
	virtual void				SetChannelTypeChangeSave(int channelIndexToChange, GUID changeGuid);
	// GetChannelTypeChangeSave
	virtual int					GetChannelTypeChangeSave(GUID* guid);
	// set protected
	virtual void				SetGroupIsProtected(bool newValue);
	// GetGroupIsProtected
	virtual bool				GetGroupIsProtected();
	// EnCrypt
	virtual void				EnCrypt(const char* fileName) ;
	// DeCrypt
	virtual void				DeCrypt(A3dFileLoader &loaderGroup);
	// get unique channel
	virtual A3d_Channel*		GetUniqueChannel(GUID guid);
	// AddMetaData
	virtual void				AddMetaData();
	// get unique channel count
	virtual int					GetUniqueChannelCount();
	// get unique channel
	virtual A3d_Channel*		GetUniqueChannel(int nr);

private:
	// Read only
	bool						isReadOnly_;
	// channels we use
	A3d_List					channelsList_;
	// for loading a channel group
	A3d_List					requestList_;
	// request count
	int							requestCount_;
	// we remember where we where created from!!
	A3d_String*					channelGroupFileName_;
	// to which group do we belong
	A3d_String*					poolGroupName_;
	// channel group index
	int							groupIndex_;
	// start channel
	int							startChannel_;
	// which Poolnr are we
	int							poolNr_;
	// channel which has the Parameters pointers
	SmartChannel*				parameterChannel_;
	// list of all our Parameterss
	A3d_List					parameterList_; 
	// our tree calculate count
	int							treeCalculateCount_;
	// unique guid for this group
	GUID						groupGuid_;
	// this group is an array?
	bool						arrayGroup_;
	// usefastdelete
	bool						useFastDelete_;
	// pointer to our extended intern only interface
	EngineInterface*			engine;
	// parent window
	HWND						parentWindow_;
	// int channelIndexToChange, GUID changeGuid
	int							channelIndexToChange_;
	// instanceGuid_
	GUID						instanceGuid_;
	// changeGuid_
	GUID						changeGuid_;
	// protected
	bool						isGroupProtected_;
	// unique channels we use
	A3d_List					uniqueChannelsList_;
	// always recalc treecount
	bool						alwaysRelalcTreeCount_;
};

#endif // !defined(AFX_A3D_CHANNELGROUP_H__2B9380A1_2CA5_11D4_A351_0050DAD61B65__INCLUDED_)
