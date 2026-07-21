#if !defined(AFX_ACTNEW_H__DADD24CA_ABC5_451F_A1AD_4D2268F3BC16__INCLUDED_)
#define AFX_ACTNEW_H__DADD24CA_ABC5_451F_A1AD_4D2268F3BC16__INCLUDED_

#define CRTDBG_MAP_ALLOC
#include <crtdbg.h>

#ifdef _DEBUG
   #define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
#else
   #define DEBUG_CLIENTBLOCK
#endif // _DEBUG

#ifdef _DEBUG
#define Act_New DEBUG_CLIENTBLOCK
#endif
#ifndef _DEBUG
#define Act_New new
#endif

#endif // !defined(AFX_ACTNEW_H__DADD24CA_ABC5_451F_A1AD_4D2268F3BC16__INCLUDED_)
