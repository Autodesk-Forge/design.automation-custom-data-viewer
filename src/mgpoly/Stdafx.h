//
//////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2016 Autodesk, Inc.  All rights reserved.
//
//  Use of this software is subject to the terms of the Autodesk license 
//  agreement provided at the time of installation or download, or which 
//  otherwise accompanies this software in either electronic or hard copy form.   
//
//////////////////////////////////////////////////////////////////////////////
//
//



// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#using <mscorlib.dll>
#using <system.dll>
#using <system.core.dll>    // for ObjectId's System.Dynamic.IDynamicMetaObjectProvider
#using <acdbmgd.dll>

#include <use_ansi.h>

#include <WTypes.h>
#include <basetsd.h>
#include <winbase.h>
#include <winnt.h>
#include <vcclr.h>

// arx headers
#include "rxdefs.h"
#include "dbmain.h"

// poly includes
#include "..\polyobj\poly.h"
#include "mgPoly.h"

#ifndef WINVER
#define WINVER 0x500
#endif

//- This line allows us to get rid of the .def file in ARX projects
#if !defined(_WIN64) && !defined (_AC64)
#pragma comment(linker, "/export:_acrxGetApiVersion,PRIVATE")
#else
#pragma comment(linker, "/export:acrxGetApiVersion,PRIVATE")
#endif
