// A3d_List.h: interface for the A3d_List class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_A3D_LIST_H__AD83C981_2CCD_11D4_A351_0050DAD61B65__INCLUDED_)
#define AFX_A3D_LIST_H__AD83C981_2CCD_11D4_A351_0050DAD61B65__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Act_New.h"

#ifdef DLLHIGHPOLY_EXPORTS
#define DLLHIGHPOLY_API __declspec(dllexport)
#else
#define DLLHIGHPOLY_API __declspec(dllimport)
#endif

class DLLHIGHPOLY_API A3d_List
{
public:
	A3d_List();
	virtual ~A3d_List();

	// release the list
	void			ReleaseItems();
	// release the items don't delete the list
	void			ClearItems();
	// add an item to the list 
	int				AddItem(void *newItem);
	// add an item to the list or insert if NULL spot
	int				AddItemOrNULL(void *newItem);
	// remove an item from the list
	void			RemoveItem(int nr);
	// get item
	void*			GetItem(int nr);
	// set item
	void			SetItem(void *newItem, int nr);
	// get the number of items we have
	int				GetItemCount();
	// allocate a number of pointers
	bool			Allocate(int newSize);
	// insert item after indexnr
	bool			InsertItem(void*, int);

protected:
	// IncreaseRealListLength
	void			IncreaseRealListLength(int requestedSize=-1);
	// we are going to make a list of type T
	void			**list_;
	// number of arguments in the list
	int				listLength_;
	// lets keep a longer internal list
	int				realListLength_;
};

#endif // !defined(AFX_A3D_LIST_H__AD83C981_2CCD_11D4_A351_0050DAD61B65__INCLUDED_)
