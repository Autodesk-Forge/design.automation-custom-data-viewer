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
// This is the main DLL file.
#include "StdAfx.h"

#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
#error _DEBUG should not be defined except in internal Adesk debug builds
#endif


//////////////////////////////////////////////////////////////////////////
#include <gcroot.h>
#include <dbdate.h>
#include "mgdinterop.h"


//////////////////////////////////////////////////////////////////////////
// constructor
Autodesk::ObjectDbxSample::Poly::Poly() 
:Autodesk::AutoCAD::DatabaseServices::Curve(System::IntPtr(new AsdkPoly()), true)
{
    acutPrintf(_T("\n*********************Constructor"));
}

//////////////////////////////////////////////////////////////////////////
Autodesk::ObjectDbxSample::Poly::Poly(System::IntPtr unmanagedPointer, bool autoDelete)
: Autodesk::AutoCAD::DatabaseServices::Curve(unmanagedPointer,autoDelete)
{
}

//////////////////////////////////////////////////////////////////////////
// set the centre of the poly
void Autodesk::ObjectDbxSample::Poly::Center::set(Point2d point)
{
  Autodesk::AutoCAD::Runtime::Interop::Check(GetImpObj()->setCenter(GETPOINT2D(point)));
}
//////////////////////////////////////////////////////////////////////////
// get the center point
Point2d Autodesk::ObjectDbxSample::Poly::Center::get()
{
    return ToPoint2d (GetImpObj()->center());
}

//////////////////////////////////////////////////////////////////////////
// set the start point
void Autodesk::ObjectDbxSample::Poly::StartPoint2d::set(Point2d point)
{
    Autodesk::AutoCAD::Runtime::Interop::Check(GetImpObj()->setStartPoint(GETPOINT2D(point)));
}
//////////////////////////////////////////////////////////////////////////
// get the start point
Point2d Autodesk::ObjectDbxSample::Poly::StartPoint2d::get()
{
    return ToPoint2d (GetImpObj()->startPoint());
}

//////////////////////////////////////////////////////////////////////////
// set number of polygon sides
void Autodesk::ObjectDbxSample::Poly::NumberOfSides::set(long value)
{
    Autodesk::AutoCAD::Runtime::Interop::Check(GetImpObj()->setNumSides(value));
}
//////////////////////////////////////////////////////////////////////////
// get the number of polygon sides
long Autodesk::ObjectDbxSample::Poly::NumberOfSides::get()
{
    return GetImpObj()->numSides();
}

//////////////////////////////////////////////////////////////////////////
// set the normal vector
void Autodesk::ObjectDbxSample::Poly::Normal::set(Vector3d vector)
{
    Autodesk::AutoCAD::Runtime::Interop::Check(GetImpObj()->setNormal(GETVECTOR3D(vector)));
}
//////////////////////////////////////////////////////////////////////////
// get the normal vector
Vector3d Autodesk::ObjectDbxSample::Poly::Normal::get()
{
    return ToVector3d(GetImpObj()->normal());
}

//////////////////////////////////////////////////////////////////////////
// set the elevation
void Autodesk::ObjectDbxSample::Poly::Elevation::set(double value)
{
    GetImpObj()->setElevation(value);
}
//////////////////////////////////////////////////////////////////////////
// get the elevation
double Autodesk::ObjectDbxSample::Poly::Elevation::get()
{
    return GetImpObj()->elevation();
}

//////////////////////////////////////////////////////////////////////////
// set the string name
void Autodesk::ObjectDbxSample::Poly::Name::set(String^ value)
{
    Autodesk::AutoCAD::Runtime::Interop::Check(GetImpObj()->setName(StringToCIF(value)));
}
//////////////////////////////////////////////////////////////////////////
// get the string name
String^ Autodesk::ObjectDbxSample::Poly::Name::get()
{
    return CIFToString(GetImpObj()->name());
}

//////////////////////////////////////////////////////////////////////////
// set the text style record 
void Autodesk::ObjectDbxSample::Poly::TextStyle::set(Autodesk::AutoCAD::DatabaseServices::ObjectId value)
{
    Autodesk::AutoCAD::Runtime::Interop::Check(GetImpObj()->setTextStyle(GETOBJECTID(value)));
}

Autodesk::AutoCAD::DatabaseServices::ObjectId Autodesk::ObjectDbxSample::Poly::TextStyle::get()
{
    return Autodesk::AutoCAD::DatabaseServices::ObjectId(System::IntPtr(GetImpObj()->styleId()));
}
//////////////////////////////////////////////////////////////////////////
// get the vertices into a 2d point array
Point2dCollection^ Autodesk::ObjectDbxSample::Poly::Vertices::get()
{
    auto pArr = new AcGePoint2dArray();
    Autodesk::AutoCAD::Runtime::Interop::Check(GetImpObj()->getVertices2d(*pArr));

    array<Point2d>^ ret = gcnew array<Point2d>(pArr->length());
    if (ret->Length>0)
    {
        pin_ptr<array<Point2d>^> pinner = &ret;
        IntPtr tmp = System::Runtime::InteropServices::Marshal::UnsafeAddrOfPinnedArrayElement(ret, 0);
        memcpy_s(tmp.ToPointer(), sizeof(Point2d)*pArr->length(), pArr->asArrayPtr(), sizeof(AcGePoint2d)*ret->Length);
    }
    return gcnew Point2dCollection(ret);
}
