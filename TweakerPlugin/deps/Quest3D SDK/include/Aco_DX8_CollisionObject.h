#if !defined(AFX_DX8_COLLISIONOBJECT_H__F73655E4_5340_4AF5_947C_AC1F0089C664__INCLUDED_)
#define AFX_DX8_COLLISIONOBJECT_H__F73655E4_5340_4AF5_947C_AC1F0089C664__INCLUDED_

#ifdef DX8_COLLISIONOBJECT_EXPORTS
#define DX8_COLLISIONOBJECT_API __declspec(dllexport)
#else
#define DX8_COLLISIONOBJECT_API __declspec(dllimport)
#endif

#define DX8_COLLISIONOBJECT_CHANNEL_NAME		"CollisionObject"

// {8C3D0983-CC73-4A3D-AB5A-9D40D9FD6E1D} 
static const GUID DX8_COLLISIONOBJECT_CHANNEL_GUID = { 0x8c3d0983, 0xcc73, 0x4a3d, { 0xab, 0x5a, 0x9d, 0x40, 0xd9, 0xfd, 0x6e, 0x1d } };

struct PolygonInfoItem {
	D3DXVECTOR3				vec[3];
	WORD					indexNr;
	D3DXVECTOR3				edge1;
	D3DXVECTOR3				edge2;
	D3DXVECTOR3				normal;
	// which polynr are we
	int						polyNr;
	// surfaceNr
	int						surfaceNr;
};

class BSPTree;
class Aco_DX8_DirectGraphicsChannel;

class DX8_COLLISIONOBJECT_API Aco_DX8_CollisionObject : public A3d_Channel
{
public:
	
	Aco_DX8_CollisionObject();
	virtual ~Aco_DX8_CollisionObject();
	// CallChannel derived interface
	virtual void			CallChannel();

	// a channel must save itself with the filesaver class
	virtual	bool			SaveChannel(A3dFileSaver& saver);
	// and load one from disk
	virtual bool			LoadChannel(A3dFileLoader& loader, A3d_ChannelGroup *group);
	// Release
	virtual void			Release();
	// we build a bsp tree of the object that is linked to us with no world rotation
	virtual bool			BuildBSPTree();
	// BuildPolygonList
	virtual bool			BuildPolygonList(D3DXVECTOR3 *minBox, D3DXVECTOR3 *maxBox);
	// other channels can ask us with a ray what the collision is
	virtual float			GetRayCollisionWithNode(D3DXVECTOR3 *position, D3DXVECTOR3 *movement, float movementLength, BSPTree* bspNode, float &oldIntersectionLength, D3DXVECTOR3 *collisionPos=NULL, D3DXVECTOR3 *collisionNormal=NULL, bool getClosestCollision=false);
	// other channels can ask us with a ray what the collision is
	virtual float			GetRayCollision(D3DXVECTOR3 position, D3DXVECTOR3 movement, D3DXVECTOR3 *collisionPos=NULL, D3DXVECTOR3 *collisionNormal=NULL, bool getClosestCollision=false);
	// check if a plane intersects a node
	virtual int				GetPlaneCollisionWithNode(D3DXPLANE* plane,  BSPTree* bspNode);
	// check if a plane intersects this object
	virtual int				GetPlaneCollision(D3DXVECTOR3* p1, D3DXVECTOR3* p2, D3DXVECTOR3* p3);
	// LinePieceAABB
	virtual bool			LinePieceAABB(D3DXVECTOR3 *minBox, D3DXVECTOR3 *maxBox, D3DXVECTOR3 *position, D3DXVECTOR3 *direction, float vectorLength);
	// we can also calculate collision with a sphere (i hope)
	virtual bool			GetIfSphereCollision(D3DXVECTOR3 position, float radius);
	// we can also calculate collision with a sphere (i hope)
	virtual bool			GetIfSphereCollisionWithNode(D3DXVECTOR3 *position, float radius, float sqrRadius, BSPTree* bspNode);
	// GetTransformedMinMaxBox
	virtual void			GetTransformedMinMaxBox(D3DXMATRIX *worldMatrix, D3DXVECTOR3 &boundingBoxMin, D3DXVECTOR3 &boundingBoxMax);
	// we can also calculate collision with a sphere (i hope)
	virtual bool			GetIfSphereCollisionWithNodeWithMatrix(D3DXVECTOR3 *position, float radius, float sqrRadius, BSPTree* bspTreeNode, D3DXMATRIX *objectMatrix);

	// helper function to test funcationality
	virtual void			DrawLine(D3DXVECTOR3 start, D3DXVECTOR3 end);
		
	// SqrDistancePointPoly
	virtual float			SqrDistancePointPoly (D3DXVECTOR3 rkPoint, PolygonInfoItem* rkTri);
	// Square
	virtual float			Square(float value);
	// we want to be able to set some settings for the bsp tree
	virtual void			SetTreeDepth(int newValue);
	// GetTreeDepth
	virtual int				GetTreeDepth();
	// we want to be able to set some settings for the bsp tree
	virtual void			SetTreePolyLeafCount(int newValue);
	// GetTreePolyLeafCount
	virtual int				GetTreePolyLeafCount();
	// when the tree is calculated lets get these settings
	virtual int				GetCreationTime();
	// get how much memory the tree takes
	virtual int				GetTreeMemorySize();
	// get how much memory the tree takes
	virtual int				GetTreeNodeCount();
	// get if valid tree
	virtual bool			GetIfValidTree();
	// get if valid tree
	virtual int				GetAvarageLeafPolyCount();
	// get the world of our current object
	virtual D3DXMATRIX		GetObjectWorld();
	// AddBoxCollisionPolygonsToList
	virtual void			AddBoxCollisionPolygonsToList(A3d_List* polyList, D3DXVECTOR3 *vecMin, D3DXVECTOR3 *vecMax);
	// AddBoxCollisionPolygonsToListFromNode
	virtual void			AddBoxCollisionPolygonsToListFromNode(A3d_List* polyList, D3DXVECTOR3 *vecMin, D3DXVECTOR3 *vecMax, BSPTree* bspTreeNode);
	// PolygonInfoItem*
	virtual PolygonInfoItem* GetPolygonInfoItem(DWORD itemNr);
	// Get minBox from first node
	virtual D3DXVECTOR3		GetMinBoxFirstNode();
	// Get minBox from first node
	virtual D3DXVECTOR3		GetMaxBoxFirstNode(); 
	// triBoxOverlap
	virtual int				triBoxOverlap(float boxcenter[3],float boxhalfsize[3],float triverts[3][3]);
	// planeBoxOverlap
	virtual int				planeBoxOverlap(float normal[3],float d, float maxbox[3]);
	// usePolygonCulling_
	virtual void			SetUsePolygonCulling(bool newValue);
	// GetUsePolygonCulling
	virtual bool			GetUsePolygonCulling();
	// get check object update
	virtual bool			GetIfCheckObjectUpdate();
	// set check object update
	virtual void			SetIfCheckObjectUpdate(bool newValue);
	// UpdateCollisionTree
	virtual void			UpdateCollisionTree();
	// GetTextureColorOfLastIntersection
	virtual D3DXVECTOR3		GetTextureColorOfLastIntersection();
	// get polygon item nr of last intersection only works with rayintersection!
	virtual int				GetPolygonItenNrOfLastIntersection();
	// GetFeedbackClass
	virtual ChannelFeedback* GetFeedbackClass();

protected:
	// BSPTree
	BSPTree*				bspTreeMainNode_;
	// list of polygons the octtree points at
	A3d_List				polygonInfoList_;
	// polylist is ready
	bool					validBSPTree_;
	// usePolygonCulling_
	bool					usePolygonCulling_;
	// timestamp object
	DWORD					timeStamp_;
	// tree depth
	int						treeDepthValue_;
	// tree leaf count
	int						treePolygonLeafCount_;
	// time it took to create tree
	int						treeCreationTimeMs_;
	// memory usage
	int						treeMemoryUse_;
	// node count
	int						treeNodeCount_;
	// avarage node leaf count
	int						avarageLeafSize_;
	// addition list
	A3d_List				polyAdditionTimeList_;
	// current time
	DWORD					currentTick_;
	// use check object update
	bool					checkForObjectUpdate_;
	// update time
	DWORD					objectDataTimeStamp_;
	
	//used for checking a node against a side
	int						planeSide_;

	//a valid vertex of the mesh
	D3DXVECTOR3				validVectorPos_;
	Aco_DX8_DirectGraphicsChannel* directGraphics_;
	// which polygon was the last with collision
	int						lastPolyItemCollisionNr_;
	float					TUCollision_;
	float					TVCollision_;
	// channelFeedback_
	ChannelFeedback*			channelFeedback_;
};

// Leave rest of header file intact!
#define DX8_COLLISIONOBJECTDLL_EXPORTS extern "C" { \
__declspec(dllexport) DllInterface * __cdecl InitDLL() \
{ \
	return new Aco_DX8_CollisionObject; \
} \
}

#endif // !defined(AFX_DX8_COLLISIONOBJECT_H__F73655E4_5340_4AF5_947C_AC1F0089C664__INCLUDED_)
