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
// DESCROPTION:
//
// Implementation of the ConstrainedRigidEntityAssocPersSubentIdPE protocol 
// extension class and all the internal supporting classes

#include "acdb.h"
#include "dbobjptr2.h"
#include "eoktest.h"
#include "AcDbAssocManager.h"
#include "AcDbAssocGeomDependency.h"
#include "AcDbAssoc2dConstraintGroup.h"
#include "AcDbAssocNetwork.h"
#include "ConstrainedRigidEntityAssocPersSubentIdPE.h"

#define AcDbObjectPointer AcDbSmartObjectPointer


// Keeps information about dragged constrained rigid entities during dragging
//
class DraggedConstrainedRigidEntities
{
public:
    ~DraggedConstrainedRigidEntities();

    // Empties contents of this object
    //
    void reset();

    // Called from ConstrainedRigidEntityAssocPersSubentIdPE::notifyEntityDragStatus()
    //
    void notifyEntityDragStatus(const AcDbEntity* pOrigEntity, AcDb::DragStat dragStatus, bool onlyIfNotNotifiedYet);

    // Called from ConstrainedRigidEntityAssocPersSubentIdPE::notifyEntityChangedShape()
    //
    void notifyEntityChangedShape(const AcDbEntity*);

    // Called from ConstrainedRigidEntityAssocPersSubentIdPE::notifyEntityChanged()
    //
    void notifyEntityChanged(const AcDbEntity*);

    // Called from DraggedConstrainedRigidEntitiesReactor::copied()
    //
    void notifyEntityCopied(const AcDbEntity* pOrigEntity, AcDbEntity* pClone);

    // Called from ConstrainedRigidEntityAssocPersSubentIdPE::notifyEntityDeleted()
    //
    void notifyEntityDeleted(const AcDbEntity*);

    // If the entity changed shape during dragging, returns AcDbObjectId of
    // the original entity. Otherwise returns AcDbObjectId::kNull.
    //
    // The passed-in entity may be either the original database-resident entity
    // or a temporary non-database-resident clone produced by the dragger.
    // If the dragged entity hasn't changed shape, returna AcDbObjectId::kNull
    //
    AcDbObjectId getOrigEntityIfChangedShape(const AcDbEntity*) const;

    bool changedShape(const AcDbEntity*) const;

    // Called from ConstrainedRigidEntityAssocPersSubentIdPE::getEdge/VertexSubentityGeometry().
    // It only caches the explcitly requested subentities, not all subentities
    // of the custom entity
    //
    void cacheOrigSubentGeometry(const AcDbEntity*, const AcDbSubentId&);

    // Returns transform or geometry of the original entity, before it has 
    // been transformed by the dragger,
    // The passed-in entity may be either the original database-resident entity
    // or a temporary non-database-resident clone produced by the dragger 
    //
    Acad::ErrorStatus getOrigRigidSetTransform      (const AcDbEntity*, AcGeMatrix3d& trans) const;
    Acad::ErrorStatus getOrigEdgeSubentityGeometry  (const AcDbEntity*, const AcDbSubentId&, AcGeCurve3d*&) const;
    Acad::ErrorStatus getOrigVertexSubentityGeometry(const AcDbEntity*, const AcDbSubentId&, AcGePoint3d&) const;

    // Called from DraggedConstrainedRigidEntitiesEvalCallback::endActionEvaluation()
    // when the evaluated network is the top-level network.
    //
    // Removes entries whose dragging has ended (their DragState is kDragEnd) 
    // and that only have constraints from the database of this top-level network, 
    // i.e. all their constraints have been solved.
    //
    // If the entity changed shape during dragging, requests one more evaluation
    // to statically evaluate the constraints with the changed entity geometry
    //
    void endTopLevelNetworkEvaluation(const AcDbAssocNetwork* pTopLevelNetwork);

private:
    class EntityEntry
    {
    public:
        explicit EntityEntry(const AcDbObjectId&      origEntityId, 
                             const AcGeMatrix3d&      origRigidSetTransform, 
                             const AcDbObjectIdArray& constraintDepIds,
                             const AcDbObjectIdArray& constraintGroupIds);

        ~EntityEntry();

        AcDbObjectId             mOrigEntityId;
        const AcDbEntity*        mpEntityClone;
        AcGeMatrix3d             mOrigRigidSetTransform;
        AcArray<Adesk::GsMarker> mOrigVertexSubentIds; // Only their AcDbSubentId::index() part
        AcArray<Adesk::GsMarker> mOrigEdgeSubentIds;
        AcArray<AcGePoint3d>     mOrigVertexSubentGeometries;
        AcArray<AcGeCurve3d*>    mOrigEdgeSubentGeometries;
        AcDbObjectIdArray        mConstraintDepIds;
        AcDbObjectIdArray        mConstraintGroupIds;
        AcDb::DragStat           mDragStat;
        bool                     mEntityChangedShape;
    };

    int  findEntityEntry  (const AcDbObjectId& origEntityId) const;
    int  findEntityEntry  (const AcDbEntity*) const; // The entity may be either the original entity or a clone
    void removeEntityEntry(const AcDbEntity* pOrigEntity, int index);

    void collectDependenciesFromConstraintGroups(const AcDbObjectId& origEntityId, 
                                                 AcDbObjectIdArray&  constraintDepIds, 
                                                 AcDbObjectIdArray&  constraintGroupIds);

    AcArray<EntityEntry*> mEntityEntries;
};


// Used to get notifications about creating temporary entity clones during 
// dragging (unfortunatelly does not notify about entity deletes)
//
class DraggedConstrainedRigidEntitiesReactor : public AcDbObjectReactor
{
public:
    ACRX_DECLARE_MEMBERS(DraggedConstrainedRigidEntitiesReactor);

    virtual void copied(const AcDbObject* dbObj, const AcDbObject* newObj);
};


// Used to get notifications about end evaluation of the top-level network
//
class DraggedConstrainedRigidEntitiesEvalCallback : public AcDbAssocEvaluationCallback
{
public:
    DraggedConstrainedRigidEntitiesEvalCallback() : mIsRegistered(false) {}

    // The only overridden method of this callback.
    // On the last drag sample, when the top-level network evaluation is ending,
    // remove entries from DraggedConstrainedRigidEntities whose dragging has
    // ended. If the entity changed shape, request one more evaluation to statically
    // evaluate the constraints with the changed entity geometry
    //
    virtual void endActionEvaluation(AcDbAssocAction*) override;

    virtual void beginActionEvaluation(AcDbAssocAction*) override {}

    virtual void setActionEvaluationErrorStatus(AcDbAssocAction*    pAction,
                                                Acad::ErrorStatus   errorStatus,
                                                const AcDbObjectId& objectId   = AcDbObjectId::kNull,
                                                AcDbObject*         pObject    = NULL,
                                                void*               pErrorInfo = NULL) override {}

    virtual void 
    beginActionEvaluationUsingObject(AcDbAssocAction*    pAction, 
                                     const AcDbObjectId& objectId, 
                                     bool                objectIsGoingToBeUsed,
                                     bool                objectIsGoingToBeModified,
                                     AcDbObject*&        pSubstituteObject) override {}

    virtual void endActionEvaluationUsingObject(AcDbAssocAction*    pAction, 
                                                const AcDbObjectId& objectId, 
                                                AcDbObject*         pObject) override {}

    void registerCallback();
    void unegisterCallback();

private:
    bool mIsRegistered;
};


ACRX_CONS_DEFINE_MEMBERS(DraggedConstrainedRigidEntitiesReactor,    AcDbObjectReactor, 0)
ACRX_CONS_DEFINE_MEMBERS(ConstrainedRigidEntityAssocPersSubentIdPE, AcDbAssocPersSubentIdPE, 0)


// Static instances
//
static DraggedConstrainedRigidEntities             sCurrentlyDragged;
static DraggedConstrainedRigidEntitiesReactor      sCurrentlyDraggedReactor;
static DraggedConstrainedRigidEntitiesEvalCallback sCurrentlyDraggedEvalCallback;


void DraggedConstrainedRigidEntitiesReactor::copied(const AcDbObject* pDbObj, const AcDbObject* pNewObj)
{
    assert(AcDbEntity::cast(pDbObj) != nullptr && AcDbEntity::cast(pNewObj) != nullptr);

    sCurrentlyDragged.notifyEntityCopied(AcDbEntity::cast(pDbObj), AcDbEntity::cast(pNewObj));
}


void DraggedConstrainedRigidEntitiesEvalCallback::endActionEvaluation(AcDbAssocAction* pAction)
{
    const AcDbAssocNetwork* const pNetwork = AcDbAssocNetwork::cast(pAction);
    if (pNetwork == nullptr)
        return;
    if (AcDbAssocManager::globalEvaluationMultiCallback()->draggingState() != kLastSampleAssocDraggingState)
        return;
    if (pNetwork->objectId() != AcDbAssocNetwork::getInstanceFromDatabase(pNetwork->database(), false))
        return; // We will do the work when the top-level network evaluation finishes

    sCurrentlyDragged.endTopLevelNetworkEvaluation(pNetwork);
}


void DraggedConstrainedRigidEntitiesEvalCallback::registerCallback()
{
    if (!mIsRegistered)
    {
        mIsRegistered = true;
        AcDbAssocManager::addGlobalEvaluationCallback(this, 0);
    }
}


void DraggedConstrainedRigidEntitiesEvalCallback::unegisterCallback()
{
    if (mIsRegistered)
    {
        mIsRegistered = false;
        AcDbAssocManager::removeGlobalEvaluationCallback(this);
    }
}


DraggedConstrainedRigidEntities::EntityEntry::EntityEntry(const AcDbObjectId&      origEntityId, 
                                                          const AcGeMatrix3d&      origRigidSetTransform, 
                                                          const AcDbObjectIdArray& constraintDepIds,
                                                          const AcDbObjectIdArray& constraintGroupIds)
  : mOrigEntityId         (origEntityId),
    mpEntityClone         (nullptr),
    mOrigRigidSetTransform(origRigidSetTransform),
    mConstraintDepIds     (constraintDepIds),
    mConstraintGroupIds   (constraintGroupIds),
    mDragStat             (AcDb::kDragStart),
    mEntityChangedShape   (false)
{ 
    assert(!origEntityId.isNull());
}


DraggedConstrainedRigidEntities::EntityEntry::~EntityEntry()
{
    for (int i = 0; i < mOrigEdgeSubentGeometries.length(); i++)
    {
        delete mOrigEdgeSubentGeometries[i];
        mOrigEdgeSubentGeometries[i] = nullptr;
    }
}


DraggedConstrainedRigidEntities::~DraggedConstrainedRigidEntities() 
{ 
    assert(mEntityEntries.isEmpty()); 
    reset(); 
}


void DraggedConstrainedRigidEntities::reset()
{
    for (int i = 0; i < mEntityEntries.length(); i++)
    {
        delete mEntityEntries[i];
    }
    mEntityEntries.removeAll();
}


void DraggedConstrainedRigidEntities::removeEntityEntry(const AcDbEntity* pOrigEntity, int index)
{
    if (index != -1)
    {
        EntityEntry* const pEntityEntry = mEntityEntries[index];
        if (pOrigEntity != nullptr)
        {
            pOrigEntity->removeReactor(&sCurrentlyDraggedReactor);
        }
        else
        {
            assert(!"Null pOrigEntity");
        }

        delete mEntityEntries[index];
        mEntityEntries[index] = nullptr;
        mEntityEntries.removeAt(index);
    }

    if (mEntityEntries.isEmpty())
    {
        sCurrentlyDraggedEvalCallback.unegisterCallback();
    }
}


int DraggedConstrainedRigidEntities::findEntityEntry(const AcDbObjectId& origEntityId) const
{
    if (origEntityId.isNull())
    {
        assert(!"Null origEntityId");
        return -1;
    }

    for (int i = mEntityEntries.length()-1; i >= 0; i--)
    {
        if (mEntityEntries[i]->mOrigEntityId == origEntityId)
            return i;
    }
    return -1;
}


int DraggedConstrainedRigidEntities::findEntityEntry(const AcDbEntity* pEntity) const
{
    if (pEntity == nullptr)
    {
        assert(!"Null pEntity");
        return -1;
    }

    const AcDbObjectId origEntityId = pEntity->objectId();
    if (!origEntityId.isNull())
    {
        return findEntityEntry(origEntityId);
    }
    else
    {
        for (int i = mEntityEntries.length()-1; i >= 0; i--)
        {
            if (mEntityEntries[i]->mpEntityClone == pEntity)
                return i;
        }
        return -1;
    }
}


void DraggedConstrainedRigidEntities::notifyEntityDragStatus(const AcDbEntity* pOrigEntity, AcDb::DragStat dragStatus, bool onlyIfNotNotifiedYet)
{
    const AcDbObjectId origEntityId = pOrigEntity->objectId();
    if (origEntityId.isNull())
        return; // Only database-resident entities may have constraints

    const int index = findEntityEntry(origEntityId);

    switch (dragStatus)
    {
    case AcDb::kDragStart:
        {
            if (index != -1)
            {
                if (onlyIfNotNotifiedYet)
                    return; // Already handled
                else
                    removeEntityEntry(pOrigEntity, index);
            }

            AcDbObjectIdArray constraintDepIds;
            AcDbObjectIdArray constraintGroupIds;
            collectDependenciesFromConstraintGroups(origEntityId, constraintDepIds, constraintGroupIds);
            if (!constraintDepIds.isEmpty())
            {
                const int kTooMany = 100; // An arbitrary limit

                if (mEntityEntries.length() >= kTooMany)
                {
                    assert(!"Too many dragged constrained rigid entities");
                    return; // Do not monitor the entity
                }

                ConstrainedRigidEntityAssocPersSubentIdPE* const pWrapperPE 
                    = ConstrainedRigidEntityAssocPersSubentIdPE::cast(pOrigEntity->queryX(AcDbAssocPersSubentIdPE::desc()));
                if (pWrapperPE == nullptr)
                {
                    assert(!"ConstrainedRigidEntityAssocPersSubentIdPE not found on the entity");
                    return;
                }
                AcDbAssocPersSubentIdPE* const pPE = pWrapperPE->getCustomEntityPE();

                AcGeMatrix3d origRigidSetTransform;
                Acad::ErrorStatus err = pPE->getRigidSetTransform(pOrigEntity, origRigidSetTransform);
                if (!eOkVerify(err))
                    return;

                pOrigEntity->addReactor(&sCurrentlyDraggedReactor);
                sCurrentlyDraggedEvalCallback.registerCallback();

                EntityEntry* const pNewEntityEntry = new EntityEntry(origEntityId, origRigidSetTransform, constraintDepIds, constraintGroupIds);
                mEntityEntries.append(pNewEntityEntry);

                // Cache original geeometry of referenced subentities of the not
                // yet dragged entity. We need to do it at the beginning, because 
                // after dragging starts, it might be impossible to obtain the 
                // original geometry of the non dragged entity in case the original 
                // entity handled dragging by itself and does not make clones for 
                // dragging
                //
                for (int i = 0; i < constraintDepIds.length(); i++)
                {
                    AcDbObjectPointer<AcDbAssocGeomDependency> pGeomDep(constraintDepIds[i], AcDb::kForRead);
                    if (!eOkVerify(pGeomDep.openStatus()))
                        continue;

                    AcArray<AcDbSubentId> transientSubentIds;
                    if (!eOkVerify(err = pGeomDep->getTransientSubentIds(transientSubentIds)))
                        continue;
                    for (int j = 0; j < transientSubentIds.length(); j++)
                    {
                        cacheOrigSubentGeometry(pOrigEntity, transientSubentIds[j]);
                    }
                }
            }
            else
            {
                // No need to remember the dragged entity as it has no dependencies
                // from a contraint group
            }
        } 
        break;
    case AcDb::kDragEnd:
        {
            if (index != -1)
            {
                mEntityEntries[index]->mDragStat = AcDb::kDragEnd;

                if (mEntityEntries[index]->mEntityChangedShape)
                {
                    // Keep the entry around because the last evaluation also needs
                    // to be done with the cached origial entity geometries.
                    //
                    // The entry will be removed later on, when the top-level network 
                    // evaluation finishes. At that time, one more (static) evaluation of 
                    // the constraints is going to be requested, to evaluate the constraints
                    // with the changed entity shape
                }
                else
                {
                    // The constrained entity hasn't changed shape during dragging
                    // and stayed as a rigid body. it should have dragged correctly
                    // as constraints can drag rigid bodies
                    //
                    removeEntityEntry(pOrigEntity, index);

                }
            }
        }
        break;
    case AcDb::kDragAbort:
        {
            if (index != -1)
            {
                removeEntityEntry(pOrigEntity, index);
            }
        }
        break;
    }
}


void DraggedConstrainedRigidEntities::notifyEntityChangedShape(const AcDbEntity* pEntity)
{
    const int index = findEntityEntry(pEntity); // Either the original entity or a temporary clone
    if (index != -1)
    {
        mEntityEntries[index]->mEntityChangedShape = true;
    }
}


// There seems to be no general AutoCAD notification when a non-database-resident entity
// is deleted, so we need to let the custom entity explictly notify from its destructor
//
void DraggedConstrainedRigidEntities::notifyEntityDeleted(const AcDbEntity* pEntity)
{
    // Only react on non-database-resident clones produced by the dragger.
    // We cannot handle erase of databse-resident original entities well 
    // as after every drag sample undo is executed and it undoes the erase 
    // of the original entity and we would not be notified about it and have
    // no undo mechanism for DraggedConstrainedRigidEntities
    //
    if (!pEntity->objectId().isNull())
        return; 

    const int index = findEntityEntry(pEntity);
    if (index != -1)
    {
        mEntityEntries[index]->mpEntityClone = nullptr;
    }
}


void DraggedConstrainedRigidEntities::endTopLevelNetworkEvaluation(const AcDbAssocNetwork* pNetwork)
{
    for (int i = mEntityEntries.length()-1; i >= 0; i--)
    {
        EntityEntry* const pEntityEntry = mEntityEntries[i];

        for (int j = pEntityEntry->mConstraintGroupIds.length()-1; j >= 0; j--)
        {
            if (pEntityEntry->mConstraintGroupIds[j].database() == pNetwork->database())
            {
                pEntityEntry->mConstraintGroupIds.removeAt(j); // Done with constraint groups in this database
            }
        }

        for (int j = pEntityEntry->mConstraintDepIds.length()-1; j >= 0; j--)
        {
            if (pEntityEntry->mConstraintDepIds[j].database() == pNetwork->database())
            {
                // Request one more evaluation (this time static non-dragging evaluation)
                // when the rigid entity shape changed during dragging to evaluate the
                // constraints with the changed entity geometry
                //
                if (pEntityEntry->mEntityChangedShape)
                {
                    AcDbAssocManager::requestToEvaluate(pEntityEntry->mConstraintDepIds[j]);
                }
                pEntityEntry->mConstraintDepIds.removeAt(j);
            }
        }

        // If there were only constraint groups from this database, this entry
        // is no more needed as all the constraints have been solved
        //
        if (pEntityEntry->mConstraintGroupIds.isEmpty()) 
        {
            AcDbObjectPointer<AcDbEntity> pOrigEntity(pEntityEntry->mOrigEntityId, AcDb::kForRead);
            removeEntityEntry(pOrigEntity, i);
        }
    }
}


void DraggedConstrainedRigidEntities::cacheOrigSubentGeometry(const AcDbEntity* pEntity, const AcDbSubentId& subentId)
{
    if (subentId.type() != AcDb::kEdgeSubentType && subentId.type() != AcDb::kVertexSubentType)
        return;

    const int index = findEntityEntry(pEntity);
    if (index == -1)
        return;
    EntityEntry* const pEntityEntry = mEntityEntries[index];
    const Adesk::GsMarker subentIdIndex = subentId.index();

    switch (subentId.type())
    {
    case AcDb::kEdgeSubentType:
        if (!pEntityEntry->mOrigEdgeSubentIds.contains(subentIdIndex))
        {
            AcDbObjectPointer<AcDbEntity> pOrigEntity(pEntityEntry->mOrigEntityId, AcDb::kForRead);
            if (!eOkVerify(pOrigEntity.openStatus()))
                return;

            ConstrainedRigidEntityAssocPersSubentIdPE* const pWrapperPE 
                = ConstrainedRigidEntityAssocPersSubentIdPE::cast(pOrigEntity->queryX(AcDbAssocPersSubentIdPE::desc()));
            if (pWrapperPE == nullptr)
            {
                assert(!"ConstrainedRigidEntityAssocPersSubentIdPE not found on the entity");
                return;
            }
            AcDbAssocPersSubentIdPE* const pPE = pWrapperPE->getCustomEntityPE();
            AcGeCurve3d* pEdgeCurve = nullptr;
            Acad::ErrorStatus err = pPE->getEdgeSubentityGeometry(pOrigEntity, subentId, pEdgeCurve);
            if (!eOkVerify(err))
                return;

            pEntityEntry->mOrigEdgeSubentIds.append(subentIdIndex);
            pEntityEntry->mOrigEdgeSubentGeometries.append(pEdgeCurve);
        }
        break;
    case AcDb::kVertexSubentType:
        if (!pEntityEntry->mOrigVertexSubentIds.contains(subentIdIndex))
        {
            AcDbObjectPointer<AcDbEntity> pOrigEntity(pEntityEntry->mOrigEntityId, AcDb::kForRead);
            if (!eOkVerify(pOrigEntity.openStatus()))
                return;

            ConstrainedRigidEntityAssocPersSubentIdPE* const pWrapperPE 
                = ConstrainedRigidEntityAssocPersSubentIdPE::cast(pOrigEntity->queryX(AcDbAssocPersSubentIdPE::desc()));
            if (pWrapperPE == nullptr)
            {
                assert(!"ConstrainedRigidEntityAssocPersSubentIdPE not found on the entity");
                return;
            }
            AcDbAssocPersSubentIdPE* const pPE = pWrapperPE->getCustomEntityPE();
            AcGePoint3d vertexPosition;
            Acad::ErrorStatus err = pPE->getVertexSubentityGeometry(pOrigEntity, subentId, vertexPosition);
            if (!eOkVerify(err))
                return;

            pEntityEntry->mOrigVertexSubentIds.append(subentIdIndex);
            pEntityEntry->mOrigVertexSubentGeometries.append(vertexPosition);
        }
        break;
    default:
        ;
    }
}


Acad::ErrorStatus DraggedConstrainedRigidEntities::getOrigEdgeSubentityGeometry(const AcDbEntity*   pEntity, 
                                                                                const AcDbSubentId& edgeSubentId, 
                                                                                AcGeCurve3d*&       pEdgeCurve) const
{
    pEdgeCurve = nullptr;
    const int index = findEntityEntry(pEntity);
    if (index == -1)
    {
        assert(!"Entity not monitored for dragging");
        return Acad::eInvalidInput;
    }
    const EntityEntry* const pEntityEntry = mEntityEntries[index];
    const int i = pEntityEntry->mOrigEdgeSubentIds.find(edgeSubentId.index());
    if (i == -1)
    {
        assert(!"Original edge geometry not cached");
        return Acad::eDegenerateGeometry;
    }
    pEdgeCurve = (AcGeCurve3d*)pEntityEntry->mOrigEdgeSubentGeometries[i]->copy();
    return Acad::eOk;
}


Acad::ErrorStatus DraggedConstrainedRigidEntities::getOrigVertexSubentityGeometry(const AcDbEntity*   pEntity, 
                                                                                  const AcDbSubentId& vertexSubentId, 
                                                                                  AcGePoint3d&        vertexPosition) const
{
    vertexPosition = AcGePoint3d::kOrigin;
    const int index = findEntityEntry(pEntity);
    if (index == -1)
    {
        assert(!"Entity not monitored for dragging");
        return Acad::eInvalidInput;
    }
    const EntityEntry* const pEntityEntry = mEntityEntries[index];
    const int i = pEntityEntry->mOrigVertexSubentIds.find(vertexSubentId.index());
    if (i == -1)
    {
        assert(!"Original vertex geometry not cached");
        return Acad::eDegenerateGeometry;
    }
    vertexPosition = pEntityEntry->mOrigVertexSubentGeometries[i];
    return Acad::eOk;
}


Acad::ErrorStatus DraggedConstrainedRigidEntities::getOrigRigidSetTransform(const AcDbEntity* pEntity, AcGeMatrix3d& trans) const
{
    trans = AcGeMatrix3d::kIdentity;

    const int index = findEntityEntry(pEntity);
    if (index == -1)
    {
        assert(!"Entity not monitored for dragging");
        return Acad::eInvalidInput;
    }
    trans = mEntityEntries[index]->mOrigRigidSetTransform;
    return Acad::eOk;
}


void DraggedConstrainedRigidEntities::collectDependenciesFromConstraintGroups(
    const AcDbObjectId& origEntityId, 
    AcDbObjectIdArray&  constraintDepIds,
    AcDbObjectIdArray&  constraintGroupIds)
{
    constraintDepIds.removeAll();
    constraintGroupIds.removeAll();

    AcDbObjectPointer<AcDbEntity> pOrigEntity(origEntityId, AcDb::kForRead);
    if (!eOkVerify(pOrigEntity.openStatus()))
        return;

    AcDbObjectIdArray depIds;
    Acad::ErrorStatus err = AcDbAssocDependency::getDependenciesOnObject(pOrigEntity, true, true, depIds);
    if (!eOkVerify(err))
        return;

    for (int i = 0; i < depIds.length(); i++)
    {
        AcDbObjectPointer<AcDbAssocGeomDependency> pGeomDep(depIds[i], AcDb::kForRead);
        if (pGeomDep.openStatus() != Acad::eOk)
            continue; // Likely not a geom dependency
        const AcDbObjectId constraintGroupId = pGeomDep->owningAction();
        if (constraintGroupId.objectClass()->isDerivedFrom(AcDbAssoc2dConstraintGroup::desc()))
        {
            constraintDepIds.append(depIds[i]);
            if (constraintGroupIds.find(constraintGroupId) == -1)
                constraintGroupIds.append(constraintGroupId);
        }
    }
}


void DraggedConstrainedRigidEntities::notifyEntityCopied(const AcDbEntity* pOrigEntity, AcDbEntity* pEntityClone)
{
    const AcDbObjectId origEntityId = pOrigEntity->objectId();
    if (origEntityId.isNull())
        return; // Some other copy

    const int index = findEntityEntry(origEntityId);
    if (index != -1)
    {
        mEntityEntries[index]->mpEntityClone = pEntityClone;

        pEntityClone->addReactor(&sCurrentlyDraggedReactor);
    }
}


AcDbObjectId DraggedConstrainedRigidEntities::getOrigEntityIfChangedShape(const AcDbEntity* pEntity) const
{
    const int index = findEntityEntry(pEntity);
    if (index != -1)
    {
        const EntityEntry* const pEntityEntry = mEntityEntries[index];
        if (pEntityEntry->mEntityChangedShape)
            return pEntityEntry->mOrigEntityId;
    }
    return AcDbObjectId::kNull;
}


bool DraggedConstrainedRigidEntities::changedShape(const AcDbEntity* pEntity) const
{
    const int index = findEntityEntry(pEntity);
    if (index == -1)
        return false;
    return mEntityEntries[index]->mEntityChangedShape;
}


//////////////////////////////////////////////////////////////////////////////
//         class ConstrainedRigidEntityAssocPersSubentIdPE
//////////////////////////////////////////////////////////////////////////////

void ConstrainedRigidEntityAssocPersSubentIdPE::notifyEntityChangedShape(const AcDbEntity* pEntity)
{
    sCurrentlyDragged.notifyEntityChangedShape(pEntity);
}


void ConstrainedRigidEntityAssocPersSubentIdPE::notifyEntityDragStatus(const AcDbEntity* pOrigEntity, AcDb::DragStat dragStatus)
{
    sCurrentlyDragged.notifyEntityDragStatus(pOrigEntity, dragStatus, false);
}


void ConstrainedRigidEntityAssocPersSubentIdPE::notifyEntityDeleted(const AcDbEntity* pEntity)
{
    sCurrentlyDragged.notifyEntityDeleted(pEntity);
}


AcDbAssocPersSubentId* ConstrainedRigidEntityAssocPersSubentIdPE::createNewPersSubent(
    AcDbEntity*                 pEntity,
    AcDbDatabase*               pDatabase,
    const AcDbCompoundObjectId& compId,
    const AcDbSubentId&         subentId)
{
    return mpCustomEntityPE->createNewPersSubent(pEntity, pDatabase, compId, subentId);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::releasePersSubent(
    AcDbEntity*                  pEntity, 
    AcDbDatabase*                pDatabase,
    const AcDbAssocPersSubentId* pPerSubentId)
{
    return mpCustomEntityPE->releasePersSubent(pEntity, pDatabase, pPerSubentId);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::transferPersSubentToAnotherEntity(
        AcDbEntity*                  pToEntity,
        AcDbDatabase*                pToDatabase,
        AcDbEntity*                  pFromEntity,
        AcDbDatabase*                pFromDatabase,
        const AcDbAssocPersSubentId* pPersSubentId)
{
    return mpCustomEntityPE->transferPersSubentToAnotherEntity(pToEntity, pToDatabase, pFromEntity, pFromDatabase, pPersSubentId);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::makePersSubentPurgeable(
    AcDbEntity*                  pEntity, 
    AcDbDatabase*                pDatabase,
    const AcDbAssocPersSubentId* pPerSubentId,
    bool                         yesNo)
{
    return mpCustomEntityPE->makePersSubentPurgeable(pEntity, pDatabase, pPerSubentId, yesNo);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getTransientSubentIds(
    const AcDbEntity*            pEntity, 
    AcDbDatabase*                pDatabase,
    const AcDbAssocPersSubentId* pPersSubentId,
    AcArray<AcDbSubentId>&       subents) const
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    const AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getTransientSubentIds(pEntityToUse, pDatabase, pPersSubentId, subents);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getAllSubentities(
    const AcDbEntity*      pEntity,
    AcDb::SubentType       subentType,
    AcArray<AcDbSubentId>& allSubentIds)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    const AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getAllSubentities(pEntityToUse, subentType, allSubentIds);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getAllSubentities(
    const AcDbEntity*      pEntity,
    const AcRxClass*       pSubentClass,
    AcArray<AcDbSubentId>& allSubentIds)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    const AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getAllSubentities(pEntityToUse, pSubentClass, allSubentIds);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getEdgeVertexSubentities(
    const AcDbEntity*      pEntity,
    const AcDbSubentId&    edgeSubentId,
    AcDbSubentId&          startVertexSubentId,
    AcDbSubentId&          endVertexSubentId,
    AcArray<AcDbSubentId>& otherVertexSubentIds)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    const AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getEdgeVertexSubentities(pEntityToUse, edgeSubentId, startVertexSubentId, endVertexSubentId, otherVertexSubentIds);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getSplineEdgeVertexSubentities(
    const AcDbEntity*      pEntity,
    const AcDbSubentId&    edgeSubentId,
    AcDbSubentId&          startVertexSubentId,
    AcDbSubentId&          endVertexSubentId,
    AcArray<AcDbSubentId>& controlPointSubentIds,
    AcArray<AcDbSubentId>& fitPointSubentIds)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    const AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getSplineEdgeVertexSubentities(pEntityToUse, edgeSubentId, startVertexSubentId, endVertexSubentId, controlPointSubentIds, fitPointSubentIds);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getVertexSubentityGeometry(
    const AcDbEntity*   pEntity,
    const AcDbSubentId& vertexSubentId,
    AcGePoint3d&        vertexPosition)
{
    sCurrentlyDragged.cacheOrigSubentGeometry(pEntity, vertexSubentId);
    if (sCurrentlyDragged.changedShape(pEntity))
    {
        return sCurrentlyDragged.getOrigVertexSubentityGeometry(pEntity, vertexSubentId, vertexPosition);
    }
    else
    {
        return mpCustomEntityPE->getVertexSubentityGeometry(pEntity, vertexSubentId, vertexPosition);
    }
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getEdgeSubentityGeometry(
    const AcDbEntity*   pEntity,
    const AcDbSubentId& edgeSubentId,
    AcGeCurve3d*&       pEdgeCurve)
{
    sCurrentlyDragged.cacheOrigSubentGeometry(pEntity, edgeSubentId);
    if (sCurrentlyDragged.changedShape(pEntity))
    {
        return sCurrentlyDragged.getOrigEdgeSubentityGeometry(pEntity, edgeSubentId, pEdgeCurve);
    }
    else
    {
        return mpCustomEntityPE->getEdgeSubentityGeometry(pEntity, edgeSubentId, pEdgeCurve);
    }
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getFaceSubentityGeometry(
    const AcDbEntity*   pEntity,
    const AcDbSubentId& faceSubentId,
    AcGeSurface*&       pFaceSurface)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    const AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getFaceSubentityGeometry(pEntityToUse, faceSubentId, pFaceSurface);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getSubentityGeometry(
    const AcDbEntity*   pEntity,
    const AcDbSubentId& subentId,
    AcGeEntity3d*&      pSubentityGeometry)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    const AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getSubentityGeometry(pEntityToUse, subentId, pSubentityGeometry);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getFaceSilhouetteGeometry(
    AcDbEntity*                  pEntity,
    AcRxObject*                  pContext,
    const AcDbSubentId&          faceSubentId,
    const AcGeMatrix3d*          pEntityTransform,
    class AcDbGeometryProjector* pGeometryProjector,
    AcArray<int>&                transientSilhIds, // Either in or out argument
    AcArray<AcGeCurve3d*>&       silhCurves)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getFaceSilhouetteGeometry(pEntityToUse, pContext, faceSubentId, pEntityTransform, pGeometryProjector, transientSilhIds, silhCurves);
}


AcDbAssocPersSubentId* ConstrainedRigidEntityAssocPersSubentIdPE::createNewPersFaceSilhouetteId(
    AcDbEntity*                  pEntity,
    AcDbDatabase*                pDatabase,
    AcRxObject*                  pContext,
    const AcDbCompoundObjectId&  compId,
    const AcDbSubentId&          faceSubentId,
    const AcGeMatrix3d*          pEntityTransform,
    class AcDbGeometryProjector* pGeometryProjector,
    int                          transientSilhId)
{
    return mpCustomEntityPE->createNewPersFaceSilhouetteId(pEntity, pDatabase, pContext, compId, faceSubentId, pEntityTransform, pGeometryProjector, transientSilhId);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getTransientFaceSilhouetteIds(
    AcDbEntity*                  pEntity,
    AcDbDatabase*                pDatabase,
    AcRxObject*                  pContext,
    const AcDbSubentId&          faceSubentId,
    const AcGeMatrix3d*          pEntityTransform,
    class AcDbGeometryProjector* pGeometryProjector,
    const AcDbAssocPersSubentId* pPersSilhId,
    AcArray<int>&                transientSilhIds)
{
    AcDbObjectPointer<AcDbEntity> origEntity(sCurrentlyDragged.getOrigEntityIfChangedShape(pEntity), AcDb::kForRead);
    AcDbEntity* const pEntityToUse = origEntity.openStatus() == Acad::eOk ? origEntity : pEntity;

    return mpCustomEntityPE->getTransientFaceSilhouetteIds(pEntityToUse, pDatabase, pContext, faceSubentId, pEntityTransform, pGeometryProjector, pPersSilhId, transientSilhIds);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::mirrorPersSubent(
    const AcDbEntity*      pMirroredEntity,
    AcDbDatabase*          pDatabase,
    AcDbAssocPersSubentId* pPersSubentIdToMirror)
{
    return mpCustomEntityPE->mirrorPersSubent(pMirroredEntity, pDatabase, pPersSubentIdToMirror);
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::getRigidSetTransform(
    const AcDbEntity* pEntity,
    AcGeMatrix3d&     trans)
{
    if (sCurrentlyDragged.changedShape(pEntity))
    {
        return sCurrentlyDragged.getOrigRigidSetTransform(pEntity, trans);
    }
    else
    {
        return mpCustomEntityPE->getRigidSetTransform(pEntity, trans);
    }
}


Acad::ErrorStatus ConstrainedRigidEntityAssocPersSubentIdPE::setRigidSetTransform(
    AcDbEntity*         pEntity, 
    const AcGeMatrix3d& newTrans)
{
    assert((newTrans.scale() - 1.0) < 1e-6);

    // If the original (unchanged) entity should be used because the entity 
    // changed shape during dragging, do not let constraints change the entity
    //
    if (sCurrentlyDragged.changedShape(pEntity))
        return Acad::eOk; // Ignore the setRigidSetTransform() call

    AcGeMatrix3d currentTrans;
    Acad::ErrorStatus err = mpCustomEntityPE->getRigidSetTransform(pEntity, currentTrans);
    if (!eOkVerify(err))
        return err;

    return pEntity->transformBy(newTrans * currentTrans.inverse());
}


static bool sIsRegistered = false;

void ConstrainedRigidEntityAssocPersSubentIdPE::registerClasses()
{
    if (!sIsRegistered)
    {
        ConstrainedRigidEntityAssocPersSubentIdPE::rxInit();
        DraggedConstrainedRigidEntitiesReactor::rxInit();
        sIsRegistered = true;
    }
}


void ConstrainedRigidEntityAssocPersSubentIdPE::unregisterClasses()
{
    sCurrentlyDragged.reset();

    if (sIsRegistered)
    {
        deleteAcRxClass(ConstrainedRigidEntityAssocPersSubentIdPE::desc());
        deleteAcRxClass(DraggedConstrainedRigidEntitiesReactor::desc());
        sIsRegistered = false;
    }
}


// Attaches AcDbAssocAnnotationPE to the custom entity
//
Acad::ErrorStatus AssocAnnotationsEnabler::enableForEntity(AcRxClass* pEntityClass)
{
    if (pEntityClass == nullptr || !pEntityClass->isDerivedFrom(AcDbEntity::desc()))
        return Acad::eNotThatKindOfClass;

    AcRxClass* const pAssocAnnotationPEClass = AcRxClass::cast(acrxClassDictionary->at(L"AcDbAssocAnnotationPE"));

    if (pEntityClass->queryX(pAssocAnnotationPEClass) == nullptr)
    {
        AcRxObject* const pAssocAnnotationPE = pAssocAnnotationPEClass->create();
        pEntityClass->addX(pAssocAnnotationPEClass, pAssocAnnotationPE);
    }
    return Acad::eOk;
}


// Detaches AcDbAssocAnnotationPE from the custom entity
//
Acad::ErrorStatus AssocAnnotationsEnabler::disableForEntity(AcRxClass* pEntityClass)
{
    if (pEntityClass == nullptr || !pEntityClass->isDerivedFrom(AcDbEntity::desc()))
        return Acad::eNotThatKindOfClass;

    AcRxClass*  const pAssocAnnotationPEClass = AcRxClass::cast(acrxClassDictionary->at(L"AcDbAssocAnnotationPE"));
    AcRxObject* const pAssocAnnotationPE      = pEntityClass->queryX(pAssocAnnotationPEClass);

    if (pAssocAnnotationPE != nullptr && pAssocAnnotationPE->isA() == pAssocAnnotationPEClass)
    {
        pEntityClass->delX(pAssocAnnotationPEClass);
        delete pAssocAnnotationPE;
    }
    return Acad::eOk;
}
