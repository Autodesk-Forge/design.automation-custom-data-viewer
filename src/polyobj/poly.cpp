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
// DESCRIPTION:
//
// This file contains the body of the class AsdkPoly.

#define WIN32_LEAN_AND_MEAN
#if defined(_DEBUG) && !defined(AC_FULL_DEBUG)
#error _DEBUG should not be defined except in internal Adesk debug builds
#endif

#include <windows.h>
#include <objbase.h>

#include "rxregsvc.h"
#include "assert.h"
#include "math.h"

#include "gepnt3d.h"
#include "gevec3d.h"
#include "gelnsg3d.h"
#include "gearc3d.h"

#include "dbents.h"
#include "dbsymtb.h"
#include "dbcfilrs.h"
#include "dbspline.h"
#include "dbproxy.h"
#include "dbxutil.h"
#include "acutmem.h"

#include "util.h"
#include "poly.h"

#include "acdb.h"
#include "dbidmap.h"
#include "adesk.h"

#include "dbapserv.h"
#include "appinfo.h"
#include "tchar.h"

#include <basetsd.h>
#include "ac64bithelpers.h"

#include "AsdkPolyAssocPersSubentIdPE.h"
#include "ConstrainedRigidEntityAssocPersSubentIdPE.h"

// This is to allow AsdkPoly entities to participate in associative relations 
// represented by the Associative Framework, such as geometric and dimensional 
// constraints can be created between AsdkPoly entities and other entities in
// the drawing.
//
// Look for a comment "For Constraints" that marks all places in the AsdkPoly 
// code related to constraint support. No other code changes are needed to make 
// a custom entity constrainable and recognizable by the Associative Framework
//
static AsdkPolyAssocPersSubentIdPE               sPolyAssocPersSubentIdPEProper;
static ConstrainedRigidEntityAssocPersSubentIdPE sPolyAssocPersSubentIdPE(&sPolyAssocPersSubentIdPEProper);


// Object Version
// Version 1:
// AcGePoint3d mCenter
// AcGePoint3d mStartPoint
// int mNumSides
// AcGeVector3d mPlaneNormal
// Version 2:
// AcGePoint2d mCenter
// AcGePoint2d mStartPoint
// int mNumSides
// AcGeVector3d mPlaneNormal
// double mElevation
#define VERSION 2

extern "C" {

// locally defined entry point invoked by Rx.

AcRx::AppRetCode __declspec(dllexport) acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt);

}

//*************************************************************************
// Statics functions used in this file. 
//*************************************************************************

static Acad::ErrorStatus drawFacets(const AsdkPoly*         poly,
                                          AcGiWorldDraw*    worldDraw,
                                          AcGiViewportDraw* vportDraw);

static Acad::ErrorStatus drawEdges (const AsdkPoly*         poly,
                                          AcGiWorldDraw*    worldDraw,
                                          AcGiViewportDraw* vportDraw);

static Acad::ErrorStatus intLine   (const AsdkPoly*         poly, 
                                    const AcGeLine3d        line,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intLine   (const AsdkPoly*         poly, 
                                    const AcDbLine*         line,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intLine   (const AsdkPoly*         poly, 
                                    const AcGeLineSeg3d     line,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intArc    (const AsdkPoly*         poly, 
                                    const AcDbArc*          arc,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intArc    (const AsdkPoly*         poly, 
                                    const AcGeCircArc3d     arc,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intCircle (const AsdkPoly*         poly, 
                                    const AcDbCircle*       circle,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intCircle (const AsdkPoly*         poly, 
                                    const AcGeCircArc3d     circle,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intPline  (const AsdkPoly*         poly, 
                                          AcDb2dPolyline*   pline,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus intPline  (const AsdkPoly*         poly, 
                                          AcDb3dPolyline*   pline,
                                          AcDb::Intersect   intType,
                                    const AcGePlane*        projPlane,
                                          AcGePoint3dArray& points);

static Acad::ErrorStatus drawName(const AsdkPoly*           poly,
                                        AcGiWorldDraw*      worldDraw);

//*************************************************************************
// Static class data initialization 
//*************************************************************************

bool AsdkPoly::mUseDragData = false;

//*************************************************************************
// Code for the Class Body. 
//*************************************************************************

ACRX_DXF_DEFINE_MEMBERS(AsdkPoly, AcDbCurve,
AcDb::kDHL_CURRENT, AcDb::kMReleaseCurrent, 
AcDbProxyEntity::kAllAllowedBits, POLYGON, 
"AsdkPolyOBJ2.0\
|Product Desc:     PolyCAD ARX App For Polygon Entity\
|Company:          Autodesk,Inc.\
|WEB Address:      www.autodesk.com");

//*************************************************************************
// Constructors and destructors 
//*************************************************************************

AsdkPoly::AsdkPoly() :
    mCenter(0,0),mStartPoint(0,0), mNumSides(3),mPlaneNormal(0,0,1),
    mTextStyle(AcDbObjectId::kNull),mpName(NULL),mDragDataFlags(0),
    mShowAffordances(false)
{
}

Acad::ErrorStatus AsdkPoly::set(const AcGePoint2d&   center,
                   const AcGePoint2d&   startPoint,
                         int            numSides, 
                   const AcGeVector3d&  normal,
                   const TCHAR*          name,
                   double               elevation)
{
    if (numSides<3 || normal.isZeroLength())
        return Acad::eInvalidInput;
    assertWriteEnabled();

    mCenter = center;
    mDragCenter = center;

    mStartPoint = startPoint;
    mDragStartPoint = startPoint;

    mPlaneNormal = normal.normal();
    mDragPlaneNormal = mPlaneNormal;

    mElevation = elevation;
    mDragElevation = elevation;

    mNumSides = numSides;
    return setName(name);
}

Acad::ErrorStatus AsdkPoly::set(const AcGePoint2d&   center,
                   const AcGePoint2d&   startPoint,
                         int            numSides, 
                   const AcGeVector3d&  normal,
                   const TCHAR*         name,
                   AcDbObjectId   styleId,
                   double elevation)
{
    if (numSides<3 || normal.isZeroLength())
        return Acad::eInvalidInput;
    assertWriteEnabled();

    mCenter = center;
    mDragCenter = center;

    mStartPoint = startPoint;
    mDragStartPoint = startPoint;

    mPlaneNormal = normal.normal();
    mDragPlaneNormal = mPlaneNormal;

    mElevation = elevation;
    mDragElevation = elevation;

    mNumSides = numSides;
    mTextStyle = styleId;

    return setName(name);
}

AsdkPoly::~AsdkPoly()
{
    sPolyAssocPersSubentIdPE.notifyEntityDeleted(this); // For Constraints
    acutDelString(mpName);
}


//*************************************************************************
// Methods specific to AsdkPoly 
//*************************************************************************
void AsdkPoly::getCenter(AcGePoint3d& cent) const
{
    assertReadEnabled();
    AcGePoint2d c2d(center());
    cent.set(c2d.x,c2d.y,elevation());
    acdbEcs2Wcs(asDblArray(cent),asDblArray(cent),asDblArray(normal()),Adesk::kFalse);
}
const TCHAR*
AsdkPoly::name() const
{
    assertReadEnabled();
    return mpName;
}

const AcDbObjectId&
AsdkPoly::styleId() const
{
    assertReadEnabled();
    return mTextStyle;
}

Acad::ErrorStatus  
AsdkPoly::setCenter(const AcGePoint2d& cen)
{
    assertWriteEnabled();
    mCenter = cen;
    mDragCenter = cen;
    return Acad::eOk;
}

Acad::ErrorStatus  
AsdkPoly::setNormal(const AcGeVector3d& norm)
{
    assertWriteEnabled();
    mPlaneNormal = norm.normal();
    mDragPlaneNormal = mPlaneNormal;
    return Acad::eOk;
}

Acad::ErrorStatus  
AsdkPoly::setStartPoint(const AcGePoint2d& startPt)
{
    assertWriteEnabled();
    mStartPoint = startPt;
    mDragStartPoint = startPt;
    return Acad::eOk;
}
    
Acad::ErrorStatus
AsdkPoly::setName(const TCHAR* pName)
{
    assertWriteEnabled();
    
    acutDelString(mpName);
    mpName = NULL;

    if(pName != NULL)
    {
        acutUpdString(pName, mpName);
    }

    return Acad::eOk;
}

// THE FOLLOWING CODE APPEARS IN THE SDK DOCUMENT.

Acad::ErrorStatus  
AsdkPoly::setNumSides(int numSides)    
{
    assertWriteEnabled(Adesk::kFalse, Adesk::kTrue);
    if (numSides<3)
        return Acad::eInvalidInput;

    if (mNumSides == numSides)
        return Acad::eOk;
    // There are situations under which AutoCAD doesn't
    // want to do undo recording, so it won't create an
    // undo filer. Check for the existence of the filer
    // before starting to write into it.
    //
    AcDbDwgFiler *pFiler = NULL;
    if ((pFiler = undoFiler()) != NULL) {
        undoFiler()->writeAddress(AsdkPoly::desc());
        undoFiler()->writeItem((Adesk::Int16)kSetNumSides);
        undoFiler()->writeItem((Adesk::Int32)mNumSides);
    }

    mNumSides = numSides; 
    return Acad::eOk;
}

// END CODE APPEARING IN SDK DOCUMENT.

Acad::ErrorStatus
AsdkPoly::setTextStyle(const AcDbObjectId& tsId)
{
    assertWriteEnabled();
    mTextStyle = tsId;
    return Acad::eOk;
}


AcGeVector3d
AsdkPoly::sideVector(int whichSide) const
{
    assertReadEnabled();

    AcGePoint2dArray vertexArray;
    getVertices2d(vertexArray);

    AcGeVector2d vec = vertexArray[whichSide + 1] - vertexArray[whichSide];
    assert(!vec.isZeroLength());
    vec.normalize();
    AcGeVector3d ret(vec.x,vec.y,elevation());
    acdbEcs2Wcs(asDblArray(vec),asDblArray(vec),asDblArray(normal()),Adesk::kTrue);
    return ret;
}

Acad::ErrorStatus 
AsdkPoly::getVertices2d(AcGePoint2dArray& vertexArray) const
{
    assertReadEnabled();

    double circumRadius = (startPoint()-center()).length();
    double sideLength = 2.0 * circumRadius * cos(3.14159265358979323846
        * 0.5 - 3.14159265358979323846 / mNumSides);

    AcGePoint2d vertex;
    double rotateBy, increment,base;
    
    vertexArray.append(startPoint());
    rotateBy = increment = 2.0 * 3.14159265358979323846 / mNumSides;
    AcGeVector2d dir(startPoint()-center());
    base = atan2(dir.y,dir.x);

    for (int i = 0; i < mNumSides ; i++) {
        vertex[X] = circumRadius * cos(rotateBy+base); 
        vertex[Y] = circumRadius * sin(rotateBy+base); 
        vertexArray.append(vertex+center().asVector());
        rotateBy += increment;
    }
    return Acad::eOk;
}

Acad::ErrorStatus 
AsdkPoly::getVertices3d(AcGePoint3dArray& vertexArray) const
{
    AcGePoint2dArray temparray;
    getVertices2d(temparray);
    AcGePoint3d temppoint;
    for (int i=0;i<temparray.length();i++)
    {
        temppoint.set(temparray[i].x,temparray[i].y,elevation());
        acdbEcs2Wcs(asDblArray(temppoint),asDblArray(temppoint),asDblArray(normal()),Adesk::kFalse);
        vertexArray.append(temppoint);
    }
    return Acad::eOk;
}


//*************************************************************************
// Overridden methods from AcDbCurve
//*************************************************************************


Acad::ErrorStatus 
AsdkPoly::getPointAtParam(double param, AcGePoint3d& point) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    // Find out how many included angles "param" contains.
    // Find that vertex, and the direction of the line defined
    // by that vertex and the following vertex. Add the appropriate
    // distance along that line direction to get the new point.

    rx_fixangle(param);

    double circumRadius      = (startPoint() - center()).length();
    double includedAngle     = 6.28318530717958647692 / mNumSides;
    int    numIncludedAngles = int(param / includedAngle);
    double theta             = param - numIncludedAngles * includedAngle;

    double gamma             = 3.14159265358979323846 * 0.5 + 3.14159265358979323846 / mNumSides - theta;
    double distToGo          = circumRadius * sin(theta) / sin(gamma);

    AcGeVector3d lineDir     = sideVector(numIncludedAngles);

    AcGePoint3dArray vertexArray;
    if ((es = getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    if (theta > 1.0e-10) {
        point = vertexArray[numIncludedAngles] + (distToGo * lineDir);
    } else {
        point = vertexArray[numIncludedAngles];
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getParamAtPoint(const AcGePoint3d& point3d, double& param) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;
    acdbWcs2Ecs(asDblArray(point3d),asDblArray(point3d),asDblArray(normal()),Adesk::kFalse);
    AcGePoint2d point(point3d.x,point3d.y);
    // Check to see if the point is the same as one of the vertices of
    // the polygon. If so, great. Otherwise, see if it lies in between 
    // two consecutive vertices. If neither is true, the point if
    // not on the polygon.

    AcGePoint2dArray vertexArray;
    if ((es = getVertices2d(vertexArray)) != Acad::eOk) {
        return es;
    }
   
    double includedAngle = 6.28318530717958647692 / mNumSides;
    double circumRadius  = (startPoint() - center()).length();

    for (int i = 0; i < vertexArray.length() - 1; i++) {

        if (point == vertexArray[i]) {
            param = i * includedAngle;
            return Acad::eOk;
        } else if (point == vertexArray[i + 1]) {
            param = (i + 1) * includedAngle;
            return Acad::eOk;
        } else {
            
            AcGeVector2d v1, v2;
            v1 = point - vertexArray[i];
            v2 = vertexArray[i + 1] - point;

            if (v2.isCodirectionalTo(v1)) {

                double alpha          = 3.14159265358979323846 * 0.5 - 3.14159265358979323846 / mNumSides;
                double halfSideLength = circumRadius * cos(alpha);
                double delta          = atan(
                                        fabs(v1.length() - halfSideLength) /
                                        (circumRadius * sin(alpha)));

                param = v1.length() > halfSideLength ? 
                        ((i + 0.5) * includedAngle + delta)  :
                        ((i + 0.5) * includedAngle - delta);

                return Acad::eOk;
            }
        }
    }

    return Acad::ePointNotOnEntity;
}


Acad::ErrorStatus 
AsdkPoly::getDistAtParam(double param, double& dist)  const
{
    assertReadEnabled();

    rx_fixangle(param);

    double circumRadius = (startPoint() - center()).length();
    double alpha        = 3.14159265358979323846 * 0.5 - 3.14159265358979323846 / mNumSides;
    double sideLength   = 2.0 * circumRadius * cos(alpha);

    double includedAngle     = 6.28318530717958647692 / mNumSides;
    int    numIncludedAngles = int(param / includedAngle);
    double theta             = param - numIncludedAngles * includedAngle;

    if (theta > 1.0e-10) {
        dist = sideLength * numIncludedAngles +
               circumRadius * sin(theta) / sin(3.14159265358979323846 - theta - alpha);
    } else {
        dist = sideLength * numIncludedAngles;
    }

    return Acad::eOk;
}


Acad::ErrorStatus 
AsdkPoly::getParamAtDist(double dist, double& param) const 
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    double circumRadius  = (startPoint() - center()).length();
    double alpha         = 3.14159265358979323846 * 0.5 - 3.14159265358979323846 / mNumSides;
    double sideLength    = 2.0 * circumRadius * cos(alpha);

    int    numSides      = int(dist / sideLength);
    double distToGo      = dist - numSides * sideLength;

    double includedAngle = 6.28318530717958647692 / mNumSides;

    if (distToGo > 1.0e-10) {

        double delta          = atan(
                                fabs(distToGo - sideLength * 0.5) /
                                (circumRadius * sin(alpha)));
        param = distToGo > sideLength * 0.5? 
                ((numSides + 0.5) * includedAngle + delta)  :
                ((numSides + 0.5) * includedAngle - delta);

    } else {
        param = numSides * includedAngle;
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getDistAtPoint(const AcGePoint3d& point, double& dist)  const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    double param;

    if ((es = getParamAtPoint(point, param)) != Acad::eOk) {
        return es;
    }
    if ((es = getDistAtParam(param, dist)) != Acad::eOk) {
        return es;
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getPointAtDist(double dist, AcGePoint3d& point) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    double param;

    if((es = getParamAtDist(dist, param)) != Acad::eOk) {
        return es;
    }
    if ((es = getPointAtParam(param, point)) != Acad::eOk) {
        return es;
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getFirstDeriv(double param, AcGeVector3d& firstDeriv) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }
   
    double includedAngle     = 6.28318530717958647692 / mNumSides;
    int    numIncludedAngles = int(param / includedAngle);
    double theta             = param - numIncludedAngles * includedAngle;

    if (theta > 1.0e-10) {              // On a side

        firstDeriv = vertexArray[numIncludedAngles + 1] -
                     vertexArray[numIncludedAngles];
        firstDeriv.normalize();

    } else {                            // On a vertex
        return Acad::eSingularPoint;
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getFirstDeriv(const AcGePoint3d&  point, 
                              AcGeVector3d& firstDeriv) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    double param;
    if ((es = getParamAtPoint(point, param)) != Acad::eOk) {
        return es;
    }

    return getFirstDeriv(param, firstDeriv);
}


Acad::ErrorStatus 
AsdkPoly::getClosestPointTo (const AcGePoint3d&   givenPoint,
                                   AcGePoint3d&   pointOnCurve,
                                   Adesk::Boolean /*extend*/) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcGeLineSeg3d lnsg(vertexArray[0], vertexArray[1]);
    double dist = lnsg.distanceTo(givenPoint);
    int index = 0;

    double dist2;
    for (int i = 1; i < vertexArray.length() - 1; i++) {

        lnsg.set(vertexArray[i], vertexArray[i + 1]);
        if ((dist2 = lnsg.distanceTo(givenPoint)) < dist) {
            dist = dist2;
            index = i;
        }
    }

    lnsg.set(vertexArray[index], vertexArray[index + 1]);
    pointOnCurve = lnsg.closestPointTo(givenPoint);

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getClosestPointTo (const AcGePoint3d&   givenPoint,
                             const AcGeVector3d&  normal,
                                   AcGePoint3d&   pointOnCurve,
                                   Adesk::Boolean /*extend*/) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    // Project the polygon as well as the point onto a plane
    // defined by the polygon center and the normal.

    AcDb::Planarity plnrty;
    AcGePlane plane;
    getPlane(plane,plnrty);
    AcDbCurve* curve = NULL;
    if ((es = getOrthoProjectedCurve(plane, curve)) != Acad::eOk) {
        return es;
    }
    assert(curve != NULL);
    AcGePoint3d pt = givenPoint.orthoProject(plane);

    // Find the nearest point on the projected curve from the
    // projected point.

    AcGePoint3d newPt;
    if ((es = curve->getClosestPointTo(pt, newPt)) != Acad::eOk) {
        return es;
    }

    // Project the point back onto the polygon.
    //
    // Define a line starting from the newPt and along the normal.
    // Intersect the polygon with that line. Find all the points
    // and pick the one closest to the given point.

    AcGePoint3dArray pts;
    AcGeLine3d line(newPt, normal);
    if ((es = intLine(this, line, pts)) != Acad::eOk) {
        return es;
    }

    pointOnCurve = pts[0];
    double length = (givenPoint - pts[0]).length();
    double length2;
    for (int i = 1; i < pts.length(); i++) {
        if ((length2 = (givenPoint - pts[i]).length()) < length) {
            pointOnCurve = pts[i];
            length = length2;
        }
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getOrthoProjectedCurve(const AcGePlane&  plane,
                                       AcDbCurve*& projectedCurve) const
{
    assertReadEnabled();

    AcGeVector3d projDir = plane.normal();
    return getProjectedCurve(plane, projDir, projectedCurve);
}


Acad::ErrorStatus 
AsdkPoly::getProjectedCurve(const AcGePlane&    plane,
                            const AcGeVector3d& projDir,
                                  AcDbCurve*&   projectedCurve) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    // project the start point and the center and create a new polygon.
    // The normal becomes the normal of the plane.

    if (projDir.isPerpendicularTo(plane.normal()))
        return Acad::eInvalidInput;
    
    if (projDir.isPerpendicularTo(normal())) {

        // Poly becomes a polyline.

        AcGePoint3dArray vertexArray;
        if ((es = getVertices3d(vertexArray)) != Acad::eOk) {
            return es;
        }

        for (int i = 0; i < vertexArray.length(); i++) {
            vertexArray[i] = vertexArray[i].project(plane, projDir);
        }
        AcDbSpline* pSpline;
        if ((es = rx_makeSpline(vertexArray, pSpline)) != Acad::eOk) {
            return es;
        }
        pSpline->setPropertiesFrom(this);

        projectedCurve = pSpline;

    } else {

        // Poly becomes another poly.

        AcGePoint3d center, startPoint;
        getCenter(center);
        center = center.project(plane, projDir);
        getStartPoint(startPoint);
        startPoint = startPoint.project(plane, projDir);

        AsdkPoly* newPoly = new AsdkPoly();
        if (newPoly==NULL)
            return Acad::eOutOfMemory;
        acdbWcs2Ecs(asDblArray(center),asDblArray(center),asDblArray(plane.normal()),Adesk::kTrue);
        acdbWcs2Ecs(asDblArray(startPoint),asDblArray(startPoint),asDblArray(plane.normal()),Adesk::kTrue);
        if ((es=newPoly->set(AcGePoint2d(center.x,center.y), AcGePoint2d(startPoint.x,startPoint.y), mNumSides, plane.normal(), mpName, center.z))!=Acad::eOk){
            delete newPoly;
            return es;
        }

        newPoly->setPropertiesFrom(this);
        projectedCurve = newPoly;
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getOffsetCurves(double            offsetDist, 
                          AcDbVoidPtrArray& offsetCurves) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    // Offset distance is measured perpendicular to each side.

    double theta = 3.14159265358979323846 / mNumSides;
    double extendBy = offsetDist / cos(theta);

    AcGeVector2d x = startPoint() - center();
    x.normalize();
    x *= extendBy;

    AcGePoint2d startPt = startPoint() + x;

    AsdkPoly* offsetPoly = new AsdkPoly();
    if (offsetPoly==NULL)
        return Acad::eOutOfMemory;

    if ((es=offsetPoly->set(center(), startPoint(), mNumSides, normal(), mpName,elevation()))!=Acad::eOk){
        delete offsetPoly;
        return es;
    }
    
    offsetPoly->setPropertiesFrom(this);
    offsetCurves.append(offsetPoly);

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getSplitCurves(const AcGeDoubleArray&  parameters,
                               AcDbVoidPtrArray& curveSegments) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    // Since this is a closed periodic curve, the number of segemnts
    // that will result from splitting will be the same as the number
    // parameters we are splitting at. Each of the segments will be
    // a 2d polyline.

    AcGePoint3dArray vertexArray;
    if ((es = getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    double param, nextParam, includedAngle = 6.28318530717958647692 / mNumSides;
    int    thisIndex, nextIndex;

    AcGePoint3dArray pts;
    AcGePoint3d   pt;

    AcDbSpline* pSpline = NULL;

    for (int i = 0; i < parameters.length(); i++) {
        
        param = parameters[i];
        if (i < parameters.length() - 1) {
            nextParam = parameters[i + 1];
        } else {
            nextParam = parameters[0];
        }

        rx_fixangle(param);
        rx_fixangle(nextParam);

        thisIndex = int(param / includedAngle) + 1;
        nextIndex = int(nextParam / includedAngle);

        double theta = nextParam - nextIndex * includedAngle;
        if (theta < 1.0e-10) {
            
            // Reduce nextIndex by one since we are going to evaluate
            // at nextParam anyway. Since integer division truncates,
            // we cannot decrement nextIndex all the time.

            nextIndex -= 1;
        }

        if (nextIndex < thisIndex)
            nextIndex += mNumSides;

        if ((es = getPointAtParam(param, pt)) != Acad::eOk)
            return es;
        pts.append(pt);

        int idx;
        for (int j = thisIndex; j <= nextIndex; j++) {

            idx = j;
            rx_fixindex(idx, mNumSides);
            pts.append(vertexArray[idx]);
        }
		if ( (es =getPointAtParam(nextParam, pt)) != Acad::eOk)
			return es;
        pts.append(pt);

		if ( (es =rx_makeSpline(pts, pSpline)) != Acad::eOk)
			return es;
        pSpline->setPropertiesFrom(this);
        pts.setPhysicalLength(0);
        curveSegments.append(pSpline);
    }

    return es;
}


Acad::ErrorStatus 
AsdkPoly::getSplitCurves(const AcGePoint3dArray& points,
                               AcDbVoidPtrArray& curveSegments) const
{
    assertReadEnabled();

    AcGeDoubleArray params;
    params.setLogicalLength(points.length());
    for (int i = 0; i < points.length(); i++) 
    {
        getParamAtPoint(points[i], params[i]);
    }

    return getSplitCurves(params, curveSegments);
}


Acad::ErrorStatus 
AsdkPoly::getArea(double& area) const
{
    assertReadEnabled();

    double circumRadius = (startPoint() - center()).length();

    area = mNumSides * circumRadius * cos(3.14159265358979323846 * 0.5 - 3.14159265358979323846 / mNumSides)
                     * circumRadius * sin(3.14159265358979323846 * 0.5 - 3.14159265358979323846 / mNumSides);

    return Acad::eOk;
}


//*************************************************************************
// Overridden methods from AcDbEntity
//*************************************************************************


Acad::ErrorStatus 
AsdkPoly::subGetSubentPathsAtGsMarker(AcDb::SubentType     type, 
                                      Adesk::GsMarker      gsMark,
                                      const AcGePoint3d&   pickPoint,
                                      const AcGeMatrix3d&  viewXform,
                                      int&                 numPaths, 
                                      AcDbFullSubentPath*& subentPaths,
                                      int                  numInserts,
                                      AcDbObjectId*        entAndInsertStack) const
{
    assertReadEnabled();
    assert(numInserts == 0);
    assert(entAndInsertStack == NULL);

    numPaths    = 0;
    subentPaths = NULL;
    
    switch (type)
    {
    case AcDb::kVertexSubentType:
        {
            if (gsMark == AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex)
            {
                numPaths       = 1;
                subentPaths    = new AcDbFullSubentPath[1];
                subentPaths[0] = AcDbFullSubentPath(objectId(), AcDb::kVertexSubentType, AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex);
                break;
            }
            if (gsMark <= 0)
                return Acad::eInvalidInput;
            numPaths       = 2;
            subentPaths    = new AcDbFullSubentPath[2];
            subentPaths[0] = AcDbFullSubentPath(objectId(), AcDb::kVertexSubentType, gsMark);
            subentPaths[1] = AcDbFullSubentPath(objectId(), AcDb::kVertexSubentType, ((int)gsMark % mNumSides) + 1);
        }
        break;
    case AcDb::kEdgeSubentType:
        {
            if (gsMark <= 0)
                return Acad::eInvalidInput;
            numPaths       = 1;
            subentPaths    = new AcDbFullSubentPath[1];
            subentPaths[0] = AcDbFullSubentPath(objectId(), AcDb::kEdgeSubentType, gsMark);
        }
        break;
    default:
        ;
    }
    return Acad::eOk;
}


Acad::ErrorStatus 
AsdkPoly::subGetGsMarkersAtSubentPath(const AcDbFullSubentPath& path, 
                                      AcDbIntPtrArray&          gsMarkers) const
{
    const AcDbSubentId subEntId = path.subentId();
    const int index = (int)subEntId.index();

    switch (subEntId.type()) 
    {
    case AcDb::kVertexSubentType:
        {
            if (index == AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex)
            {
                gsMarkers.append(AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex);
                break;
            }
            if (index <= 0)
                return Acad::eInvalidIndex;
            // Append Gs marker of the edge starting from the vertex
            // and the edge ending in the vertex
            gsMarkers.append(index);                           
            gsMarkers.append(index == 1 ? numSides() : index-1);
        }
        break;
    case AcDb::kEdgeSubentType:
        {
            if (index <= 0)
                return Acad::eInvalidInput;
            gsMarkers.append(index);
        }
        break;
    default:
        assert(Adesk::kFalse);
    }
    return Acad::eOk;
}


AcDbEntity* 
AsdkPoly::subSubentPtr(const AcDbFullSubentPath& path) const
{
    const AcDbSubentId subEntId = path.subentId();
    const int index = (int)subEntId.index();

    AcGePoint3dArray vertexCoords;
    getVertices3d(vertexCoords);

    switch (subEntId.type()) 
    {
    case AcDb::kVertexSubentType:
        {
            if (!(1 <= index && index <= numSides() || index == AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex))
                return nullptr;

            AcGePoint3d position;
            if (index == AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex)
                getCenter(position);
            else
                position = vertexCoords[index-1];

            AcDbPoint* const pPoint = new AcDbPoint(position);
            pPoint->setNormal(normal());
            pPoint->setPropertiesFrom(this);
            return pPoint;
        }
    case AcDb::kEdgeSubentType:
        {
            if (index == AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex)
            {
                // Pseudo edge subentity at the center 
                //
                AcGePoint3d cen;
                getCenter(cen);
                const double radius = center().distanceTo(startPoint());
                AcDbCircle* pCircle = new AcDbCircle(cen, normal(), radius);
                pCircle->setPropertiesFrom(this);
                return pCircle;
            }
            else
            {
                if (!(1 <= index && index <= numSides()))
                    return nullptr;

                AcDbLine* const pLine = new AcDbLine(vertexCoords[index-1], vertexCoords[index % numSides()]);
                pLine->setNormal(normal());
                pLine->setPropertiesFrom(this);
                return pLine;
            }
        }        
    default:
        assert(Adesk::kFalse);
    }
    return nullptr;
}


// THE FOLLOWING CODE APPEARS IN THE SDK DOCUMENT.

Acad::ErrorStatus
AsdkPoly::subGetOsnapPoints(
    AcDb::OsnapMode       osnapMode,
    Adesk::GsMarker       gsSelectionMark,
    const AcGePoint3d&    pickPoint,
    const AcGePoint3d&    lastPoint,
    const AcGeMatrix3d&   viewXform,
    AcGePoint3dArray&     snapPoints,
    AcDbIntArray&         /*geomIds*/) const
{

    assertReadEnabled();
    Acad::ErrorStatus es = Acad::eOk;

    if (gsSelectionMark == 0)
        return Acad::eOk;

    if (   osnapMode != AcDb::kOsModeEnd
        && osnapMode != AcDb::kOsModeMid
        && osnapMode != AcDb::kOsModeNear
        && osnapMode != AcDb::kOsModePerp
        && osnapMode != AcDb::kOsModeCen
        && osnapMode != AcDb::kOsModeIns)
    {
        return Acad::eOk;
    }

    // First, check to see if the gsSelection marker is the
    // text geometry. If so, handle center and insertion 
    // modes, then return. No need to go into perp, mid, etc.
    //
    AcGePoint3d center;
    getCenter(center);
    if (gsSelectionMark == AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex) {
        if (osnapMode == AcDb::kOsModeIns)
            snapPoints.append(center);
        else if (osnapMode == AcDb::kOsModeCen)
            snapPoints.append(center);
        
        return es;
    }

    if (!(1 <= gsSelectionMark && gsSelectionMark <= numSides()))
    {
        assert(!"gsSelectionMark out of range");
        return Acad::eInvalidInput;
    }

    int startIndex = (int)(gsSelectionMark - 1);

    AcGePoint3dArray vertexArray;
    if ((es = getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcGeLineSeg3d lnsg(vertexArray[startIndex],
            vertexArray[startIndex + 1]);
    AcGePoint3d pt;

    AcGeLine3d line, perpLine;
    AcGeVector3d vec;

    AcGeVector3d viewDir(viewXform(Z, 0), viewXform(Z, 1),
            viewXform(Z, 2));

    switch (osnapMode) {
    case AcDb::kOsModeEnd:
        snapPoints.append(vertexArray[startIndex]);
        snapPoints.append(vertexArray[startIndex + 1]);
        break;

    case AcDb::kOsModeMid:
        pt.set(
            ((vertexArray[startIndex])[X]
                + (vertexArray[startIndex + 1])[X]) * 0.5,
            ((vertexArray[startIndex])[Y]
                + (vertexArray[startIndex + 1])[Y]) * 0.5,
            ((vertexArray[startIndex])[Z]
                + (vertexArray[startIndex + 1])[Z]) * 0.5);
        snapPoints.append(pt);
        break;

    case AcDb::kOsModeNear:
        pt = lnsg.projClosestPointTo(pickPoint, viewDir);
        snapPoints.append(pt);
        break;

    case AcDb::kOsModePerp:

        // Create a semi-infinite line and find a point on it.
        //
        vec = vertexArray[startIndex + 1]
            - vertexArray[startIndex];
        vec.normalize();
        line.set(vertexArray[startIndex], vec);
        pt = line.closestPointTo(lastPoint);
        snapPoints.append(pt);
        break;

    case AcDb::kOsModeCen:
        snapPoints.append(center);
        break;

    default:
        return Acad::eOk;

    }
    return es;
}

Acad::ErrorStatus
AsdkPoly::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray& osnapModes,
    AcDbIntArray& geomIds) const
{
    assertReadEnabled();
    Acad::ErrorStatus es;
    if ((es = getVertices3d(gripPoints)) != Acad::eOk)
        return es;

    // Remove the duplicate point at the start/end and add
    // center as the last point.
    //
    gripPoints.removeAt(gripPoints.length() - 1);
    AcGePoint3d center;
    getCenter(center);
    gripPoints.append(center);
    return es;
}

Acad::ErrorStatus
AsdkPoly::subMoveGripPointsAt(
    const AcDbIntArray& indices,
    const AcGeVector3d& offset)
{
    if (indices.length()== 0 || offset.isZeroLength())
        return Acad::eOk; //that's easy :-)

    if (mDragDataFlags & kCloneMeForDraggingCalled) {
        mDragDataFlags |= kUseDragCache;
    } else
        // Only if we're not dragging do we want to make an undo
        // recording and check if the object's open for write.
        //
        assertWriteEnabled();

    //if there more than one hot vertex or there's one and it is the center
    //then simply transform
    if (indices.length()>1 || indices[0] == mNumSides)
        return transformBy(AcGeMatrix3d::translation(offset));
        
    AcGeVector3d off(offset);
    //calculate the offset vector of the startpoint
    //from the offset vector on a vertex
    double rotateBy = 2.0 * 3.14159265358979323846 / mNumSides * indices[0];
    AcGePoint3d cent;
    getCenter(cent);
    off.transformBy(AcGeMatrix3d::rotation(-rotateBy,normal(),cent));
    acdbWcs2Ecs(asDblArray(off),asDblArray(off),asDblArray(normal()),Adesk::kTrue);
    if (mDragDataFlags & kUseDragCache){
        mDragCenter = mCenter;
        mDragPlaneNormal = mPlaneNormal;
        mDragStartPoint = mStartPoint + AcGeVector2d(off.x,off.y);
        mDragElevation = mElevation + off.z;
    }else{
        mStartPoint = mStartPoint + AcGeVector2d(off.x,off.y);
        mElevation = mElevation + off.z;
    }

    if (off.length() > 1e-6)
    {
        sPolyAssocPersSubentIdPE.notifyEntityChangedShape(this); // For Constraints
    }
    return Acad::eOk;
}

// Stretch points are the same as grip points except
// for the center of the polygon.
//
Acad::ErrorStatus
AsdkPoly::subGetStretchPoints(
    AcGePoint3dArray& stretchPoints) const
{
    assertReadEnabled();
    Acad::ErrorStatus es;

    if ((es = getVertices3d(stretchPoints))
        != Acad::eOk)
    {
        return es;
    }

    // Remove the duplicate point at the start and end.
    //
    stretchPoints.removeAt(stretchPoints.length() - 1);
    return es;
}

Acad::ErrorStatus
AsdkPoly::subMoveStretchPointsAt(
    const AcDbIntArray& indices,
    const AcGeVector3d& offset)
{
    return moveGripPointsAt(indices, offset);
}

// Transform points and create a polyline out of them.
//
Acad::ErrorStatus AsdkPoly::subGetTransformedCopy(
    const AcGeMatrix3d& mat,
    AcDbEntity*& ent) const
{
    assertReadEnabled();
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = getVertices3d(vertexArray))
        != Acad::eOk)
    {
        return es;
    }

    for (int i = 0; i < vertexArray.length(); i++) {
        vertexArray[i].transformBy(mat);
    }

    AcDbSpline *pSpline = NULL;
    if ((es = rx_makeSpline(vertexArray, pSpline))
        != Acad::eOk)
    {
        return es;
    }
    assert(pSpline != NULL);
    pSpline->setPropertiesFrom(this);

    ent = pSpline;
    return es;
}

Acad::ErrorStatus
AsdkPoly::subIntersectWith(
    const AcDbEntity* ent,
    AcDb::Intersect intType,
    AcGePoint3dArray& points,
    Adesk::GsMarker /*thisGsMarker*/,
    Adesk::GsMarker /*otherGsMarker*/) const
{
    assertReadEnabled();
    Acad::ErrorStatus es = Acad::eOk;
    if (ent == NULL)
        return Acad::eNullEntityPointer;

    // The idea is to intersect each side of the polygon
    // with the given entity and return all the points.
    // 
    // For non-R12-entities, with
    // intersection methods defined, we call that method for
    // each of the sides of the polygon. For R12-entities,
    // we use the locally defined intersectors, since their
    // protocols are not implemented.
    //
    if (ent->isKindOf(AcDbLine::desc())) {
        if ((es = intLine(this, AcDbLine::cast(ent),
            intType, NULL, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDbArc::desc())) {
        if ((es = intArc(this, AcDbArc::cast(ent), intType,
            NULL, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDbCircle::desc())) {
        if ((es = intCircle(this, AcDbCircle::cast(ent),
            intType, NULL, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDb2dPolyline::desc())) {
        if ((es = intPline(this, AcDb2dPolyline::cast(ent),
            intType, NULL, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDb3dPolyline::desc())) {
        if ((es = intPline(this, AcDb3dPolyline::cast(ent),
            intType, NULL, points)) != Acad::eOk)
        {
            return es;
        }
    } else {
        AcGePoint3dArray vertexArray;
        if ((es = getVertices3d(vertexArray))
            != Acad::eOk)
        {
            return es;
        }
        if (intType == AcDb::kExtendArg
            || intType == AcDb::kExtendBoth)
        {
            intType = AcDb::kExtendThis;
        }

        AcDbLine *pAcadLine;
        for (int i = 0; i < vertexArray.length() - 1; i++) {
            pAcadLine = new AcDbLine();
            pAcadLine->setStartPoint(vertexArray[i]);
            pAcadLine->setEndPoint(vertexArray[i + 1]);
            pAcadLine->setNormal(normal());

            if ((es = ent->intersectWith(pAcadLine, intType,
                points)) != Acad::eOk)
            {
                delete pAcadLine;
                return es;
            }
            delete pAcadLine;
        }
    }
    return es;
}

Acad::ErrorStatus
AsdkPoly::subIntersectWith(
    const AcDbEntity* ent,
    AcDb::Intersect intType,
    const AcGePlane& projPlane,
    AcGePoint3dArray& points,
    Adesk::GsMarker /*thisGsMarker*/,
    Adesk::GsMarker /*otherGsMarker*/) const
{
    assertReadEnabled();
    Acad::ErrorStatus es = Acad::eOk;
    if (ent == NULL)
        return Acad::eNullEntityPointer;

    // The idea is to intersect each side of the polygon
    // with the given entity and return all the points.
    // 
    // For non-R12-entities, with
    // intersection methods defined, we call that method for
    // each of the sides of the polygon. For R12-entities,
    // we use the locally defined intersectors, since their
    // protocols are not implemented.
    //

    if (ent->isKindOf(AcDbLine::desc())) {
        if ((es = intLine(this, AcDbLine::cast(ent),
            intType, &projPlane, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDbArc::desc())) {
        if ((es = intArc(this, AcDbArc::cast(ent), intType,
            &projPlane, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDbCircle::desc())) {
        if ((es = intCircle(this, AcDbCircle::cast(ent),
            intType, &projPlane, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDb2dPolyline::desc())) {
        if ((es = intPline(this, AcDb2dPolyline::cast(ent),
            intType, &projPlane, points)) != Acad::eOk)
        {
            return es;
        }
    } else if (ent->isKindOf(AcDb3dPolyline::desc())) {
        if ((es = intPline(this, AcDb3dPolyline::cast(ent),
            intType, &projPlane, points)) != Acad::eOk)
        {
            return es;
        }
    } else {
        AcGePoint3dArray vertexArray;
        if ((es = getVertices3d(vertexArray))
            != Acad::eOk)
        {
            return es;
        }
        if (intType == AcDb::kExtendArg
            || intType == AcDb::kExtendBoth)
        {
            intType = AcDb::kExtendThis;
        }

        AcDbLine *pAcadLine;
        int i;
        for (i = 0; i < vertexArray.length() - 1; i++) {
            pAcadLine = new AcDbLine();
            pAcadLine->setStartPoint(vertexArray[i]);
            pAcadLine->setEndPoint(vertexArray[i + 1]);
            pAcadLine->setNormal(normal());

            if ((es = ent->intersectWith(pAcadLine, intType,
                projPlane, points)) != Acad::eOk)
            {
                delete pAcadLine;
                return es;
            }
            delete pAcadLine;
        }

        // All the points that we selected in this process are on
        // the other curve; we are dealing with apparent
        // intersection. If the other curve is 3D or is not
        // on the same plane as poly, the points are not on
        // poly.
        // 
        // In this case, we need to do some more work. Project the
        // points back onto the plane. They should lie on
        // the projected poly. Find points on real poly
        // corresponding to the projected points.

        AcGePoint3d projPt, planePt;
        AcGePoint3dArray pts;
        AcGeLine3d line;
        
        AcGePlane polyPlane;
        AcDb::Planarity plnrty;
        getPlane(polyPlane,plnrty);

        for (i = 0; i < points.length(); i++) {

            // Define a line starting from the projPt and
            // along the normal.  Intersect the polygon with
            // that line. Find all the points and pick the
            // one closest to the given point.
            //

            projPt = points[i].orthoProject(projPlane);
            line.set(projPt, projPlane.normal());
            if ((es = intLine(this, line, pts))
                != Acad::eOk)
            {
                return es;
            }

            planePt = projPt.project(polyPlane,
                projPlane.normal());
            points[i] = pts[0];
            double length = (planePt - pts[0]).length();
            double length2;

            for (int j = 1; j < pts.length(); j++) {
                if ((length2 = (planePt - pts[j]).length())
                    < length)
                {
                    points[i] = pts[j];
                    length = length2;
                }
            }
        }
    }
    return es;
}

// END CODE APPEARING IN SDK DOCUMENT.

void            
AsdkPoly::subList() const
{
    assertReadEnabled();

    AcDbEntity::subList();

    double circumRadius = (startPoint() - center()).length();
    double sideLength = 2.0 * circumRadius * cos(3.14159265358979323846 * 0.5 - 3.14159265358979323846 / mNumSides);

    acutPrintf(_T("%18s%16s %d \n"), _T(/*MSG0*/""), _T("Number Of Sides:"), mNumSides);

    acutPrintf(_T("%18s%16s %-9.16q0 \n"), 
        _T(/*MSG0*/""), _T("Circumference:"), sideLength * mNumSides);

    double areaVal;
    getArea(areaVal);
    acutPrintf(_T("%18s%16s %-9.16q0 \n"), _T(/*MSG0*/""), _T("Area:"), areaVal);
    
    acutPrintf(_T("%18s%16s "), _T(/*MSG0*/""), _T("Center:"));

    AcGePoint3d cent(center().x,center().y,elevation());
    acdbEcs2Ucs(asDblArray(cent), asDblArray(cent), asDblArray(normal()),Adesk::kFalse);
    acutPrintf(_T("X = %-9.16q0, Y = %-9.16q0, Z = %-9.16q0\n"), 
                      cent.x, cent.y, cent.z);

    AcGePoint3d sp(startPoint().x,startPoint().y,elevation());
    acdbEcs2Ucs(asDblArray(sp), asDblArray(sp), asDblArray(normal()),Adesk::kFalse);
    acutPrintf(_T("%18s%16s "), _T(/*MSG0*/""), _T("Start Point:"));
    acutPrintf(_T("X = %-9.16q0, Y = %-9.16q0, Z = %-9.16q0\n"), 
               sp.x, sp.y, sp.z);

    AcGeVector3d norm(normal());
    acdbWcs2Ucs(asDblArray(norm), asDblArray(norm), Adesk::kTrue);
    acutPrintf(_T("%18s%16s "), _T(/*MSG0*/""), _T("Normal:"));
    acutPrintf(_T("X = %-9.16q0, Y = %-9.16q0, Z = %-9.16q0\n"), 
                      norm.x, norm.y, norm.z);

}

// THE FOLLOWING CODE APPEARS IN THE SDK DOCUMENT.

Acad::ErrorStatus
AsdkPoly::subTransformBy(const AcGeMatrix3d& xform)
{
    // If we're dragging, we aren't really going to change our
    // data, so we don't want to make an undo recording nor do
    // we really care if the object's open for write.
    //
    if (mDragDataFlags & kCloneMeForDraggingCalled) {
        mDragDataFlags |= kUseDragCache;
        mDragPlaneNormal = mPlaneNormal;
        mDragElevation = mElevation;
        AcGeMatrix2d xform2d(xform.convertToLocal(mDragPlaneNormal,mDragElevation));
        mDragCenter = mCenter;
        mDragCenter.transformBy(xform2d);
        mDragStartPoint = mStartPoint;
        mDragStartPoint.transformBy(xform2d);
        mDragPlaneNormal.normalize();
    } else {
        assertWriteEnabled();
        AcGeMatrix2d xform2d(xform.convertToLocal(mPlaneNormal,mElevation));
        mCenter.transformBy(xform2d);
        mStartPoint.transformBy(xform2d);
        mPlaneNormal.normalize();
    }

    if (fabs(xform.scale() - 1.0) > 1e-6 || xform.det() < 0.0)
    {
        sPolyAssocPersSubentIdPE.notifyEntityChangedShape(this); // For Constraints
    }
    return Acad::eOk;
}

// END CODE APPEARING IN SDK DOCUMENT.

Acad::ErrorStatus       
AsdkPoly::subExplode(AcDbVoidPtrArray& entitySet) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = Acad::eOk;

    // Exploding a polygon produces a set of lines which
    // are the sides of the polygon.

    AcGePoint3dArray vertexArray;
    if ((es = getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcDbLine* line;

    for (int i = 0; i < vertexArray.length() - 1; i++) {

        line = new AcDbLine();
        line->setStartPoint(vertexArray[i]);
        line->setEndPoint(vertexArray[i + 1]);
        line->setNormal(normal());

        entitySet.append(line);
    }

    AcDbText *text ;

    if ((mpName != NULL) && (mpName[0] != _T('\0')))
    {
        AcGePoint3d center,startPoint;
        getCenter(center);
        getStartPoint(startPoint);
        AcGeVector3d direction = startPoint - center;

        if (mTextStyle != AcDbObjectId::kNull)
            text =new AcDbText (center, mpName, mTextStyle, 0, direction.angleTo (AcGeVector3d (1, 0, 0))) ;
        else
            text =new AcDbText (center, mpName, mTextStyle, direction.length() / 20, direction.angleTo (AcGeVector3d (1, 0, 0))) ;

        entitySet.append (text) ;
    }

    return es;
}

Adesk::Boolean 
AsdkPoly::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();

    if (worldDraw->regenAbort()) {
        //clear the drag flags once we are drawn
        mDragDataFlags = 0;
        return Adesk::kTrue;
    }

    drawName(this, worldDraw);

    switch (worldDraw->regenType()) {
        
    case kAcGiHideOrShadeCommand:
    case kAcGiShadedDisplay:
        
        drawFacets(this, worldDraw, NULL);
        break;
        
    case kAcGiStandardDisplay:
    case kAcGiSaveWorldDrawForR12:
    case kAcGiSaveWorldDrawForProxy:
        
        drawEdges (this, worldDraw, NULL);
        
        break;
        
    default:
        assert(Adesk::kFalse);

    }
    if (mShowAffordances || objectId().isNull())
    {
        AcGePoint3dArray vertexArray;
        if (getVertices3d(vertexArray) == Acad::eOk) {
            for (int i=0;i<vertexArray.length();i++)
                worldDraw->rawGeometry()->circle(vertexArray[i],1,mPlaneNormal);
        }
    }
    //clear the drag flags once we are drawn
    mDragDataFlags = 0;
    return Adesk::kTrue; // Don't call viewportDraw().
}

Adesk::Boolean
AsdkPoly::subCloneMeForDragging()
{
    if (mUseDragData) {
        // let the rest of poly's code know we've been called
        //
        mDragDataFlags |= kCloneMeForDraggingCalled;

        // let AutoCAD know that we don't want to be cloned for dragging
        //
        return Adesk::kFalse;
    } else
        return Adesk::kTrue;
}

void AsdkPoly::dragStatus(const AcDb::DragStat dragStatus)
{
    sPolyAssocPersSubentIdPE.notifyEntityDragStatus(this, dragStatus); // For Constraints
}


//*************************************************************************
// Overridden methods from AcDbObject
//*************************************************************************

// THE FOLLOWING CODE APPEARS IN THE SDK DOCUMENT.

Acad::ErrorStatus 
AsdkPoly::applyPartialUndo(AcDbDwgFiler* filer,
                           AcRxClass*    classObj)
{
    // The first thing to check is whether the class matches with
    // ours. If it doesn't, we call the base class's
    // applyPartialUndo(); hopefully, one of them will
    // take care of it.
    //
    if (classObj != AsdkPoly::desc())
        return AcDbCurve::applyPartialUndo(filer, classObj);

    // Read the op-code and call the appropriate "set"
    // method to undo what was done. The "set" does the
    // filing again for redo.
    //
    Adesk::Int16 shortCode;
    filer->readItem(&shortCode);

    PolyOpCodeForPartialUndo code;
    code = (PolyOpCodeForPartialUndo)shortCode;

    Adesk::UInt32 value32;
    switch (code) {
    case kSetNumSides:
        filer->readItem(&value32);
        setNumSides(value32);
        break;
    default:
        assert(Adesk::kFalse);
        break;
    }
    return Acad::eOk;
}


Acad::ErrorStatus
AsdkPoly::dwgInFields(AcDbDwgFiler* filer)
{
    assertWriteEnabled();
    Acad::ErrorStatus es;

    if ((es = AcDbCurve::dwgInFields(filer)) != Acad::eOk) 
    {
        return es;
    }

    // Object Version - must always be the first item.
    //
    Adesk::Int16 version;
    filer->readItem(&version);
    if (version > VERSION)
      return Acad::eMakeMeProxy;

    switch (version)
    {
    case 1:
    {
        AcGePoint3d center;
        filer->readPoint3d(&center);
        AcGePoint3d startPoint;
        filer->readPoint3d(&startPoint);
        filer->readInt32(&mNumSides);
        filer->readVector3d(&mPlaneNormal);
        acutDelString(mpName);
        filer->readString(&mpName);
        if (mpName[0]==0)
            setName(NULL);
        filer->readHardPointerId(&mTextStyle);
        //convert data from old format
        acdbWcs2Ecs(asDblArray(center),asDblArray(center),asDblArray(mPlaneNormal),Adesk::kFalse);
        mCenter.set(center.x,center.y);
        mElevation = center.z;
        acdbWcs2Ecs(asDblArray(startPoint),asDblArray(startPoint),asDblArray(mPlaneNormal),Adesk::kFalse);
        mStartPoint.set(startPoint.x,startPoint.y);
        assert(mElevation == startPoint.z);
        break;
    }
    case 2:
    {
        filer->readPoint2d(&mCenter);
        filer->readPoint2d(&mStartPoint);
        filer->readInt32(&mNumSides);
        filer->readVector3d(&mPlaneNormal);
        acutDelString(mpName);
        filer->readString(&mpName);
        filer->readHardPointerId(&mTextStyle);
        filer->readDouble(&mElevation);
        break;
    }
    default:
        assert(false);
    }
    return filer->filerStatus();
}

Acad::ErrorStatus
AsdkPoly::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    Acad::ErrorStatus es;

    if ((es = AcDbCurve::dwgOutFields(filer))
        != Acad::eOk)
    {
        return es;
    }

    // Object Version - must always be the first item.
    // 
    Adesk::Int16 version = VERSION;
    filer->writeItem(version);

    filer->writePoint2d(mCenter);
    filer->writePoint2d(mStartPoint);
    filer->writeInt32(mNumSides);
    filer->writeVector3d(mPlaneNormal);
    if (mpName)
        filer->writeString(mpName);
    else
        filer->writeString(_T(""));

    // mTextStyle is a hard pointer id, so filing it out to
    // the purge filer (kPurgeFiler) prevents purging of
    // this object.
    //
    filer->writeHardPointerId(mTextStyle);
    filer->writeDouble(mElevation);
    return filer->filerStatus();
}


#ifdef ORDER_DEPENDENT

Acad::ErrorStatus
AsdkPoly::dxfInFields(AcDbDxfFiler* filer)
{
    assertWriteEnabled();
    
    if ((AcDbCurve::dxfInFields(filer) != Acad::eOk) ||
        !filer->atSubclassData(_T("AsdkPoly")) )
    {
        return filer->filerStatus();
    }
    
    try
    {
        resbuf rb;
        
        // Object Version
        Adesk::Int16 version;
        filer->readItem(&rb);
        if (rb.restype != AcDb::kDxfInt16) 
            throw AcDb::kDxfInt16;
            
        version = rb.resval.rint;
        if (version > VERSION)
            return Acad::eMakeMeProxy;
        
        if (version == 1)
        {
            AcGePoint3d cent,sp;
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfXCoord) 
                throw AcDb::kDxfXCoord
                cent = asPnt3d(rb.resval.rpoint);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfXCoord + 1) 
                throw AcDb::kDxfXCoord + 1;
            sp = asPnt3d(rb.resval.rpoint);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfInt32) 
                throw AcDb::kDxfInt32;
            mNumSides = rb.resval.rlong;
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfNormalX) 
                throw AcDb::kDxfNormalX
                mPlaneNormal = asVec3d(rb.resval.rpoint);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfText) 
                throw AcDb::kDxfText;
            setName(rb.resval.rstring);
            
            filer->readItem(&rb);
            if (rb.restype != kDxfHardPointerId) 
                throw AcDb::kDxfHardPointerId;
            acdbGetObjectId(mTextStyle, rb.resval.rlname);
            
            //convert data from old format
            acdbWcs2Ecs(asDblArray(cent),asDblArray(cent),asDblArray(mPlaneNormal),Adesk::kFalse);
            mCenter.set(cent.x,cent.y);
            mElevation = cent.z;
            acdbWcs2Ecs(asDblArray(sp),asDblArray(sp),asDblArray(mPlaneNormal),Adesk::kFalse);
            mStartPoint.set(sp.x,sp.y);
            assert(mElevation == sp.z);
        } 
        else if (version == 2)
        {
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfXCoord) 
                throw AcDb::kDxfXCoord;
            mCenter = asPnt2d(rb.resval.rpoint);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfXCoord + 1) 
                throw AcDb::kDxfXCoord + 1;
            mStartPoint = asPnt2d(rb.resval.rpoint);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfInt32) 
                throw AcDb::kDxfInt32
                mNumSides = rb.resval.rlong;
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfNormalX)
                throw AcDb::kDxfNormalX;
            mPlaneNormal = asVec3d(rb.resval.rpoint);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfText) 
                throw AcDb::kDxfText
                setName(rb.resval.rstring);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfHardPointerId) 
                throw AcDb::kDxfHardPointerId;
            acdbGetObjectId(mTextStyle, rb.resval.rlname);
            
            filer->readItem(&rb);
            if (rb.restype != AcDb::kDxfReal) 
                throw AcDb::kDxfReal;
            mElevation = rb.resval.rreal;
        } 
        else assert(false);
    }
    catch (AcDb::DxfCode code)
    {
        filer->pushBackItem();
        filer->setError(Acad::eInvalidDxfCode,
            _T("\nError: expected group code %d"), code);
        return filer->filerStatus();
    }
}
#else

Acad::ErrorStatus
AsdkPoly::dxfInFields(AcDbDxfFiler* filer)
{
    assertWriteEnabled();
    Acad::ErrorStatus es = Acad::eOk;
    resbuf rb;

    if ((AcDbCurve::dxfInFields(filer) != Acad::eOk)
        || !filer->atSubclassData(_T("AsdkPoly")))
    {
        return filer->filerStatus();
    }

    // Object Version
    Adesk::Int16 version;
    filer->readItem(&rb);
    if (rb.restype != AcDb::kDxfInt16) 
    {
        filer->pushBackItem();
        filer->setError(Acad::eInvalidDxfCode,
            _T("\nError: expected group code %d (version)"), AcDb::kDxfInt16);
        return filer->filerStatus();
    }
    version = rb.resval.rint;
    if (version > VERSION)
        return Acad::eMakeMeProxy;
    AcGePoint3d cen3d,sp3d;
    AcGePoint2d cen2d,sp2d;
    long numSides = 0;
    AcDbObjectId textStyle;
    double elevation = 0.0;
    Adesk::UInt32 fieldsFlags = 0;
    TCHAR * pName = NULL;
    AcGeVector3d planeNormal;
    while ((es == Acad::eOk)
        && ((es = filer->readResBuf(&rb)) == Acad::eOk))
    {
        switch (rb.restype) {

        case AcDb::kDxfXCoord:
            if (version == 1)
                cen3d = asPnt3d(rb.resval.rpoint);
            else 
                cen2d = asPnt2d(rb.resval.rpoint);
            fieldsFlags |= 0x1;
            break;

        case AcDb::kDxfXCoord + 1:
            if (version == 1)
                sp3d = asPnt3d(rb.resval.rpoint);
            else
                sp2d = asPnt2d(rb.resval.rpoint);
            fieldsFlags |= 0x2;
            break;

        case AcDb::kDxfInt32:
            numSides = rb.resval.rlong;
            fieldsFlags |= 0x4;
            break;
        case AcDb::kDxfNormalX:
            planeNormal = asVec3d(rb.resval.rpoint);
            fieldsFlags |= 0x8;
            break;

        case AcDb::kDxfText:
            acutUpdString(rb.resval.rstring,pName);
            fieldsFlags |= 0x11;
            break;

        case AcDb::kDxfHardPointerId:
            acdbGetObjectId(textStyle, rb.resval.rlname);
            fieldsFlags |= 0x12;
            break;
        case AcDb::kDxfReal:
            if (version == 2)
            {
                fieldsFlags |= 0x10;
                elevation = rb.resval.rreal;
                break;    
            }
            //fall through intentional
        default:
            // An unrecognized group. Push it back so that
            // the subclass can read it again.
            filer->pushBackItem();
            es = Acad::eEndOfFile;
            break;
        }
    }

    // At this point the es variable must contain eEndOfFile
    // - either from readResBuf() or from pushback. If not,
    // it indicates that an error happened and we should
    // return immediately.
    //
    if (es != Acad::eEndOfFile)
        return Acad::eInvalidResBuf;

    // Now check to be sure all necessary group codes were present.
    //
    // Mandatory fields:
    // - center
    // - start point
    // - normal
    // - number of sides
    // - elevation (if version > 1)
    short required[]  = 
        {AcDb::kDxfXCoord, AcDb::kDxfXCoord+1, AcDb::kDxfInt32,
        AcDb::kDxfNormalX, AcDb::kDxfReal};
    
    for (short i = 0; i < (version>1?4:3); i++) {
        if (!fieldsFlags & 0x1) {
            filer->setError(Acad::eMissingDxfField,
                _T("\nMissing DXF group code: %d"), 2, required[i]);
            return Acad::eMissingDxfField;
        } else
            fieldsFlags >>= 1;
    }
    mPlaneNormal = planeNormal;
    mNumSides = numSides;
    mTextStyle = textStyle;
    setName(pName);
    acutDelString(pName);
    if (version==1)
    {
        //convert data from old format
        acdbWcs2Ecs(asDblArray(cen3d),asDblArray(cen3d),asDblArray(planeNormal),Adesk::kFalse);
        mCenter.set(cen3d.x,cen3d.y);
        mElevation = cen3d.z;
        acdbWcs2Ecs(asDblArray(sp3d),asDblArray(sp3d),asDblArray(planeNormal),Adesk::kFalse);
        mStartPoint.set(sp3d.x,sp3d.y);
        assert(mElevation == sp3d.z);
    } else {
        mCenter = cen2d;
        mStartPoint = sp2d;
        mElevation = elevation;
    }
    return es;
}

#endif // ORDER_DEPENDENT 


Acad::ErrorStatus
AsdkPoly::dxfOutFields(AcDbDxfFiler* filer) const
{
    assertReadEnabled();
    Acad::ErrorStatus es;

    if ((es = AcDbCurve::dxfOutFields(filer))
        != Acad::eOk)
    {
        return es;
    }
    filer->writeItem(AcDb::kDxfSubclass, _T("AsdkPoly"));

    // Object Version
    //
    Adesk::Int16 version = VERSION;
    filer->writeInt16(AcDb::kDxfInt16, version);
    filer->writePoint2d(AcDb::kDxfXCoord, mCenter);
    filer->writePoint2d(AcDb::kDxfXCoord + 1, mStartPoint);
    filer->writeInt32(AcDb::kDxfInt32, mNumSides);
    //always use max precision when writing out the normal
    filer->writeVector3d(AcDb::kDxfNormalX, mPlaneNormal,16);
    filer->writeString(AcDb::kDxfText, mpName);
    filer->writeItem(AcDb::kDxfHardPointerId, mTextStyle);
    filer->writeDouble(AcDb::kDxfReal, mElevation);
    return filer->filerStatus();
}


Acad::ErrorStatus
AsdkPoly::subDeepClone(AcDbObject*    pOwner,
                    AcDbObject*&   pClonedObject,
                    AcDbIdMapping& idMap,
                    Adesk::Boolean isPrimary) const
{
    // You should always pass back pClonedObject == NULL
    // if, for any reason, you do not actually clone it
    // during this call.  The caller should pass it in
    // as NULL, but to be safe, we set it here as well.
    //
    pClonedObject = NULL;

    // If this object is in the idMap and is already
    // cloned, then return.
    //
    bool isPrim = false;
    if (isPrimary)
        isPrim = true;
    AcDbIdPair idPair(objectId(), (AcDbObjectId)NULL,
                      false, isPrim);
    if (idMap.compute(idPair) && (idPair.value() != NULL))
        return Acad::eOk;    

    // Create the clone
    //
    AsdkPoly *pClone = (AsdkPoly*)isA()->create();
    if (pClone != NULL)
        pClonedObject = pClone;    // set the return value
    else
        return Acad::eOutOfMemory;

    AcDbDeepCloneFiler filer;
    dwgOut(&filer);

    filer.seek(0L, AcDb::kSeekFromStart);
    pClone->dwgIn(&filer);
    bool bOwnerXlated = false;
    if (isPrimary)
    {
        AcDbBlockTableRecord *pBTR =
            AcDbBlockTableRecord::cast(pOwner);
        if (pBTR != NULL)
        {
            pBTR->appendAcDbEntity(pClone);
            bOwnerXlated = true;
        }
        else
        {
            pOwner->database()->addAcDbObject(pClone);
        }
    } else {
        pOwner->database()->addAcDbObject(pClone);
        pClone->setOwnerId(pOwner->objectId());
        bOwnerXlated = true;
    }

    // This must be called for all newly created objects
    // in deepClone.  It is turned off by endDeepClone()
    // after it has translated the references to their
    // new values.
    //
    pClone->setAcDbObjectIdsInFlux();
    pClone->disableUndoRecording(true);


    // Add the new information to the idMap.  We can use
    // the idPair started above.
    //
    idPair.setValue(pClonedObject->objectId());
    idPair.setIsCloned(Adesk::kTrue);
    idPair.setIsOwnerXlated(bOwnerXlated);
    idMap.assign(idPair);

    // Using the filer list created above, find and clone
    // any owned objects.
    //
    AcDbObjectId id;
    while (filer.getNextOwnedObject(id)) {

        AcDbObject *pSubObject;
        AcDbObject *pClonedSubObject;

        // Some object's references may be set to NULL, 
        // so don't try to clone them.
        //
        if (id == NULL)
            continue;

        // Open the object and clone it.  Note that we now
        // set "isPrimary" to kFalse here because the object
        // is being cloned, not as part of the primary set,
        // but because it is owned by something in the
        // primary set.
        //
        acdbOpenAcDbObject(pSubObject, id, AcDb::kForRead);
        pClonedSubObject = NULL;
        pSubObject->deepClone(pClonedObject,
                              pClonedSubObject,
                              idMap, Adesk::kFalse);

        // If this is a kDcInsert context, the objects
        // may be "cheapCloned".  In this case, they are
        // "moved" instead of cloned.  The result is that
        // pSubObject and pClonedSubObject will point to
        // the same object.  So, we only want to close
        // pSubObject if it really is a different object
        // than its clone.
        //
        if (pSubObject != pClonedSubObject)
            pSubObject->close();
        
        // The pSubObject may either already have been
        // cloned, or for some reason has chosen not to be
        // cloned.  In that case, the returned pointer will
        // be NULL.  Otherwise, since we have no immediate
        // use for it now, we can close the clone.
        //
        if (pClonedSubObject != NULL)
            pClonedSubObject->close();
    }

    // Leave pClonedObject open for the caller
    //
    return Acad::eOk;
}


Acad::ErrorStatus
AsdkPoly::subWblockClone(AcRxObject*    pOwner,
                      AcDbObject*&   pClonedObject,
                      AcDbIdMapping& idMap,
                      Adesk::Boolean isPrimary) const
{
    // You should always pass back pClonedObject == NULL
    // if, for any reason, you do not actually clone it
    // during this call.  The caller should pass it in
    // as NULL, but to be safe, we set it here as well.
    //
    pClonedObject = NULL;

    // If this is a fast wblock operation then no cloning
    // should take place, so we simply call the base class's
    // wblockClone() and return whatever it returns.
    //
    // For fast wblock, the source and destination databases
    // are the same, so we can use that as the test to see
    // if a fast wblock is in progress.
    //
    AcDbDatabase *pDest, *pOrig;
    idMap.destDb(pDest);
    idMap.origDb(pOrig);
    if (pDest == pOrig)
        return AcDbCurve::subWblockClone(pOwner, pClonedObject,
            idMap, isPrimary);

    // If this is an Xref bind operation and this AsdkPoly
    // entity is in Paper Space,  then we don't want to
    // clone because Xref bind doesn't support cloning
    // entities in Paper Space.  So we simply return
    // Acad::eOk
    //
    AcDbObjectId pspace;
    AcDbBlockTable *pTable;
    database()->getSymbolTable(pTable, AcDb::kForRead);
    pTable->getAt(ACDB_PAPER_SPACE, pspace);
    pTable->close(); 

    if (   idMap.deepCloneContext() == AcDb::kDcXrefBind
        && ownerId() == pspace)
        return Acad::eOk;
    
    // If this object is in the idMap and is already
    // cloned, then return.
    //
    bool isPrim = false;
    if (isPrimary)
        isPrim = true;

    AcDbIdPair idPair(objectId(), (AcDbObjectId)NULL,
                      false, isPrim);
    if (idMap.compute(idPair) && (idPair.value() != NULL))
        return Acad::eOk;    

    // The owner object can be either an AcDbObject, or an
    // AcDbDatabase.  AcDbDatabase is used if the caller is
    // not the owner of the object being cloned (because it
    // is being cloned as part of an AcDbHardPointerId
    // reference).  In this case, the correct ownership
    // will be set during reference translation.  So, if
    // the owner is an AcDbDatabase, then pOwn will be left
    // NULL here, and is used as a "flag" later.
    //

    AcDbObject   *pOwn = AcDbObject::cast(pOwner);
    AcDbDatabase *pDb = AcDbDatabase::cast(pOwner);
    if (pDb == NULL) 
        pDb = pOwn->database();

    // STEP 1:
    // Create the clone
    //
    AsdkPoly *pClone = (AsdkPoly*)isA()->create();
    if (pClone != NULL)
        pClonedObject = pClone;    // set the return value
    else
        return Acad::eOutOfMemory;

    // STEP 2:
    // If the owner is an AcDbBlockTableRecord, go ahead
    // and append the clone.  If not, but we know who the
    // owner is, set the clone's ownerId to it.  Otherwise,
    // we set the clone's ownerId to our own ownerId (in
    // other words, the original owner Id).  This Id will
    // then be used later, in reference translation, as
    // a key to finding who the new owner should be.  This
    // means that the original owner must also be cloned at
    // some point during the wblock operation. 
    // EndDeepClone's reference translation aborts if the
    // owner is not found in the idMap.
    //
    // The most common situation where this happens is
    // AcDbEntity references to Symbol Table Records, such
    // as the Layer an Entity is on.  This is when you will
    // have to pass in the destination database as the owner
    // of the Layer Table Record.  Since all Symbol Tables
    // are always cloned in Wblock, you do not need to make
    // sure that Symbol Table Record owners are cloned. 
    //
    // However, if the owner is one of your own classes,
    // then it is up to you to make sure that it gets
    // cloned.  This is probably easiest to do at the end
    // of this function.  Otherwise you may have problems
    // with recursion when the owner, in turn, attempts
    // to clone this object as one of its subObjects.
    // 
    AcDbBlockTableRecord *pBTR = NULL;
    if (pOwn != NULL)
        pBTR = AcDbBlockTableRecord::cast(pOwn);
    if (pBTR != NULL && isPrimary) {
        pBTR->appendAcDbEntity(pClone);
    } else {
        pDb->addAcDbObject(pClonedObject);
    }

    // STEP 3:
    // The AcDbWblockCloneFiler makes a list of
    // AcDbHardOwnershipIds and AcDbHardPointerIds.  These
    // are the references which must be cloned during a
    // Wblock operation.
    //
    AcDbWblockCloneFiler filer;
    dwgOut(&filer);

    // STEP 4:
    // Rewind the filer and read the data into the clone.
    //
    filer.seek(0L, AcDb::kSeekFromStart);
    pClone->dwgIn(&filer);

    idMap.assign(AcDbIdPair(objectId(), pClonedObject->objectId(), Adesk::kTrue,
        isPrim, (Adesk::Boolean)(pOwn != NULL) ));

   pClonedObject->setOwnerId((pOwn != NULL) ?
        pOwn->objectId() : ownerId());

    // STEP 5:
    // This must be called for all newly created objects
    // in wblockClone.  It is turned off by endDeepClone()
    // after it has translated the references to their
    // new values.
    //
    pClone->setAcDbObjectIdsInFlux();

    // STEP 6:
    // Add the new information to the idMap.  We can use
    // the idPair started above.  We must also let the
    // idMap entry know whether the clone's owner is
    // correct, or needs to be translated later.
    //

    // STEP 7:
    // Using the filer list created above, find and clone
    // any hard references.
    //
    AcDbObjectId id;
    while (filer.getNextHardObject(id)) {

        AcDbObject *pSubObject;
        AcDbObject *pClonedSubObject;

        // Some object's references may be set to NULL, 
        // so don't try to clone them.
        //
        if (id == NULL)
            continue;

        // If the referenced object is from a different
        // database, such as an xref, do not clone it.
        //
        acdbOpenAcDbObject(pSubObject, id, AcDb::kForRead);
        if (pSubObject->database() != database()) {
            pSubObject->close();
            continue;
        }

        // We can find out if this is an AcDbHardPointerId
        // verses an AcDbHardOwnershipId, by seeing if we
        // are the owner of the pSubObject.  If we are not,
        // then we cannot pass our clone in as the owner
        // for the pSubObject's clone.  In that case, we
        // pass in our clone's database (the destination
        // database).
        // 
        // Note that we now set "isPrimary" to kFalse here
        // because the object is being cloned, not as part
        // of the primary set, but because it is owned by
        // something in the primary set.
        //
        pClonedSubObject = NULL;
        if (pSubObject->ownerId() == objectId()) {
            pSubObject->wblockClone(pClone,
                                    pClonedSubObject,
                                    idMap, Adesk::kFalse);
        } else {
            pSubObject->wblockClone(pClone->database(),
                                    pClonedSubObject,
                                    idMap, Adesk::kFalse);
        }
        pSubObject->close();
        
        // The pSubObject may either already have been
        // cloned, or for some reason has chosen not to be
        // cloned.  In that case, the returned pointer will
        // be NULL.  Otherwise, since we have no immediate
        // use for it now, we can close the clone.
        //
        if (pClonedSubObject != NULL)
            pClonedSubObject->close();
    }

    // Leave pClonedObject open for the caller.
    //
    return Acad::eOk;
}


//return the CLSID of the class here
Acad::ErrorStatus   AsdkPoly::subGetClassID(CLSID* pClsid) const
{
    assertReadEnabled();
    //*pClsid = CLSID_ComPolygon;
    return Acad::eNotImplementedYet;
}
// END CODE APPEARING IN SDK DOCUMENT.

void
AsdkPoly::setUseDragData(bool use)
{
    mUseDragData = use;
}


bool
AsdkPoly::useDragData()
{
    return mUseDragData;
}

Acad::ErrorStatus 
AsdkPoly::subHighlight (const AcDbFullSubentPath& path , const Adesk::Boolean highlightAll) const
{
    AcDbObjectId id = objectId();
#ifdef _DEBUG
    acutPrintf(_T("\nHighlight %d"), id.asOldId());
#endif
    acdbQueueForRegen(&id,1);
    mShowAffordances = true;
    return AcDbEntity::subHighlight(path,highlightAll);
}

Acad::ErrorStatus 
AsdkPoly::subUnhighlight(const AcDbFullSubentPath& path, const Adesk::Boolean highlightAll) const
{
    AcDbObjectId id = objectId();
#ifdef _DEBUG
    acutPrintf(_T("\nUnhighlight %d"), id.asOldId());
#endif
    acdbQueueForRegen(&id,1);
    mShowAffordances = false;
    return AcDbEntity::subUnhighlight(path,highlightAll);
}


//*************************************************************************
// Statics functions used in this file. 
//*************************************************************************

static Acad::ErrorStatus drawFacets(const AsdkPoly*         poly,
                                          AcGiWorldDraw*    worldDraw,
                                          AcGiViewportDraw* vportDraw)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }
        
    // Since GS interface handles polygons with more than three sides,
    // we will pass in the whole thing. No need of triangulation.

    Adesk::UInt32 faceListSize = vertexArray.length() + 1; // one face + 
                                                           // num Vertices for
                                                           // the face.
    Adesk::Int32* faceList = new Adesk::Int32[faceListSize];

    faceList[0] = vertexArray.length();
    for (int i = 0; i < vertexArray.length(); i++) {
        faceList[i + 1] = i;
    }

    // If we wanted to do anything special for edges and faces, e.g. making
    // edges invisible, setting face color, etc., we would have bothered
    // with these two objects.

    AcGiEdgeData* edgeData = NULL;
    AcGiFaceData* faceData = NULL;
    
    if (worldDraw != NULL) {
        
        worldDraw->geometry().shell(vertexArray.length(),
                                    vertexArray.asArrayPtr(),
                                    faceListSize,
                                    faceList,
                                    edgeData,
                                    faceData);
    } else {
        assert(Adesk::kFalse);
    }
    
    delete [] faceList;
    delete    edgeData;
    delete    faceData;

    return es;
}


static Acad::ErrorStatus drawEdges(const AsdkPoly*         poly,
                                         AcGiWorldDraw*    worldDraw,
                                         AcGiViewportDraw* vportDraw)
{
    Acad::ErrorStatus es = Acad::eOk;

    // Draw each edge of the polygon as a line. We could have drawn
    // the whole polygon as a polyline, but we want to attach subEntity
    // traits (e.g. which line it is) to each line which will be used
    // for snap.
    //
    // Since we are drawing the polygon line by line, we also have the
    // control over setting the linetype and color of each line (via
    // subEntity traits), but we won't get that fancy.


    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcGePoint3d ptArray[2];

    for (int i = 0; i < vertexArray.length() - 1; i++) {

        if (worldDraw != NULL) {
            worldDraw->subEntityTraits().setSelectionMarker(i + 1);
        } else {
            assert(Adesk::kFalse);
            //vportDraw->subEntityTraits().setSelectionMarker(i + 1);
        }

        ptArray[0] = vertexArray[i];
        ptArray[1] = vertexArray[i + 1];

        if (worldDraw != NULL) {
            worldDraw->geometry().polyline(2, ptArray);
        } else {
            assert(Adesk::kFalse);
            //vportDraw->geometry().polyline3d(2, ptArray);
        }
    }

    return es;
}


static Acad::ErrorStatus drawName(const AsdkPoly*      poly,
                                        AcGiWorldDraw* worldDraw)
{
    const TCHAR *pName = poly->name();

    AcGePoint3d center;
    poly->getCenter(center);
    const AcGeVector3d normal = poly->normal();

    worldDraw->subEntityTraits().setSelectionMarker(AsdkPolyAssocPersSubentIdPE::kCenterSubentIndex);

    if ((pName != NULL) && (pName[0] != _T('\0')))
    {
        AcDbObjectId styleId = poly->styleId();
        AcGiTextStyle textStyle;

        if (styleId != AcDbObjectId::kNull)
            if (rx_getTextStyle(textStyle, styleId) != Acad::eOk)
                styleId = AcDbObjectId::kNull;

        AcGePoint3d startPoint;
        poly->getStartPoint(startPoint);
        AcGeVector3d direction = startPoint - center;
        Adesk::Boolean ok;

        if (styleId != AcDbObjectId::kNull)
            ok = 
                worldDraw->geometry().text(center, normal, direction,
                 pName, -1, 0, textStyle);
        else
            ok = 
                worldDraw->geometry().text(center, normal, direction,
                 direction.length() / 20, 1, 0, pName);
    }
    else
    {
        // Draw a point so that there is selectable geometry with a GS marker
        // at the center
        //
        worldDraw->geometry().polypoint(1, &center, &normal);
    }
    return Acad::eOk;
}


static Acad::ErrorStatus intLine(const AsdkPoly*         poly, 
                                 const AcGeLine3d        line,
                                       AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcGeLineSeg3d tlnsg;
    AcGePoint3d   pt;

    for (int i = 0; i < vertexArray.length() - 1; i++) {

        tlnsg.set(vertexArray[i], vertexArray[i + 1]);

        if (!tlnsg.intersectWith(line, pt)) {
            continue;
        } else {
            points.append(pt);
        }
    }

    return es;
}


static Acad::ErrorStatus intLine(const AsdkPoly*         poly, 
                                 const AcDbLine*         line,
                                       AcDb::Intersect   intType,
                                 const AcGePlane*        projPlane,
                                       AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGeLineSeg3d lnsg(line->startPoint(), line->endPoint());
    es = intLine(poly, lnsg, intType, projPlane, points);

    return es;
}


static Acad::ErrorStatus intLine(const AsdkPoly*         poly, 
                                 const AcGeLineSeg3d     lnsg,
                                       AcDb::Intersect   intType,
                                 const AcGePlane*        projPlane,
                                       AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcGeLine3d aline(lnsg.startPoint(), lnsg.endPoint());
    AcGeLineSeg3d tlnsg;
    AcGePoint3d   pt;
    AcGePoint3d   dummy;

    for (int i = 0; i < vertexArray.length() - 1; i++) {

        tlnsg.set(vertexArray[i], vertexArray[i + 1]);

        if (intType == AcDb::kExtendArg || intType == AcDb::kExtendBoth) {
            if (projPlane == NULL) {
                if (!tlnsg.intersectWith(aline, pt)) {
                    continue;
                } else {
                    points.append(pt);
                }
            } else {
                if (!tlnsg.projIntersectWith(aline, projPlane->normal(),
                    pt, dummy))
                {
                    continue;
                } else {
                    points.append(pt);
                }
            }
        } else {
            if (projPlane == NULL) {
                if (!tlnsg.intersectWith(lnsg, pt)) {
                    continue;
                } else {
                    points.append(pt);
                }
            } else {
                if (!tlnsg.projIntersectWith(lnsg, projPlane->normal(),
                    pt, dummy))
                {
                    continue;
                } else {
                    points.append(pt);
                }
            }
        }
    }

    return es;
}

static Acad::ErrorStatus intArc(const AsdkPoly*         poly, 
                                const AcDbArc*          arc,
                                      AcDb::Intersect   intType,
                                const AcGePlane*        projPlane,
                                      AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGeCircArc3d aarc(arc->center(), arc->normal(), 
                       arc->normal().perpVector(), arc->radius(), 
                       arc->startAngle(), arc->endAngle());
    es = intArc(poly, aarc, intType, projPlane, points);

    return es;
}


static Acad::ErrorStatus intArc(const AsdkPoly*         poly, 
                                const AcGeCircArc3d     arc,
                                      AcDb::Intersect   intType,
                                const AcGePlane*        projPlane,
                                      AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcGeCircArc3d  acircle(arc.center(), arc.normal(), arc.radius());
    AcGeLineSeg3d lnsg;
    AcGePoint3d   pt1, pt2;
    AcGePoint3d   dummy1, dummy2;
    int           howMany;

    for (int i = 0; i < vertexArray.length() - 1; i++) {

        lnsg.set(vertexArray[i], vertexArray[i + 1]);

        if (intType == AcDb::kExtendArg || intType == AcDb::kExtendBoth) {
            if (projPlane == NULL) {
                if (!acircle.intersectWith(lnsg, howMany, pt1, pt2)) {
                    continue;
                } else {
                    if (howMany > 1) {
                        points.append(pt1);
                        points.append(pt2);
                    } else {
                        points.append(pt1);
                    }
                }
            } else {
                if (!acircle.projIntersectWith(lnsg, projPlane->normal(),
                    howMany, pt1, pt2, dummy1, dummy2))
                {
                    continue;
                } else {
                    if (howMany > 1) {
                        points.append(pt1);
                        points.append(pt2);
                    } else {
                        points.append(pt1);
                    }
                }
            }
        } else {
            if (projPlane == NULL) {
                if (!arc.intersectWith(lnsg, howMany, pt1, pt2)) {
                    continue;
                } else {
                    if (howMany > 1) {
                        points.append(pt1);
                        points.append(pt2);
                    } else {
                        points.append(pt1);
                    }
                }
            } else {
                if (!arc.projIntersectWith(lnsg, projPlane->normal(),
                    howMany, pt1, pt2, dummy1, dummy2))
                {
                    continue;
                } else {
                    if (howMany > 1) {
                        points.append(pt1);
                        points.append(pt2);
                    } else {
                        points.append(pt1);
                    }
                }
            }
        }
    }

    return es;
}


static Acad::ErrorStatus intCircle(const AsdkPoly*         poly, 
                                   const AcDbCircle*       circle,
                                         AcDb::Intersect   intType,
                                   const AcGePlane*        projPlane,
                                         AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGeCircArc3d  acircle(circle->center(), circle->normal(),
                          circle->radius());
    es = intCircle(poly, acircle, intType, projPlane, points);

    return es;
}


static Acad::ErrorStatus intCircle(const AsdkPoly*         poly, 
                                   const AcGeCircArc3d     circle,
                                         AcDb::Intersect   intType,
                                   const AcGePlane*        projPlane,
                                         AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcGeLineSeg3d lnsg;
    AcGePoint3d   pt1, pt2;
    AcGePoint3d   dummy1, dummy2;
    int           howMany;

    for (int i = 0; i < vertexArray.length() - 1; i++) {

        lnsg.set(vertexArray[i], vertexArray[i + 1]);

        if (projPlane == NULL) {
            if (!circle.intersectWith(lnsg, howMany, pt1, pt2)) {
                continue;
            } else {
                if (howMany > 1) {
                    points.append(pt1);
                    points.append(pt2);
                } else {
                    points.append(pt1);
                }
            }
        } else {
            if (!circle.projIntersectWith(lnsg, projPlane->normal(), 
               howMany, pt1, pt2, dummy1, dummy2))
            {
                continue;
            } else {
                if (howMany > 1) {
                    points.append(pt1);
                    points.append(pt2);
                } else {
                    points.append(pt1);
                }
            }
        }
    }

    return es;
}


static Acad::ErrorStatus intPline(const AsdkPoly*         poly, 
                                        AcDb2dPolyline*   pline,
                                        AcDb::Intersect   intType,
                                  const AcGePlane*        projPlane,
                                        AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcDbSpline*      spline = NULL;
    AcDbLine*        acadLine = NULL;

    AcGePoint3dArray    pts;
    AcGeDoubleArray  bulges;
    int              numPoints, i;
    AcGePoint3d      pt1, pt2;

    AcGeCircArc3d    arc;
    AcGeLineSeg3d    lnsg;

    AcGeVector3d entNorm = pline->normal();
    AcDb::Intersect type = AcDb::kOnBothOperands;

    switch (pline->polyType()) {

    case AcDb::k2dSimplePoly:
    case AcDb::k2dFitCurvePoly:

        // Intersect with each line or arc segment of the polyline. 
        // Depending on the intType, extend the last segment.

        if ((es = rx_scanPline(pline, pts, bulges)) != Acad::eOk) {
            return es;
        }
        numPoints = pts.length();

        for (i = 0; i < numPoints - 1; i++) {

            pt1 = pts[i]; pt2 = pts[i + 1];
            if (i == numPoints - 2)
                type = intType;

            if (bulges[i] > 1.0e-10) {     // create an arc

                acdbWcs2Ecs(asDblArray(pt1), asDblArray(pt1), asDblArray(entNorm), 
                                                              Adesk::kFalse);
                acdbWcs2Ecs(asDblArray(pt2), asDblArray(pt2), asDblArray(entNorm), 
                                                              Adesk::kFalse);

                rx_makeArc(pt1, pt2, bulges[i], entNorm, arc);
                intArc(poly, arc, type, projPlane, points);

            } else {                       // create a line

                lnsg.set(pt1, pt2);
                intLine(poly, lnsg, type, projPlane, points);
            }
        }
        break;

    case AcDb::k2dQuadSplinePoly:
    case AcDb::k2dCubicSplinePoly:

        if ((es = pline->getSpline(spline)) != Acad::eOk) {
            return es;
        }

        if (intType == AcDb::kExtendArg || intType == AcDb::kExtendBoth) {
            intType = AcDb::kExtendThis;
        }

        for (i = 0; i < vertexArray.length() - 1; i++) {

            acadLine = new AcDbLine();
            acadLine->setStartPoint(vertexArray[i]);
            acadLine->setEndPoint(vertexArray[i + 1]);
            acadLine->setNormal(poly->normal());

            if (projPlane == NULL) {
                spline->intersectWith(acadLine, intType, points);
            } else {
                spline->intersectWith(acadLine, intType, *projPlane, points);
            }

            delete acadLine;
        }

        delete spline;
        break;

    default:
       return Acad::eInvalidInput;
    }

    return es;
}


static Acad::ErrorStatus intPline(const AsdkPoly*         poly, 
                                        AcDb3dPolyline*   pline,
                                        AcDb::Intersect   intType,
                                  const AcGePlane*        projPlane,
                                        AcGePoint3dArray& points)
{
    Acad::ErrorStatus es = Acad::eOk;

    AcGePoint3dArray vertexArray;
    if ((es = poly->getVertices3d(vertexArray)) != Acad::eOk) {
        return es;
    }

    AcDbSpline*      spline = NULL;
    AcDbLine*        acadLine = NULL;

    AcGePoint3dArray    pts;
    int              numPoints, i;

    AcGeLineSeg3d    lnsg;
    AcDb::Intersect  type = AcDb::kOnBothOperands;

    switch (pline->polyType()) {

    case AcDb::k3dSimplePoly:

        if ((es = rx_scanPline(pline, pts)) != Acad::eOk) {
            return es;
        }
        numPoints = pts.length();

        // Intersect with each line segment of the polyline. 
        // Depending on the intType, extend the last segment.

        for (i = 0; i < numPoints - 1; i++) {

            if (i == numPoints - 2)
                type = intType;

            lnsg.set(pts[i], pts[i + 1]);
            if ((es = intLine(poly, lnsg, type, projPlane, points)) 
                   != Acad::eOk) {
                return es;
            }
        }

    case AcDb::k3dQuadSplinePoly:
    case AcDb::k3dCubicSplinePoly:

        if ((es = pline->getSpline(spline)) != Acad::eOk) {
            delete spline;
            return es;
        }

        if (intType == AcDb::kExtendArg || intType == AcDb::kExtendBoth) {
            intType = AcDb::kExtendThis;
        }

        for (i = 0; i < vertexArray.length() - 1; i++) {

            acadLine = new AcDbLine();
            acadLine->setStartPoint(vertexArray[i]);
            acadLine->setEndPoint(vertexArray[i + 1]);
            acadLine->setNormal(poly->normal());

            if (projPlane == NULL) {
                spline->intersectWith(acadLine, intType, points);
            } else {
                spline->intersectWith(acadLine, intType, *projPlane, points);
            }

            delete acadLine;
        }

        delete spline;
        break;

    default:
       return Acad::eInvalidInput;
    }

    return es;
}

void changeAppNameCallback(const AcRxClass* classObj, TCHAR*& newAppName,
    int saveVer)
{
    if (saveVer == AcDb::kDHL_1014 && classObj == AsdkPoly::desc())
        acutNewString(_T("AsdkPolyOBJ")
        _T("|Product Desc:     PolyCAD ARX App For Polygon Entity")
        _T("|Company:          Autodesk,Inc.")
        _T("|WEB Address:      www.autodesk.com"), newAppName);
}


AcRx::AppRetCode __declspec(dllexport)
acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt)
{
    switch(msg) {

    case AcRx::kInitAppMsg:

        acrxUnlockApplication(pkt);     // Try to allow unloading

        acrxRegisterAppMDIAware(pkt);

        AsdkPoly::rxInit(changeAppNameCallback);

        // For constraints
        //
        ConstrainedRigidEntityAssocPersSubentIdPE::registerClasses();
        AsdkPolyAssocPersSubentIdPE::rxInit();

        // Register a service using the class name.
        if (!acrxServiceIsRegistered(_T("AsdkPoly")))
            acrxRegisterService(_T("AsdkPoly"));

        acrxBuildClassHierarchy();

        // For constraints
        //
        AsdkPoly::desc()->addX(AcDbAssocPersSubentIdPE::desc(), &sPolyAssocPersSubentIdPE);

        AssocAnnotationsEnabler::enableForEntity(AsdkPoly::desc());
        break;

    case AcRx::kUnloadAppMsg:
        // Unregister the service
        AcRxObject *obj = acrxServiceDictionary->remove(_T("AsdkPoly"));
        if (obj != NULL)
            delete obj;

        AssocAnnotationsEnabler::disableForEntity(AsdkPoly::desc());

        // For constraints
        //
        AsdkPoly::desc()->delX(AcDbAssocPersSubentIdPE::desc());
        ConstrainedRigidEntityAssocPersSubentIdPE::unregisterClasses();
        deleteAcRxClass(AsdkPolyAssocPersSubentIdPE::desc());

        deleteAcRxClass(AsdkPoly::desc());
        acrxBuildClassHierarchy();
        break;
    }
    return AcRx::kRetOK;
}
