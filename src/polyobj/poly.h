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
// POLY.H
//

#ifndef POLYSAMP_POLY_H
#define POLYSAMP_POLY_H


#include "dbmain.h"
#include "dbcurve.h"
#include "geassign.h"
#include "gegbl.h"
#include "gegblabb.h"
#include "gemat3d.h"
#include "acestext.h"
#include "gept2dar.h"
#include "windows.h"
#include "dbxutil.h"

// The following is part of the code used to export a poly API
// and/or use the exported API.
//
#pragma warning( disable: 4275 4251 )
#ifdef POLYSAMP
#define DLLIMPEXP __declspec( dllexport )
#else

// Note: we don't use __declspec(dllimport) here, because of the
// "local vtable" problem with msvc.  If you use __declspec(dllimport),
// then, when a client dll does a new on the class, the object's
// vtable pointer points to a vtable allocated in that client
// dll.  If the client dll then passes the object to another dll,
// and the client dll is then unloaded, the vtable becomes invalid
// and any virtual calls on the object will access invalid memory.
//
// By not using __declspec(dllimport), we guarantee that the
// vtable is allocated in the server dll during the ctor and the
// client dll does not overwrite the vtable pointer after calling
// the ctor.  And, since we expect the server dll to remain in
// memory indefinitely, there is no problem with vtables unexpectedly
// going away.
// 
#define DLLIMPEXP
#endif

// The "DLLIMPEXP" is only required for exporting a poly API or using
// the exported API.  It is not necessary for any custom classes that
// are not exporting an API of their own.
//
class DLLIMPEXP AsdkPoly: public  AcDbCurve
{
public:
    ACRX_DECLARE_MEMBERS(AsdkPoly);
    
    //*************************************************************************
    // Constructors and destructor
    //*************************************************************************
    
    AsdkPoly();
    virtual ~AsdkPoly();
    
    Acad::ErrorStatus set(const AcGePoint2d&   center,
        const AcGePoint2d&   startPoint,
        int                  numSides, 
        const AcGeVector3d&  normal,
        const TCHAR*	  pName,
        double elevation);
    
    Acad::ErrorStatus set(const AcGePoint2d&   center,
        const AcGePoint2d&   startPoint,
        int                  numSides, 
        const AcGeVector3d&  normal,
        const TCHAR*	  pName,
        AcDbObjectId styleId,
        double elevation);
    
    //*************************************************************************
    // Methods specific to AsdkPoly 
    //*************************************************************************
    
    const AcGePoint2d&	center()      const  
    {
        assertReadEnabled();
        return mDragDataFlags & kUseDragCache ? mDragCenter : mCenter;
    }
    //we return 3d data this way to avoid calling the copy constructor of AcGePoint3d
    //on the stack
    void getCenter(AcGePoint3d& center) const;
    //see getStartPoint for 3d version
    const AcGePoint2d&	startPoint()  const  
    {
        assertReadEnabled();
        return mDragDataFlags & kUseDragCache ? mDragStartPoint : mStartPoint;
    }
    int numSides()    const  
    {
        assertReadEnabled();
        return mNumSides;
    }
    const AcGeVector3d&	normal()      const  
    {
        assertReadEnabled();
        return mDragDataFlags & kUseDragCache ? mDragPlaneNormal : mPlaneNormal;
    }
    void setElevation(double elev)
    {
        assertWriteEnabled();
        mDragElevation = elev;
        mElevation = elev;
    }
    double elevation() const 
    {
        assertReadEnabled();
        return mDragDataFlags & kUseDragCache ? mDragElevation : mElevation;
    }
    const TCHAR*	       name()	     const;
    const AcDbObjectId&	styleId()     const;
    
    Acad::ErrorStatus  setCenter     (const AcGePoint2d& cen);
    Acad::ErrorStatus  setNormal     (const AcGeVector3d& norm);
    Acad::ErrorStatus  setStartPoint (const AcGePoint2d& startPt);
    Acad::ErrorStatus  setName	     (const TCHAR* pName);
    Acad::ErrorStatus  setNumSides   (int numSides);
    Acad::ErrorStatus  setTextStyle  (const AcDbObjectId& );
    
    AcGeVector3d       sideVector    (int  whichSide) const;
    Acad::ErrorStatus  getVertices2d   (AcGePoint2dArray& vertexArray) const;
    Acad::ErrorStatus  getVertices3d   (AcGePoint3dArray& vertexArray) const;

    static void setUseDragData(bool use);
    static bool useDragData();

    //*************************************************************************
    // Overridden methods from AcDbCurve
    //*************************************************************************
    
    virtual Adesk::Boolean    isClosed	   () const {assertReadEnabled();
    return Adesk::kTrue;}  
    virtual Adesk::Boolean    isPeriodic   () const {assertReadEnabled();
    return Adesk::kTrue;}   
    virtual Adesk::Boolean    isPlanar	   () const {assertReadEnabled();
    return Adesk::kTrue;}
    
    virtual Acad::ErrorStatus getPlane  (AcGePlane&        plane, 
        AcDb::Planarity&  type) const 
    {
        assertReadEnabled();
        plane.set(AcGePoint3d::kOrigin + mPlaneNormal*mElevation, mPlaneNormal);
        type = AcDb::kPlanar;
        return Acad::eOk;
    }
    
    virtual Acad::ErrorStatus getStartParam  (double& startParam)     const
    {
        assertReadEnabled();
        startParam = 0.0;
        return Acad::eOk;
    }

    virtual Acad::ErrorStatus getEndParam  (double& endParam)    const
    {
        assertReadEnabled();
        endParam = 6.28318530717958647692;
        return Acad::eOk;
    }
    
    virtual Acad::ErrorStatus getStartPoint (AcGePoint3d& startPoint)  const
    {
        assertReadEnabled();
        startPoint.set(mStartPoint.x,mStartPoint.y,mElevation);
        acdbEcs2Wcs(asDblArray(startPoint),asDblArray(startPoint),asDblArray(mPlaneNormal),Adesk::kFalse);
        return Acad::eOk;
    }
    
    virtual Acad::ErrorStatus getEndPoint (AcGePoint3d& startPoint)    const
    {
        assertReadEnabled();
        startPoint.set(mStartPoint.x,mStartPoint.y,mElevation);
        acdbEcs2Wcs(asDblArray(startPoint),asDblArray(startPoint),asDblArray(mPlaneNormal),Adesk::kFalse);
        return Acad::eOk;
    }
    
    virtual Acad::ErrorStatus getPointAtParam  (double             param, 
                                                AcGePoint3d&       point) const;
    virtual Acad::ErrorStatus getParamAtPoint  (const AcGePoint3d& point,
                                                double&            param) const;
    virtual Acad::ErrorStatus getDistAtParam   (double             param,
                                                double&            dist)  const;
    virtual Acad::ErrorStatus getParamAtDist    (double             dist,
                                                double&            param) const; 
    virtual Acad::ErrorStatus getDistAtPoint  (const AcGePoint3d& point,
                                                double&            dist)  const;
    virtual Acad::ErrorStatus getPointAtDist  (double             dist,
                                                AcGePoint3d&       point) const;
    
    virtual Acad::ErrorStatus getFirstDeriv  (double          param, 
                                                AcGeVector3d&   firstDeriv) const;
    virtual Acad::ErrorStatus getFirstDeriv  (const AcGePoint3d& point, 
        AcGeVector3d&   firstDeriv) const;
    virtual Acad::ErrorStatus getSecondDeriv (double          param, 
        AcGeVector3d&   secondDeriv)const
    { return Acad::eNotApplicable;}
    virtual Acad::ErrorStatus getSecondDeriv  (const AcGePoint3d& point, 
        AcGeVector3d&  secondDeriv)const
    { return Acad::eNotApplicable;}
    
    virtual Acad::ErrorStatus getClosestPointTo (const AcGePoint3d& givenPoint,
        AcGePoint3d&       pointOnCurve,
        Adesk::Boolean     extend = 
        Adesk::kFalse) 
        const;
    
    virtual Acad::ErrorStatus getClosestPointTo (const AcGePoint3d&  givenPoint,
        const AcGeVector3d& normal,
        AcGePoint3d&      pointOnCurve,
        Adesk::Boolean      extend = Adesk::kFalse) 
        const;
    
    virtual Acad::ErrorStatus getOrthoProjectedCurve(const AcGePlane&  plane,
        AcDbCurve*&       projectedCurve) 
        const;
    
    virtual Acad::ErrorStatus getProjectedCurve(const AcGePlane&    plane,
        const AcGeVector3d& projDir,
        AcDbCurve*&       projectedCurve) 
        const;
    
    virtual Acad::ErrorStatus getOffsetCurves (double             offsetDist,
        AcDbVoidPtrArray&           offsetCurves)  const;
    
    virtual Acad::ErrorStatus getSplitCurves (const AcGeDoubleArray& parameters,
        AcDbVoidPtrArray&           curveSegments) const;
    
    virtual Acad::ErrorStatus getSplitCurves  (const AcGePoint3dArray& points,
        AcDbVoidPtrArray&           curveSegments) const;
    
    virtual Acad::ErrorStatus extend (double		 newParam)
    {return Acad::eNotApplicable;}
    
    virtual Acad::ErrorStatus extend (Adesk::Boolean       extendStart,
        const AcGePoint3d&   toPoint)
    {return Acad::eNotApplicable;}
    
    virtual Acad::ErrorStatus getArea (double&		   area) const; 

    virtual void dragStatus(const AcDb::DragStat) override;
    
    //*************************************************************************
    // Overridden methods from AcDbObject
    //*************************************************************************
    
    virtual Acad::ErrorStatus   applyPartialUndo(AcDbDwgFiler* undoFiler, 
        AcRxClass*    classObj);
    virtual Acad::ErrorStatus	dwgInFields(AcDbDwgFiler* filer);
    virtual Acad::ErrorStatus	dwgOutFields(AcDbDwgFiler* filer) const;
    
    virtual Acad::ErrorStatus	dxfInFields(AcDbDxfFiler* filer);
    virtual Acad::ErrorStatus	dxfOutFields(AcDbDxfFiler* filer) const;

protected:
    virtual Adesk::Boolean      subWorldDraw(AcGiWorldDraw*	mode);

    virtual void                subList() const;
    virtual Acad::ErrorStatus   subTransformBy(const AcGeMatrix3d& xform);
    virtual Acad::ErrorStatus	subExplode(AcDbVoidPtrArray& entitySet) const;

    virtual Acad::ErrorStatus   subGetOsnapPoints(
        AcDb::OsnapMode       osnapMode,
        Adesk::GsMarker       gsSelectionMark,
        const AcGePoint3d&    pickPoint,
        const AcGePoint3d&    lastPoint,
        const AcGeMatrix3d&   viewXform,
        AcGePoint3dArray&        snapPoints,
        AcDbIntArray&     geomIds) const;
    
    virtual Acad::ErrorStatus   subGetGripPoints(AcGePoint3dArray&     gripPoints,
        AcDbIntArray&  osnapModes,
        AcDbIntArray&  geomIds) const;
    
    virtual Acad::ErrorStatus   subMoveGripPointsAt(const AcDbIntArray& indices,
        const AcGeVector3d&     offset);
    
    virtual Acad::ErrorStatus   subGetStretchPoints(
        AcGePoint3dArray&  stretchPoints)
        const;
    virtual Acad::ErrorStatus   subMoveStretchPointsAt(
        const AcDbIntArray& indices,
        const AcGeVector3d&     offset);
    
    virtual Acad::ErrorStatus   subGetTransformedCopy(const AcGeMatrix3d& mat,
        AcDbEntity*&        ent) const;
    
        
    //*************************************************************************
    // Overridden methods from AcDbEntity
    //*************************************************************************
    
    virtual Acad::ErrorStatus subGetSubentPathsAtGsMarker(
        AcDb::SubentType      type,
        Adesk::GsMarker       gsMark,
        const AcGePoint3d&    pickPoint,
        const AcGeMatrix3d&   viewXform,
        int&                  numPaths,
        AcDbFullSubentPath*&  subentPaths,
        int                   numInserts = 0,
        AcDbObjectId*         entAndInsertStack 
        = NULL) const;
    
    virtual Acad::ErrorStatus subGetGsMarkersAtSubentPath(
        const AcDbFullSubentPath& subPath, 
        AcDbIntPtrArray&         gsMarkers) const;
    
    virtual AcDbEntity*       subSubentPtr(const AcDbFullSubentPath& id) const;
 
    virtual Acad::ErrorStatus   subIntersectWith(
        const AcDbEntity*   ent,
        AcDb::Intersect     intType,
        AcGePoint3dArray&   points,
        Adesk::GsMarker     thisGsMarker  = 0,
        Adesk::GsMarker     otherGsMarker = 0)
        const;
    
    virtual Acad::ErrorStatus   subIntersectWith(
        const AcDbEntity*   ent,
        AcDb::Intersect     intType,
        const AcGePlane&    projPlane,
        AcGePoint3dArray&   points,
        Adesk::GsMarker     thisGsMarker  = 0,
        Adesk::GsMarker     otherGsMarker = 0)
        const;
        
    virtual Adesk::Boolean subCloneMeForDragging();

    //*************************************************************************
    // Overridden methods from AcDbObject
    //*************************************************************************
    
    virtual Acad::ErrorStatus subDeepClone(AcDbObject* pOwnerObject,
        AcDbObject*& pClonedObject,
        AcDbIdMapping& idMap,
        Adesk::Boolean isPrimary
        = Adesk::kTrue) const;
    
    virtual Acad::ErrorStatus subWblockClone(AcRxObject* pOwnerObject,
        AcDbObject*& pClonedObject,
        AcDbIdMapping& idMap,
        Adesk::Boolean isPrimary
        = Adesk::kTrue) const;
    
    virtual Acad::ErrorStatus subGetClassID(CLSID* pClsid) const;

    virtual Acad::ErrorStatus subHighlight  (const AcDbFullSubentPath&
                                          = kNullSubent, const Adesk::Boolean highlightAll = Adesk::kFalse) const;

    virtual Acad::ErrorStatus subUnhighlight(const AcDbFullSubentPath&
                                          = kNullSubent, const Adesk::Boolean highlightAll = Adesk::kFalse) const;

private:

    // These are here because otherwise dllexport tries to export the
    // private methods of AcDbObject.  They're private in AcDbObject
    // because vc5 and vc6 do not properly support array new and delete.
    // The "vector deleting dtor" gets optimized into a scalar deleting
    // dtor by the linker if the server dll does not call vector delete.
    // The linker shouldn't do that, because it doesn't know that the
    // object won't be passed to some other dll which *does* do vector
    // delete.
    //
    void *operator new[](size_t nSize) { return 0;}
    void operator delete[](void *p) {};
    void *operator new[](size_t nSize, const TCHAR *file, int line) { return 0;}

        // Associated Objects
        // It's necessary to distinguish when cloneMeForDragging() has been
        // called versus when we really want to use the drag data because
        // for certain operations cloneMeForDragging() will actually be
        // called before dragging is really going to start which will mess
        // up worldDraw() calls
        //
    enum {
        kCloneMeForDraggingCalled = 0x1,
        kUseDragCache = 0x2,
    };

    AcGePoint2d   mCenter;
    AcGePoint2d   mStartPoint;
    Adesk::Int32  mNumSides;
    AcGeVector3d  mPlaneNormal;
    double        mElevation;
    
    AcGePoint2d   mDragCenter,
                  mDragStartPoint;
    AcGeVector3d  mDragPlaneNormal;
    double        mDragElevation;
    //AcGeVector2d  mDragPlaneNormal;

    int           mDragDataFlags;
    
    // Note: the name and text style fields in this entity are here
    // mainly for exercising those types of objects.  Don't worry
    // too much about whether they make sense as part of this
    // particular entity.  The text style field is there to test
    // that hard id's prevent purging of the object pointed to.
    // It's also used to verify that purging works with proxy
    // entities.
    
    TCHAR*	  mpName;
    AcDbHardPointerId  mTextStyle;
    
    // This enum is meant for speficying operation type on a poly 
    // that do partial undo recording, for example, changing number of
    // sides of the poly.
    //
    enum PolyOpCodeForPartialUndo { kNoOp		= 0,
				    kSetNumSides        = 1 };
    
    // This static variable is used in AsdkPoly::cloneMeForDragging() to
    // allow runtime setting of whether or not AutoCAD's drag complex will
    // use a clone of each poly or not.
    // mUseDragData == true  :  use noClone mechanism
    // mUseDragData = false  :  use old clone mechanism
    //
    static bool mUseDragData;
    mutable bool mShowAffordances;
};

#endif /* POLYSAMP_POLY_H */

//- This line allows us to get rid of the .def file in ARX projects
#if !defined(_WIN64) && !defined (_AC64)
#pragma comment(linker, "/export:_acrxGetApiVersion,PRIVATE")
#else
#pragma comment(linker, "/export:acrxGetApiVersion,PRIVATE")
#endif