// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MNM_MESH_GRID_H
#define __MNM_MESH_GRID_H

#pragma once

#include "MNM.h"
#include "Tile.h"
#include "MNMProfiler.h"
#include "Islands.h"
#include "DangerousAreas.h"

#include <CryMath/SimpleHashLookUp.h>
#include <CryCore/Containers/VectorMap.h>

#include <CryAISystem/NavigationSystem/MNMNavMesh.h>
#include <CryAISystem/NavigationSystem/INavigationQuery.h>

class OffMeshNavigationManager;

namespace MNM
{
struct OffMeshNavigation;
class CTileConnectivityData;

struct SWayQueryWorkingSet;
struct SWayQueryResult;
struct SWayQueryRequest;

///////////////////////////////////////////////////////////////////////

class CNavMesh : public MNM::INavMesh
{
	friend class CIslands;

public:
	enum { x_bits = 11, };   // must add up to a 32 bit - the "tileName"
	enum { y_bits = 11, };
	enum { z_bits = 10, };

	enum { max_x = (1 << x_bits) - 1, };
	enum { max_y = (1 << y_bits) - 1, };
	enum { max_z = (1 << z_bits) - 1, };

	struct SGridParams
	{
		SGridParams()
			: origin(ZERO)
			, tileSize(16)
			, tileCount(1024)
			, voxelSize(0.1f)
		{
		}

		Vec3   origin;
		Vec3i  tileSize;
		Vec3   voxelSize;
		uint32 tileCount;
	};

	enum { SideCount = 14, };

	enum EPredictionType
	{
		ePredictionType_TriangleCenter = 0,
		ePredictionType_Advanced,
		ePredictionType_Latest,
	};

	enum EWayQueryResult
	{
		eWQR_Continuing = 0,
		eWQR_Done,
	};

	struct SWayQueryRequest
	{
		SWayQueryRequest(EntityId _requesterEntityId, TriangleID _from, const vector3_t& _fromLocation, TriangleID _to,
			const vector3_t& _toLocation, const OffMeshNavigation& _offMeshNavigation, const OffMeshNavigationManager& _offMeshNavigationManager,
			const DangerousAreasList& dangerousAreas, const INavMeshQueryFilter* pFilter, const MNMCustomPathCostComputerSharedPtr& _pCustomPathCostComputer)
			: m_from(_from)
			, m_to(_to)
			, m_fromLocation(_fromLocation)
			, m_toLocation(_toLocation)
			, m_offMeshNavigation(_offMeshNavigation)
			, m_offMeshNavigationManager(_offMeshNavigationManager)
			, m_dangerousAreas(dangerousAreas)
			, m_pCustomPathCostComputer(_pCustomPathCostComputer)
			, m_requesterEntityId(_requesterEntityId)
			, m_pFilter(pFilter)
		{}

		virtual ~SWayQueryRequest() {}

		virtual bool                    CanUseOffMeshLink(const OffMeshLinkID linkID, float* costMultiplier) const;
		bool                            IsPointValidForAgent(const Vec3& pos, uint32 flags) const { return true; }

		ILINE const TriangleID          From() const { return m_from; }
		ILINE const TriangleID          To() const { return m_to; }
		ILINE const OffMeshNavigation&  GetOffMeshNavigation() const { return m_offMeshNavigation; }
		ILINE const DangerousAreasList& GetDangersInfos() { return m_dangerousAreas; }
		ILINE const vector3_t&          GetFromLocation() const { return m_fromLocation; };
		ILINE const vector3_t&          GetToLocation() const { return m_toLocation; };
		ILINE const MNMCustomPathCostComputerSharedPtr& GetCustomPathCostComputer() const { return m_pCustomPathCostComputer; }  // might be nullptr (which is totally OK)
		ILINE const INavMeshQueryFilter* GetFilter() const { return m_pFilter; }

	private:
		const TriangleID                m_from;
		const TriangleID                m_to;
		const vector3_t                 m_fromLocation;
		const vector3_t                 m_toLocation;
		const OffMeshNavigation&        m_offMeshNavigation;
		const OffMeshNavigationManager& m_offMeshNavigationManager;
		DangerousAreasList              m_dangerousAreas;
		MNMCustomPathCostComputerSharedPtr m_pCustomPathCostComputer;
		const INavMeshQueryFilter*      m_pFilter;
	protected:
		const EntityId                  m_requesterEntityId;
	};

	CNavMesh();
	~CNavMesh();

	void                 Init(const SGridParams& params, const SAgentSettings& agentSettings);

	static inline size_t ComputeTileName(size_t x, size_t y, size_t z)
	{
		return (x & ((1 << x_bits) - 1)) |
		       ((y & ((1 << y_bits) - 1)) << x_bits) |
		       ((z & ((1 << z_bits) - 1)) << (x_bits + y_bits));
	}

	static inline void ComputeTileXYZ(size_t tileName, size_t& x, size_t& y, size_t& z)
	{
		x = tileName & ((1 << x_bits) - 1);
		y = (tileName >> x_bits) & ((1 << y_bits) - 1);
		z = (tileName >> (x_bits + y_bits)) & ((1 << z_bits) - 1);
	}

	inline vector3_t ToWorldSpace(const vector3_t& localPosition) const
	{
		return localPosition + vector3_t(m_params.origin);
	}
	inline vector3_t ToMeshSpace(const vector3_t& worldPosition) const
	{
		return worldPosition - vector3_t(m_params.origin);
	}
	inline aabb_t ToMeshSpace(const aabb_t& worldAabb) const
	{
		const vector3_t origin = vector3_t(m_params.origin);
		return aabb_t(worldAabb.min - origin, worldAabb.max - origin);
	}

	size_t     GetTriangles(aabb_t localAabb, TriangleID* triangles, size_t maxTriCount, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const;
	TriangleID GetTriangleAt(const vector3_t& localPosition, const real_t verticalDownwardRange, const real_t verticalUpwardRange, const INavMeshQueryFilter* pFilter, float minIslandArea = 0.f) const;
	TriangleID GetClosestTriangle(const vector3_t& localPosition, real_t vrange, real_t hrange, const INavMeshQueryFilter* pFilter, real_t* distance = nullptr, vector3_t* closest = nullptr, float minIslandArea = 0.f) const;
	TriangleID GetClosestTriangle(const vector3_t& localPosition, const aabb_t& aroundPositionAABB, const INavMeshQueryFilter* pFilter, real_t* distance = nullptr, vector3_t* closest = nullptr, const real_t maxDistance = real_t::max(), const float minIslandArea = 0.f) const;

	bool IsTriangleAcceptableForLocation(const vector3_t& localPosition, TriangleID triangleID) const;

	bool GetVertices(TriangleID triangleID, vector3_t& v0, vector3_t& v1, vector3_t& v2) const;
	bool GetVertices(TriangleID triangleID, vector3_t* verts) const;
	// Sets a bit in linkedEdges for each edge on this triangle that has a link.
	// Bit 0: v0->v1, Bit 1: v1->v2, Bit 2: v2->v0
	bool          GetLinkedEdges(TriangleID triangleID, size_t& linkedEdges) const;
	bool          GetTriangle(TriangleID triangleID, Tile::STriangle& triangle) const;

	size_t        GetMeshBorders(const aabb_t& localAabb, const INavMeshQueryFilter* pFilter, Vec3* pBorders, size_t maxBorderCount, float minIslandArea = 0.f) const;

	bool          PushPointInsideTriangle(const TriangleID triangleID, vector3_t& localPosition, real_t amount) const;


	inline size_t GetTriangleCount() const
	{
		return m_triangleCount;
	}

	void                      MarkTrianglesNotConnectedToSeeds(const MNM::AreaAnnotation::value_type flags);

	void                      AddOffMeshLinkToTile(const TileID tileID, const TriangleID triangleID, const uint16 offMeshIndex);
	void                      UpdateOffMeshLinkForTile(const TileID tileID, const TriangleID triangleID, const uint16 offMeshIndex);
	void                      RemoveOffMeshLinkFromTile(const TileID tileID, const TriangleID triangleID);

	CNavMesh::EWayQueryResult FindWay(SWayQueryRequest& inputRequest, SWayQueryWorkingSet& workingSet, SWayQueryResult& result) const;

	real_t                    CalculateHeuristicCostForCustomRules(const vector3_t& locationComingFrom, const vector3_t& locationGoingTo, const Vec3& meshOrigin, const IMNMCustomPathCostComputer* pCustomPathCostComputer) const;
	void                      PullString(const vector3_t& fromLocalPosition, const TriangleID fromTriID, const vector3_t& toLocalPosition, const TriangleID toTriID, vector3_t& middleLocalPosition) const;

	struct RayHit
	{
		TriangleID triangleID;
		real_t     distance;
		size_t     edge;
	};

	/*
	 ********************************************************************************************
	   RayCastRequestBase holds the actual request information needed to perform a RayCast request.
	   It needs to be constructed through the RayCastRequest that assures the presence of the way.
	 ********************************************************************************************
	 */

	struct RaycastRequestBase
	{
	protected:
		// This class can't be instantiated directly.
		// cppcheck-suppress uninitMemberVar
		RaycastRequestBase(const size_t _maxWayTriCount)
			: maxWayTriCount(_maxWayTriCount)
			, way(NULL)
			, wayTriCount(0)
			, result(ERayCastResult::NoHit)
		{}

	public:
		RayHit         hit;
		TriangleID*    way;
		size_t         wayTriCount;
		const size_t   maxWayTriCount;
		ERayCastResult result;
	};

	template<size_t MaximumNumberOfTrianglesInWay>
	struct RayCastRequest : public RaycastRequestBase
	{
		RayCastRequest()
			: RaycastRequestBase(MaximumNumberOfTrianglesInWay)
		{
			way = &(wayArray[0]);
			wayTriCount = 0;
		}
	private:
		TriangleID wayArray[MaximumNumberOfTrianglesInWay];
	};

	// ********************************************************************************************

	ERayCastResult RayCast(const vector3_t& fromLocalPosition, TriangleID fromTri, const vector3_t& toLocalPosition, TriangleID toTri,
	                       RaycastRequestBase& wayRequest, const INavMeshQueryFilter* filter) const;

	ERayCastResult RayCast_v1(const vector3_t& fromLocalPosition, TriangleID frfromTriangleIDomTri, const vector3_t& toLocalPosition, TriangleID toTriangleID, RaycastRequestBase& wayRequest) const;
	ERayCastResult RayCast_v2(const vector3_t& fromLocalPosition, TriangleID fromTriangleID, const vector3_t& toLocalPosition, TriangleID toTriangleID, RaycastRequestBase& wayRequest) const;
	
	template<typename TFilter>
	ERayCastResult RayCast_v3(const vector3_t& fromLocalPosition, TriangleID fromTriangleID, const vector3_t& toLocalPosition, TriangleID toTriangleID, const TFilter& filter, RaycastRequestBase& wayRequest) const;

	TileID         SetTile(size_t x, size_t y, size_t z, STile& tile);
	void           ClearTile(TileID tileID, bool clearNetwork = true);

	void           CreateNetwork();
	void           ConnectToNetwork(const TileID tileID, const CTileConnectivityData* pConnectivityData);

	inline bool    Empty() const
	{
		return m_tiles.GetTileCount() == 0;
	}

	inline size_t GetTileCount() const
	{
		return m_tiles.GetTileCount();
	}

	TileID GetTileID(size_t x, size_t y, size_t z) const;
	const STile&    GetTile(TileID) const;
	STile&          GetTile(TileID);
	const vector3_t GetTileContainerCoordinates(TileID) const;

	void                      Swap(CNavMesh& other);

	inline const SGridParams& GetGridParams() const
	{
		return m_params;
	}

	inline void OffsetOrigin(const Vec3& offset)
	{
		m_params.origin += offset;
	}

	void Draw(size_t drawFlags, const ITriangleColorSelector& colorSelector, TileID excludeID = 0) const;

	bool CalculateMidEdge(const TriangleID triangleID1, const TriangleID triangleID2, Vec3& result) const;

	enum ProfilerTimers
	{
		NetworkConstruction = 0,
	};

	enum ProfilerMemoryUsers
	{
		TriangleMemory = 0,
		VertexMemory,
		BVTreeMemory,
		LinkMemory,
		GridMemory,
	};

	enum ProfilerStats
	{
		TileCount = 0,
		TriangleCount,
		VertexCount,
		BVTreeNodeCount,
		LinkCount,
	};

	typedef MNMProfiler<ProfilerMemoryUsers, ProfilerTimers, ProfilerStats> ProfilerType;
	const ProfilerType& GetProfiler() const;

	CIslands& GetIslands() { return m_islands; }
	const CIslands& GetIslands() const { return m_islands; }

	TileID GetNeighbourTileID(size_t x, size_t y, size_t z, size_t side) const;
	TileID GetNeighbourTileID(const TileID tileId, size_t side) const;

	void SetTrianglesAnnotation(const MNM::TriangleID* pTrianglesArray, const size_t trianglesCount, const MNM::AreaAnnotation areaAnnotation, std::vector<TileID>& affectedTiles);
	bool SnapPosition(const vector3_t& localPosition, const SOrderedSnappingMetrics& snappingMetrics, const INavMeshQueryFilter* pFilter, vector3_t* pSnappedLocalPosition, MNM::TriangleID* pTriangleId) const;
	bool SnapPosition(const vector3_t& localPosition, const MNM::SSnappingMetric& snappingMetric, const INavMeshQueryFilter* pFilter, vector3_t* pSnappedLocalPosition, MNM::TriangleID* pTriangleId) const;

	// MNM::INavMesh
	virtual void       GetMeshParams(NavMesh::SParams& outParams) const override;
	virtual TileID     FindTileIDByTileGridCoord(const vector3_t& tileGridCoord) const override;
	virtual size_t     QueryTriangles(const aabb_t& queryLocalAabb, const INavMeshQueryFilter* pOptionalFilter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const override;
	virtual void       QueryTrianglesWithProcessing(const aabb_t& queryAabbWorld, const INavMeshQueryFilter* pFilter, INavMeshQueryProcessing* pQueryProcessing) const override;
	virtual TriangleID FindClosestTriangle(const vector3_t& localPosition, const TriangleID* pCandidateTriangles, const size_t candidateTrianglesCount, vector3_t* pOutClosestLocalPosition, float* pOutClosestDistanceSq) const override;
	virtual bool       GetTileData(const TileID tileId, Tile::STileData& outTileData) const override;
	virtual const AreaAnnotation* GetTriangleAnnotation(TriangleID triangleID) const override;
	virtual bool CanTrianglePassFilter(const TriangleID triangleID, const INavMeshQueryFilter& filter) const override;
	// ~MNM::INavMesh

	template<typename TQuery>
	void QueryTrianglesWithProcessing(const aabb_t& queryAabbWorld, const INavMeshQueryFilter* pFilter, TQuery&& query) const;

	void RemoveTrianglesByFlags(const MNM::AreaAnnotation::value_type flags);

private:
	template<typename TFilter, typename TQuery>
	void QueryTrianglesWithProcessingInternal(const aabb_t& queryAabbWorld, const TFilter& filter, TQuery&& query) const;

	template<typename TFilter, typename TQuery>
	INavMeshQueryProcessing::EResult QueryTileTrianglesWithProcessing(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, TQuery&& query) const;

	template<typename TFilter>
	CNavMesh::EWayQueryResult FindWayInternal(SWayQueryRequest& inputRequest, SWayQueryWorkingSet& workingSet, const TFilter& filter, SWayQueryResult& result) const;

	void    PredictNextTriangleEntryPosition(const TriangleID bestNodeTriangleID, const vector3_t& bestNodeLocalPosition, const TriangleID nextTriangleID, const unsigned int vertexIndex, const vector3_t& finalLocalPosition, vector3_t& outLocalPosition) const;

	//! Function provides next triangle edge through with the ray is leaving the triangle and returns whether the ray ends in the triangle or not.
	//! intersectingEdgeIndex is set to InvalidEdgeIndex if the ray ends in the triangle or there was no intersection found.
	bool FindNextIntersectingTriangleEdge(const vector3_t& rayStartPos, const vector3_t& rayEndPos, const vector2_t pVertices[3], real_t& rayIntersectionParam, uint16& intersectingEdgeIndex) const;

	//! Returns id of the neighbour triangle corresponding to the edge index of the current triangle or InvalidTriangleID if the edge is on navmesh boundaries
	template<typename TFilter>
	TriangleID StepOverEdgeToNeighbourTriangle(const vector3_t& rayStart, const vector3_t& rayEnd, const TileID currentTileID, const TriangleID currentTriangleID, const uint16 edgeIndex, const TFilter& filter) const;

	//! Filter for QueryTriangles, which accepts all triangles.
	//! Note, that the signature of PassFilter() function is same as INavMeshQueryFilter::PassFilter(), but it's not virtual.
	//! This way, templated implementation functions can avoid unnecessary checks and virtual calls.
	struct SAcceptAllQueryTrianglesFilter
	{
		inline bool PassFilter(const Tile::STriangle&) const { return true; }
		inline float GetCostMultiplier(const MNM::Tile::STriangle& triangle) const { return 1.0f; }
		inline MNM::real_t GetCost(const MNM::vector3_t& fromPos, const MNM::vector3_t& toPos,
			const MNM::Tile::STriangle& currentTriangle, const MNM::Tile::STriangle& nextTriangle,
			const MNM::TriangleID currentTriangleId, const MNM::TriangleID nextTriangleId) const
		{
			return (fromPos - toPos).lenNoOverflow();
		}
	};

	struct SMinIslandAreaQueryTrianglesFilter;

	template<typename TFilter>
	size_t QueryTrianglesWithFilterInternal(const aabb_t& queryLocalAabb, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;

	template<typename TFilter>
	size_t     QueryTileTriangles(const TileID tileID, const vector3_t& tileOrigin, const aabb_t& queryAabbWorld, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;
	template<typename TFilter>
	size_t     QueryTileTrianglesLinear(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;
	template<typename TFilter>
	size_t     QueryTileTrianglesBV(const TileID tileID, const STile& tile, const aabb_t& queryAabbTile, TFilter& filter, const size_t maxTrianglesCount, TriangleID* pOutTriangles) const;

	TriangleID FindClosestTriangleInternal(const vector3_t& localPosition, const TriangleID* pCandidateTriangles, const size_t candidateTrianglesCount, vector3_t* pOutClosestLocalPosition, real_t::unsigned_overflow_type* pOutClosestDistanceSq) const;

protected:
	void ComputeAdjacency(size_t x, size_t y, size_t z, const real_t& toleranceSq, STile& tile, const CTileConnectivityData* pConnectivityData);
	void ReComputeAdjacency(size_t x, size_t y, size_t z, const real_t& toleranceSq, STile& tile,
	                        size_t side, size_t tx, size_t ty, size_t tz, TileID targetID);

	bool           IsLocationInTriangle(const vector3_t& localPosition, const TriangleID triangleID) const;
	typedef VectorMap<TriangleID, TriangleID> RaycastCameFromMap;
	ERayCastResult ConstructRaycastResult(const ERayCastResult returnResult, const RayHit& rayHit, const TriangleID lastTriangleID, const RaycastCameFromMap& comeFromMap, RaycastRequestBase& raycastRequest) const;

	const vector3_t GetTileOrigin(const uint32 x, const uint32 y, const uint32 z) const
	{
		return vector3_t(
			real_t(x * m_params.tileSize.x),
			real_t(y * m_params.tileSize.y),
			real_t(z * m_params.tileSize.z));
	}

	struct TileContainer
	{
		TileContainer()
			: x(0)
			, y(0)
			, z(0)
		{
		}

		uint32 x: x_bits;
		uint32 y: y_bits;
		uint32 z: z_bits;

		STile  tile;
	};

	// - manages memory used for TileContainer instances in one big block
	// - kind of an optimized std::vector<TileContainer>
	// - free TileContainers are treated as a list of free indexes
	// - the free list and a counter of used elements specify a "high-water" mark such that the free list needs not grow unnecessarily
	class TileContainerArray
	{
	public:
		TileContainerArray();

		// Custom copy-construction that bypasses the m_tiles[] pointer and its data from the rhs argument.
		// Objects of this class are currently only copy-constructed in CNavMesh's copy-ctor.
		// When the assert() in our ctor's body fails, a copy-construction obviously occured in a different situation, and we will have to rethink about the use-cases.
		TileContainerArray(const TileContainerArray& rhs);

		~TileContainerArray();

		// called only once (by CNavMesh::Init())
		void Init(size_t initialTileContainerCount);

		// - allocates a new TileContainer (or re-uses an unused one)
		// - the returned TileID-1 can then be used as index for operator[]
		// - never returns a TileID of value 0
		const TileID AllocateTileContainer();

		// - marks the TileContainer associated with given TileID as unused
		// - calling this function more than once in a row with the same TileID will fail an assert()
		void         FreeTileContainer(const TileID tileID);

		const size_t GetTileCount() const;

		// - access a TileContainer by index
		// - in most cases, you will have a TileID at hand, and must then pass in TileID-1
		TileContainer&       operator[](size_t index);
		const TileContainer& operator[](size_t index) const;

		void                 Swap(TileContainerArray& other);
		void                 UpdateProfiler(ProfilerType& profiler) const;

#if DEBUG_MNM_DATA_CONSISTENCY_ENABLED
		void BreakOnInvalidTriangle(const TriangleID triangleID) const;
#else
		void BreakOnInvalidTriangle(const TriangleID triangleID) const {}
#endif

	private:
		// Assignment not supported (we'd have to think about potential use-cases first as it's not inherently clear what should happen with the m_tiles pointer)
		// (VC++ 2012 does not yet support defaulted and deleted functions, so we still need to declare it in the private scope and omit the implementation)
		TileContainerArray& operator=(const TileContainerArray&);

		void                Grow(size_t amount);

	private:
		TileContainer* m_tiles;
		size_t         m_tileCount;
		size_t         m_tileCapacity;

		typedef std::vector<size_t> FreeIndexes;    // dynamically-resizing array of indexes pointing to unused elements in m_tiles[]
		FreeIndexes m_freeIndexes;
	};

	inline Tile::STriangle& GetTriangleUnsafe(const TileID tileID, const uint16 triangleIdx) const
	{
		const TileContainer& container = m_tiles[tileID - 1];

		CRY_ASSERT(triangleIdx < container.tile.triangleCount);
		return container.tile.triangles[triangleIdx];
	}

	TileContainerArray m_tiles;
	size_t             m_triangleCount;

	// replace with something more efficient will speed up path-finding, ray-casting and
	// connectivity computation
	typedef std::map<uint32, uint32> TileMap;
	TileMap             m_tileMap;

	CIslands            m_islands;

	SGridParams                          m_params;
	SAgentSettings                       m_agentSettings;
	ProfilerType                         m_profiler;

	static const real_t                  kMinPullingThreshold;
	static const real_t                  kMaxPullingThreshold;
	static const real_t                  kAdjecencyCalculationToleranceSq;
};
} // namespace MNM

#include "NavMesh.inl"

#endif  // #ifndef __MNM_MESH_GRID_H
