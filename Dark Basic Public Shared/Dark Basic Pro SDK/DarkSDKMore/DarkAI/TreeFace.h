#ifndef TREEFACE_H
#define TREEFACE_H

#include "Point.h"
#include "Vector.h"

//stripped down face data for collision only, vertex position + face normal + face plane distance d
class TreeFace
{
    public:    
        AIPoint vert1,vert2,vert3;
        Vector normal;
        float d;
        //bool collisionon;
        int iID;
        TreeFace* nextFace;

		TreeFace( ) { };
		virtual ~TreeFace( ) { };

		bool MakeFace( AIPoint *p1, AIPoint *p2, AIPoint *p3, int id );
        
        virtual bool Intersects( const AIPoint* p, const Vector* v, int dir, float *dist ) const;
		bool PointInPoly( const AIPoint* p ) const;
		
    private:
};

#endif
