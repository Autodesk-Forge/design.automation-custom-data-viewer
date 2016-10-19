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
// mgPoly.h

#pragma once
#include <tchar.h>
using namespace System;
using namespace Autodesk::AutoCAD::Geometry;
using namespace Autodesk::AutoCAD::DatabaseServices;

namespace Autodesk {
    namespace ObjectDbxSample
    {
        [Autodesk::AutoCAD::Runtime::Wrapper("AsdkPoly")]
        public ref class Poly :  public Autodesk::AutoCAD::DatabaseServices::Curve
        {

        public:
            Poly();

        internal:
            Poly(System::IntPtr unmanagedPointer, bool autoDelete);
            inline AsdkPoly*  GetImpObj()
            {
                return static_cast<AsdkPoly*>(UnmanagedObject.ToPointer());
            }

        public:
            property Point2d Center
            {
                void set(Point2d point);
                Point2d get();
            }

            property Point2d StartPoint2d
            {
                void set(Point2d point);
                Point2d get();
            }
            property long NumberOfSides
            {
                void set(long value);
                long get();
            }
            property Vector3d Normal
            {
                void set(Vector3d vector);
                Vector3d get();
            }
        
            property double Elevation
            {
                void set(double value);
                double get();
            }
            
            property String^ Name
            {
                void set(String^ value);
                String^ get();
            }
        
            property Autodesk::AutoCAD::DatabaseServices::ObjectId TextStyle
            {
                void set(Autodesk::AutoCAD::DatabaseServices::ObjectId value);
                Autodesk::AutoCAD::DatabaseServices::ObjectId get();
            }

            property  Point2dCollection^ Vertices
            {
                Point2dCollection^ get();
            }


        };
    }

}