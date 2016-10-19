//////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2016 Autodesk, Inc.  All rights reserved.
//
//  Use of this software is subject to the terms of the Autodesk license 
//  agreement provided at the time of installation or download, or which 
//  otherwise accompanies this software in either electronic or hard copy form.   
//
//////////////////////////////////////////////////////////////////////////////

#pragma once
#include "AcDbAssocPersSubentIdPE.h"

// Definition of the AcDbAssocPersSubentIdPE for the AsdkPoly entity.
// This protocol extention reveals subentities of the AsdkPoly entity to the
// Associative Framework.
//
// When the AsdkPoly entity exposes this protocol extension, it can participate 
// in associative relations represented by the Associative Framework, such as
// geometric and dimensional constraints can be created between AsdkPoly
// entities and other entities in the drawing.
//
// The AsdkPoly entity is treated by the constraint system as a rigid body, 
// i.e. it can be translated and rotated as a whole but its shape cannot 
// be modified by constraints. The constraint system calls getRigidSetTransform()
// method of the AcDbAssocPersSubentIdPE protocol extensions to get the 
// coordinate system of the rigid entity; it does not transform the individual 
// subentities (edges, vertices).
//
// The coordinate system relates to the AsdkPoly entity definition data as follows:
//
// origin: center
// xAxis:  (startPoint - center).normal()
// zAxis:  normal
// yAxis:  zAxis corssProduct xAxis
//
// The edge and vertex subentities of the AskdPoly entity are defined as follows:
//
// - AcDbSubentId(AcDb::kEdgeSubentType, index), where 'index' is the 1-based
//   index of the polygon edge.
// - AcDbSubentId(AcDb::kVertexSubentType, index), where 'index' is the 1-based
//   index of the polygon vertex.
// - AcDbSubentId(AcDb::kVertexSubentType, kCenterSubentIndex) for the center 
//   vertex subentity.
// - AcDbSubentId(AcDb::kEdgeSubentType,   kCenterSubentIndex) for the "pseudo" 
//   edge subentity at the center (needed because constraints deal with edges, 
//   not isolated vertices).
//
// Edge with index i has start vertex with index i and end vertex with index i%numSides+1.
//
// AcDbAssocSimplePersSubentId is used for AcDbAssocPersSubentId as the topology
// of the AskdPoly entity is simple and does not change. AcDbAssocPersSubentIdPE
// methods like createNewPersSubent() and getTransientSubentIds() do not need to be 
// overriden and implemented as the base class implementation is used.
//
// Note:
// ----
// The AsdkPolyAssocPersSubentIdPE should not be exposed directly by the 
// AsdkPoly entity, but it needs to be wrapped in an instance of 
// ConstrainedRigidEntityAssocPersSubentIdPE concrete class. AsdkPoly entity should
// expose this instance. The reason for this is to better support dragging of 
// constrained AsdkPoly entities when the constrained AsdkPoly entity is edited 
// and changes shape during dragging
//
class AsdkPolyAssocPersSubentIdPE : public AcDbAssocPersSubentIdPE
{
public:
    ACRX_DECLARE_MEMBERS(AsdkPolyAssocPersSubentIdPE);

    static const int kCenterSubentIndex = -1; 

    // Returns a rigid transform matrix from the AsdkPoly definition data
    //
    virtual Acad::ErrorStatus getRigidSetTransform(const AcDbEntity* pEntity,
                                                   AcGeMatrix3d&     trans) override;

    // Does not need to be overridden as the ConstrainedRigidEntityAssocPersSubentIdPE
    // wrapper calls transformBy() on the entity
    //
    // virtual Acad::ErrorStatus setRigidSetTransform(AcDbEntity*         pEntity, 
    //                                               const AcGeMatrix3d&  newTrans) override;

    // Returns the segments and vertices as edge and vertex subentities
    //
    virtual Acad::ErrorStatus getAllSubentities(const AcDbEntity*      pEntity,
                                                AcDb::SubentType       subentType,
                                                AcArray<AcDbSubentId>& allSubentIds) override;

    // For a polygon segment AcDbSubentId returns its start and end vertex AcDbSubentId
    //
    virtual Acad::ErrorStatus getEdgeVertexSubentities(const AcDbEntity*      pEntity,
                                                       const AcDbSubentId&    edgeSubentId,
                                                       AcDbSubentId&          startVertexSubentId,
                                                       AcDbSubentId&          endVertexSubentId,
                                                       AcArray<AcDbSubentId>& otherVertexSubentIds) override;

    virtual Acad::ErrorStatus getVertexSubentityGeometry(const AcDbEntity*   pEntity,
                                                         const AcDbSubentId& vertexSubentId,
                                                         AcGePoint3d&        vertexPosition) override;

    virtual Acad::ErrorStatus getEdgeSubentityGeometry(const AcDbEntity*   pEntity,
                                                       const AcDbSubentId& edgeSubentId,
                                                       AcGeCurve3d*&       pEdgeCurve) override;
};
