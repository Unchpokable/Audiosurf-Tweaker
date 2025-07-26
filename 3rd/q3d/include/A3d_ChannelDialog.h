// A3d_Channels.h: interface for the A3d_Channels class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHANNELDIALOG_H__0E5E0941_2B0D_11D4_A351_0050DAD61B65__INCLUDED_)
#define AFX_CHANNELDIALOG_H__0E5E0941_2B0D_11D4_A351_0050DAD61B65__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef CHANNELDIAG_EXPORTS
#define CHANNELDIAG_API __declspec(dllexport)
#else
#define CHANNELDIAG_API __declspec(dllimport)
#endif

#define TAB_CHANNELDIALOG		0
#define TAB_GENERALDIALOG		1
#define TAB_CHANNELINTERFACEDIALOG		2
#define TAB_FEEDBACKDIALOG		3
 
class SmartChannel;
class ChannelInterfaces;
class FeedbackDialog;

// lets define a channel dialog module
class CHANNELDIAG_API ChannelDialog : public DllInterface
									, public EngineListener
{
public:
	ChannelDialog();
	virtual ~ChannelDialog();

	virtual void		OnAboutToReleaseChannel(A3d_Channel*);
	// release this channel diag
	virtual void		Release();
	// set the channel for this dialog
	virtual void		SetDialogChannel(A3d_Channel* input);
	// get the channel
	virtual A3d_Channel* GetDialogChannel();
	// set the channel for this dialog
	virtual	bool		GetChannelDialog(bool modeLess);
	// if the dialog uses a 3d interface you need to be told when to draw since 
	// if you want to use the already initialized engine channels you need the correct time 
	virtual bool		UpdateDialog();

	// functions to manage the editor
	virtual void		UpdateEditor();
	// get the HWND from quest3d
	virtual HWND		GetApplicationHandle();
	// get the HINSTANCE from quest3d
	virtual HINSTANCE	GetApplicationInstance();
	// let the editor know that this dialog is closed!
	virtual void		Quest3DReleaseDialog();
	// SetDialogClosed
	virtual void		SetDialogClosed(bool closed);
	// GetDialogClosed
	virtual bool		GetDialogClosed();
	// add dialog
	virtual void		AddDialogHwndToIsDialogLoop(HWND hwnd);
	// add dialog
	virtual void		RemoveDialogHwndFromIsDialogLoop(HWND hwnd);
	// support docking
	virtual bool		GetIfDockingIsSupported();
	// support docking
	virtual HWND		CreateDockingInterface();
	// DestroyDockingWindow
	virtual void		DestroyDockingWindow();
	// HandleDockingWindowMessages
	virtual bool		HandleDockingWindowMessages(UINT message, WPARAM wParam, LPARAM lParam);
	// CreateCorrectTabWindow
	virtual void		CreateCorrectTabWindow() ;
	// create docked channel dialog
	virtual bool		CreateDockedChannelDialog(HWND parentHwnd);
	// HideMainWindow
	virtual void		HideMainWindow(bool hide);
	// SetNewTabWindow
	void				SetNewTabWindow(int newTab);
	// SetTabWindowSize
	virtual void		SetTabWindowSize(int X, int Y, int newWidth, int newHeight);
	// ReSizeDockingControls
	virtual void		ReSizeDockingControls();
	// ClickedOk
	virtual void		ClickedOk();
	// ClickedCancel
	virtual void		ClickedCancel();
	// GetStandardDialogRect
	virtual RECT		GetStandardDialogRect();
	// support cancel
	virtual bool		GetIfSupportForCancel();
	// GetIfUseChannelTab
	virtual bool		GetIfUseTabNr(int nr);
	// HideOldTabWindow
	virtual void		HideOldTabWindow(int oldTabNr);
 	// GetDialogEditorVersion
	virtual int			GetDialogEditorVersion();
	// SendMoveMessage
	virtual void		SendOurMoveMessage();
	// on return pressed for channel window
	virtual void		OnReturnPressed();
	// GetEditionRegistration
	virtual bool		GetEditionRegistration(int editionType);

protected:
	// we remember the channel
	A3d_Channel*		channel_;
	// lets remember the channel as a smart channel!
	SmartChannel*		smartChannel_;
	// are we closed
	bool				dialogClosed_;
	// dockingHwnd_
	HWND				dockingHwnd_;
	// tabHwnd_
	HWND				tabHwnd_;
	// current tab setting
	int					currentTabSetting_;

private:
	// ChannelDialog
	ChannelDialog*		generalDialog_;
	// main window was created
	bool				mainWindowWasCreated_;
	// docking rect
	RECT				dockingRect_;
	// ChannelInterfaces
	ChannelInterfaces*	channelInterfaces_;
	// feedbackDialog_
	FeedbackDialog*     feedbackDialog_;
};

#endif // !defined(AFX_CHANNELDIALOG_H__0E5E0941_2B0D_11D4_A351_0050DAD61B65__INCLUDED_)
