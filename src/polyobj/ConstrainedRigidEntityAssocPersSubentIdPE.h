//////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2016 Autodesk, Inc.  All rights reserved.
//
//  Use of this software is subject to the terms of the Autodesk license 
//  agreement provided at the time of installation or download, or which 
//  otherwise accompanies this software in either electronic or hard copy form.   
//
// DESCRIPTION: 
//
// Declaration of classes:
// - ConstrainedRigidEntityAssocPersSubentIdPE
// - AssocAnnotationsEnabler
//
//////////////////////////////////////////////////////////////////////////////

#pragma once
#include "AcDbAssocPersSubentIdPE.h"

// General-purpose concrete class to be used with any custom entity. It is not 
// limited to AsdkPoly entity.
//
// This concrete protocol extension class allows custom entities to participate 
// in constraint systems. Geometric and dimensional constraints can be created 
// between the custom entity and other entities in the drawing. When any of the
// constrained entities are modified, the constraint system is solved and it 
// transforms other entities so that constraints between all the entities are 
// satisfied.
//
// The custom entity is treated by the constraint system as a rigid body, i.e. 
// the constraint system translates and rotates the custom entity as a whole 
// but it does not change the entity shape. The constraint system interacts with 
// rigid entities by calling getRigidSetTransform() and setRigidSetTransform() 
// methods of their AcDbAssocPersSubentIdPE protocol extensions to get and set 
// the coordinate system of the rigid entity; it does not transform the 
// individual subentities (edges, vertices) of the custom entity.
//
// When the custom entity is selected and edited and its shape changes during 
// dragging, the constraints stop being solved regarding to the changed custom 
// entity (as if the custom entity stayed unchanged). Constrains are then solved
// at the end, after the dragging has been finished. The reason for this behavior 
// is because the constraint system solver solves all constraints simultaneously 
// and cannot handle entities whose shape changes arbitrarily during dragging.
//
// The custom entity needs to expose an instance of this concrete
// ConstrainedRigidEntityAssocPersSubentIdPE class and provide its own 
// AcDbAssocPersSubentIdPE as an argument to the constructor. This entity-specific 
// AcDbAssocPersSubentIdPE implements methods to get the coordinate system of
// the custom entity and to obtain geometry of the custom entity. It is up to the 
// custom entity how it defines its coordinate system.
//
// The entity-specific AcDbAssocPersSubentIdPE needs to override and implement methods:
//
// - getRigidSetTransform()
// - getAllSubentities()
// - getEdgeVertexSubentities()
// - getVertexSubentityGeometry()
// - getEdgeSubentityGeometry()
//
// Other protocol extension methods may, but do not need to, be overridden.
// In particular, getRigidSetTransform() does not need to be implemented 
// because ConstrainedRigidEntityAssocPersSubentIdPE calls transformBy() 
// on the custom entity to change the entity position.
//
// The custom entity needs to call the following methods of the 
// ConstrainedRigidEntityAssocPersSubentIdPE:
//
// - notifyEntityDragStatus():
//   Must be called from the custom entity's overridden dragStatus() method.
//
// - notifyEntityChangedShape():
//   Must be called when the custom entity shape changes (including scaling
//   and mirror). When the whole entity is transformed by a rigid transform 
//   (translation and rotation), it does not cause entity shape change.
//
// - notifyEntityDeleted():
//   Must be caled from the custom entity's destructor.
//
// Caveat: The constrained dragging only works when the custom entity makes 
//         clones for dragging (the AcDbEntity::subCloneMeForDragging() 
//         returns true). 
//
class ConstrainedRigidEntityAssocPersSubentIdPE : public AcDbAssocPersSubentIdPE
{
public:
    ACRX_DECLARE_MEMBERS(ConstrainedRigidEntityAssocPersSubentIdPE);

    explicit ConstrainedRigidEntityAssocPersSubentIdPE() : mpCustomEntityPE(nullptr) {}

    // The custom entity must provide its own entity-specific instance of a class 
    // derived from AcDbAssocPersSubentIdPE. which needs to override and implement:
    // - getRigidSetTransform()
    // - getAllSubentities()
    // - getEdgeVertexSubentities()
    // - getVertexSubentityGeometry()
    // - getEdgeSubentityGeometry()
    // 
    explicit ConstrainedRigidEntityAssocPersSubentIdPE(AcDbAssocPersSubentIdPE* pCustomEntityPE) 
      : mpCustomEntityPE(pCustomEntityPE) {}

    AcDbAssocPersSubentIdPE* getCustomEntityPE() const { return mpCustomEntityPE; }

    // These methods must be called from the custom entity's code to provide 
    // information about what is happening to the custom entity
    //
    void notifyEntityDragStatus  (const AcDbEntity* pOrigEntity, AcDb::DragStat); // Call from the entity's dragStatus() method
    void notifyEntityChangedShape(const AcDbEntity*);                             // Call when the entity shape changes
    void notifyEntityDeleted     (const AcDbEntity*);                             // Call from the entity's destructor

    // Methods of the AcDbAssocPersSubentIdPE just delegate to the 
    // custom-entity-specific protocol extension, but may provide the original 
    // unchanged entity instead of the passed in temporary clone produced by
    // the dragger when the edited custom entity changed shape during dragging

    virtual int getRigidSetState(const AcDbEntity*) override { return kNonScalableRigidSet; }

    virtual AcDbAssocPersSubentId* createNewPersSubent(AcDbEntity*                 pEntity,
                                                       AcDbDatabase*               pDatabase,
                                                       const AcDbCompoundObjectId& /*compId*/,
                                                       const AcDbSubentId&         subentId) override;

    virtual Acad::ErrorStatus releasePersSubent(AcDbEntity*                  pEntity, 
                                                AcDbDatabase*                pDatabase,
                                                const AcDbAssocPersSubentId* pPerSubentId) override;

    virtual Acad::ErrorStatus transferPersSubentToAnotherEntity(
        AcDbEntity*                  pToEntity,
        AcDbDatabase*                pToDatabase,
        AcDbEntity*                  pFromEntity,
        AcDbDatabase*                pFromDatabase,
        const AcDbAssocPersSubentId* pPersSubentId) override;

    virtual Acad::ErrorStatus makePersSubentPurgeable(AcDbEntity*                  pEntity, 
                                                      AcDbDatabase*                pDatabase,
                                                      const AcDbAssocPersSubentId* pPerSubentId,
                                                      bool                         yesNo) override;

    virtual Acad::ErrorStatus getTransientSubentIds(const AcDbEntity*            pEntity, 
                                                    AcDbDatabase*                pDatabase,
                                                    const AcDbAssocPersSubentId* pPersSubentId,
                                                    AcArray<AcDbSubentId>&       subents) const override;

    virtual Acad::ErrorStatus getAllSubentities(const AcDbEntity*      pEntity,
                                                AcDb::SubentType       subentType,
                                                AcArray<AcDbSubentId>& allSubentIds) override;

    virtual Acad::ErrorStatus getAllSubentities(const AcDbEntity*      pEntity,
                                                const AcRxClass*       pSubentClass,
                                                AcArray<AcDbSubentId>& allSubentIds) override;

    virtual Acad::ErrorStatus getEdgeVertexSubentities(const AcDbEntity*      pEntity,
                                                       const AcDbSubentId&    edgeSubentId,
                                                       AcDbSubentId&          startVertexSubentId,
                                                       AcDbSubentId&          endVertexSubentId,
                                                       AcArray<AcDbSubentId>& otherVertexSubentIds) override;

    virtual Acad::ErrorStatus getSplineEdgeVertexSubentities(const AcDbEntity*      pEntity,
                                                             const AcDbSubentId&    edgeSubentId,
                                                             AcDbSubentId&          startVertexSubentId,
                                                             AcDbSubentId&          endVertexSubentId,
                                                             AcArray<AcDbSubentId>& controlPointSubentIds,
                                                             AcArray<AcDbSubentId>& fitPointSubentIds) override;

    virtual Acad::ErrorStatus getVertexSubentityGeometry(const AcDbEntity*   pEntity,
                                                         const AcDbSubentId& vertexSubentId,
                                                         AcGePoint3d&        vertexPosition) override;

    virtual Acad::ErrorStatus getEdgeSubentityGeometry(const AcDbEntity*   pEntity,
                                                       const AcDbSubentId& edgeSubentId,
                                                       AcGeCurve3d*&       pEdgeCurve) override;

    virtual Acad::ErrorStatus getFaceSubentityGeometry(const AcDbEntity*   pEntity,
                                                       const AcDbSubentId& faceSubentId,
                                                       AcGeSurface*&       pFaceSurface) override;

    virtual Acad::ErrorStatus getSubentityGeometry(const AcDbEntity*   pEntity,
                                                   const AcDbSubentId& subentId,
                                                   AcGeEntity3d*&      pSubentityGeometry) override;

    virtual Acad::ErrorStatus getFaceSilhouetteGeometry(AcDbEntity*                  pEntity,
                                                        AcRxObject*                  pContext,
                                                        const AcDbSubentId&          faceSubentId,
                                                        const AcGeMatrix3d*          pEntityTransform,
                                                        class AcDbGeometryProjector* pGeometryProjector,
                                                        AcArray<int>&                transientSilhIds, // Either in or out argument
                                                        AcArray<AcGeCurve3d*>&       silhCurves) override;

    virtual AcDbAssocPersSubentId* createNewPersFaceSilhouetteId(AcDbEntity*                  pEntity,
                                                                 AcDbDatabase*                pDatabase,
                                                                 AcRxObject*                  pContext,
                                                                 const AcDbCompoundObjectId&  /*compId*/,
                                                                 const AcDbSubentId&          faceSubentId,
                                                                 const AcGeMatrix3d*          pEntityTransform,
                                                                 class AcDbGeometryProjector* pGeometryProjector,
                                                                 int                          transientSilhId) override;

    virtual Acad::ErrorStatus getTransientFaceSilhouetteIds(AcDbEntity*                  pEntity,
                                                            AcDbDatabase*                pDatabase,
                                                            AcRxObject*                  pContext,
                                                            const AcDbSubentId&          faceSubentId,
                                                            const AcGeMatrix3d*          pEntityTransform,
                                                            class AcDbGeometryProjector* pGeometryProjector,
                                                            const AcDbAssocPersSubentId* pPersSilhId,
                                                            AcArray<int>&                transientSilhIds) override;


    virtual Acad::ErrorStatus mirrorPersSubent(const AcDbEntity*      pMirroredEntity,
                                               AcDbDatabase*          pDatabase,
                                               AcDbAssocPersSubentId* pPersSubentIdToMirror) override;

    virtual Acad::ErrorStatus getRigidSetTransform(const AcDbEntity* pEntity,
                                                   AcGeMatrix3d&     trans) override;

    // If the custom entity changed shape during dragging, does nothing. Otherwise
    // calls transformBy() on the custom entity with the difference between the 
    // current rigid set transform of the entity and the newly provided transform
    //
    virtual Acad::ErrorStatus setRigidSetTransform(AcDbEntity*         pEntity, 
                                                   const AcGeMatrix3d& newTrans) override;

    static void registerClasses();
    static void unregisterClasses();

private:
    AcDbAssocPersSubentIdPE* mpCustomEntityPE;
};


// Enables associative dimensions on a custom entity by making the entity 
// expose AcDbAssocAnnotationPE. The Associative Framework obtains entity 
// geometry via the entity's AcDbAssocPersSubentIdPE
//
class AssocAnnotationsEnabler
{
public:
    static Acad::ErrorStatus enableForEntity (AcRxClass* pEntityClass);
    static Acad::ErrorStatus disableForEntity(AcRxClass* pEntityClass);
};
