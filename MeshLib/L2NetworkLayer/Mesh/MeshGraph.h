/*
 *  Copyright (c) 2016. Anyfi Inc.
 *
 *  All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Anyfi Inc. and its suppliers, if any.  The intellectual and technical concepts
 *  contained herein are proprietary to Anyfi Inc. and its suppliers and may be
 *  covered by U.S. and Foreign Patents, patents in process, and are protected
 *  by trade secret or copyright law.
 *
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from Anyfi Inc.
 */

#ifndef ANYFIMESH_CORE_MESHGRAPH_H
#define ANYFIMESH_CORE_MESHGRAPH_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "Graph/GraphInfo.h"
#include "Graph/MeshEdge.h"
#include "Graph/GraphChangeInfo.h"
#include "Neighbor.h"

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest_prod.h>
#endif

typedef std::unordered_map<Anyfi::UUID, std::shared_ptr<MeshEdge>, Anyfi::UUID::Hasher> MeshEdgeMap;
typedef std::unordered_set<Anyfi::UUID, Anyfi::UUID::Hasher> UuidSet;

/**
 * MeshNetwork에서 Node 사이의 연결고리 정보에 대한 그래프 클래스입니다.
 * 각 Edge의 업데이트 정보와 최단거리 계산 등에 활용됩니다.
 */
class MeshGraph {
public:
    explicit MeshGraph(Anyfi::UUID myUid, const std::string &myDeviceName);

    /**
     * Get current update time of edge between given uids
     * Return 0 if it does not exist
     * @param srcUid
     * @param destUid
     * @return update time of edge
     */
    uint64_t getUpdateTime(Anyfi::UUID srcUid, Anyfi::UUID destUid);

    /**
     * Get current update time of given edge
     * Return 0 if it does not exist
     * @param edge
     * @return update time of given edge
     */
    uint64_t getUpdateTime(std::shared_ptr<MeshEdge> edge);

    std::shared_ptr<GraphInfo> getGraphInfo();
    std::shared_ptr<GraphInfo> getDisconnGraph(std::shared_ptr<GraphInfo> otherGraph);
    /**
     * 연결된 모든 노드를 전달합니다.
     * @return 노드 리스트
     */
    UuidSet getConnectedNodes();
    /**
     * Get diffrence between given node set and current connected node set
     * @param prevNodes
     * @return all nodes in prevNodes but not in current connected node set
     */
    UuidSet getRemovedNodes(const UuidSet prevNodes);
    /**
     * Add given graph_info to my graph
     * @param graphInfo new graph to be updated
     * @return 수정된 그래프 정보
     */
    std::vector<std::shared_ptr<GraphChangeInfo>> updateGraph(std::shared_ptr<GraphInfo> graphInfo);
    /**
     * _nodeMap을 업데이트합니다. MeshGraph가 가지고 있는 노드의 UpdateTime과 파라미터로 들어온 MeshNode의 UpdateTime을 비교하여
     * 파라미터로 들어온 MeshNode의 UpdateTime이 클 경우 반영합니다.
     * @param nodeList 업데이트할 노드 리스트
     * @return updateCount MeshGraph에 업데이트가 반영된 노드의 갯수.
     */
    int updateNodeList(std::vector<std::shared_ptr<MeshNode>> nodeList);

    /**
     * uid에 해당하는 노드까지 가장 빠르게 갈 수 있는 Neighbor를 반환합니다.
     * @param destUid destination uid
     * @return std::shared_ptr<MeshEdge> 최단거리 MeshEdge
     */
    std::shared_ptr<MeshEdge> getFastestEdge(Anyfi::UUID destUid);
    /**
     * destUid에 해당하는 내 주변 노드로 가는 MeshEdge를 반환합니다.
     * @param destUid
     * @return std::shared_ptr<MeshEdge> destUid로 가는 MeshEdge
     */
    std::shared_ptr<MeshEdge> getNeighborEdge(Anyfi::UUID destUid);

    /**
     * 자기 자신 MeshNode의 type을 업데이트 합니다.
     * @param type
     * @return type이 업데이트 된 MeshNode
     */
    std::shared_ptr<MeshNode> setMeshNodeType(uint8_t type);

    /**
     * Type에 해당하는 MeshNode 중 가장 가까운 MeshNode를 찾아 반환합니다.
     * @param type
     * @return std::pair<Anyfi::Address, Cost>
     */
    std::pair<Anyfi::Address, uint8_t> getClosestMeshNode(uint8_t type);

private:
    /**
     * Lock이 없는 MeshGraph::getUpdateTime
     * @param edge
     * @return
     */
    uint64_t _getUpdateTimeWithoutLock(std::shared_ptr<MeshEdge> edge);

    /**
     * Lock이 없는 MeshGraph::getFastestEdge
     * @param destUid
     * @return
     */
    std::shared_ptr<MeshEdge> _getFastestEdgeWithoutLock(Anyfi::UUID destUid);

    /**
     * Set current update time of given edge
     * @param edge
     */
    void _setUpdateTime(std::shared_ptr<MeshEdge> edge);

    std::unique_ptr<int32_t> _getUpdateTimeCost(std::shared_ptr<MeshEdge> edge);

    /**
     * Run Dijkstra algorithm on current node map
     * and save the result which is the fastest route to each node in __node_path
     */
    void _runDijkstra();

    /**
     * Remove unreachable nodes from graph
     */
    void _refreshGraph();

    /**
     * Sort given HashMap by its value
     * Here, we are sorting the map by edge cost
     */
    MeshEdgeMap _sortByCostValue(const MeshEdgeMap &map);

    /**
     * Update Single Mesh Edge
     * Create or delete two mesh edges (source -> destination / destination -> source)
     * Ignore if update time is lower
     * @param newEdge : edge to be updated
     * @return std::shared_ptr<GraphChangeInfo>
     */
    std::shared_ptr<GraphChangeInfo> _updateMeshEdge(const std::shared_ptr<MeshEdge> &newEdge);

    std::vector<std::shared_ptr<MeshEdge>> _getGraphEdgeList();

    void _updateClosestMap();

    void _printGraph();

    Anyfi::UUID _myUid;
    std::string _myDeviceName;

    /**
     * MeshEdge의 Update Time을 관리하기 위한 map입니다.
     */
    std::unordered_map<std::shared_ptr<MeshEdge>, std::shared_ptr<MeshEdge>, MeshEdge::Hasher, MeshEdge::Equaler> _edgeUpdateMatrix;

    /**
     * Dijkstra에 의하여 산출된 경로
     * map<목적지 UID, 방향 UID>
     */
    MeshEdgeMap _nodePath;

    /**
     * 각 노드들의 연결정보
     * map<UID, map<UID, MeshEdge>>
     */
    std::unordered_map<Anyfi::UUID, std::unordered_map<Anyfi::UUID, std::shared_ptr<MeshEdge>, Anyfi::UUID::Hasher>, Anyfi::UUID::Hasher> _edgeMatrix;

    /**
     * Node Map <uid, Node>
     */
    std::unordered_map<Anyfi::UUID, std::shared_ptr<MeshNode>, Anyfi::UUID::Hasher> _nodeMap;

    /**
     * 각 타입별 가장 가까운 노드를 기록합니다.
     * Map<Type value, Pair<Address, Distance>>
     */
    std::unordered_map<uint8_t, std::pair<Anyfi::Address, uint8_t>> _closestMap;
    Anyfi::Config::mutex_type _closestMapMutex;

    /**
     * _edgeUpdateMatrix, _nodePath, _edgeMatrix, _nodeMap의 Thread-safe를 위한 mutex입니다.
     */
    Anyfi::Config::mutex_type _graphMutex;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
    FRIEND_TEST(MeshGraph, Init);
    FRIEND_TEST(MeshGraph, MeshGraphUpdateEdge);
#endif

};
#endif //ANYFIMESH_CORE_MESHGRAPH_H
