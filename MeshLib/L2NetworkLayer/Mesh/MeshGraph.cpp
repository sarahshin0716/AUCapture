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

#include <algorithm>
#include <functional>
#include <Log/Frontend/Log.h>
#include <Core.h>
#include "MeshGraph.h"

MeshGraph::MeshGraph(Anyfi::UUID myUid, const std::string &myDeviceName) {
    _myUid = myUid;
    _myDeviceName = myDeviceName;
    _edgeUpdateMatrix.clear();
    _nodePath.clear();
    _edgeMatrix.clear();
    _nodeMap.clear();

    auto updateTime = getCurrentTimeMilliseconds();
    auto myMeshNode = std::make_shared<MeshNode>(myUid, Anyfi::Core::getInstance()->myDeviceName(), 0, updateTime);
    _nodeMap[myUid] = myMeshNode;
}

uint64_t MeshGraph::getUpdateTime(Anyfi::UUID srcUid, Anyfi::UUID destUid) {
    Anyfi::Config::lock_type guard(_graphMutex);

    auto edge = std::make_shared<MeshEdge>(srcUid, destUid);
    return _getUpdateTimeWithoutLock(edge);
}

uint64_t MeshGraph::getUpdateTime(std::shared_ptr<MeshEdge> edge) {
    Anyfi::Config::lock_type guard(_graphMutex);
    return _getUpdateTimeWithoutLock(edge);
}
uint64_t MeshGraph::_getUpdateTimeWithoutLock(std::shared_ptr<MeshEdge> edge) {
    auto edgeIterator = _edgeUpdateMatrix.find(edge);
    if (edgeIterator != _edgeUpdateMatrix.end()) {
        return edgeIterator->second->updateTime();
    }
    return 0;
}

std::unique_ptr<int32_t> MeshGraph::_getUpdateTimeCost(std::shared_ptr<MeshEdge> edge) {
    auto edgeIterator = _edgeUpdateMatrix.find(edge);
    if (edgeIterator != _edgeUpdateMatrix.end()) {
        return make_unique<int32_t>(edgeIterator->second->cost());
    }else {
        return nullptr;
    }
}

void MeshGraph::_setUpdateTime(std::shared_ptr<MeshEdge> edge) {
    uint64_t updateTime = _getUpdateTimeWithoutLock(edge);
    if (updateTime < edge->updateTime()) {
        _edgeUpdateMatrix[edge] = edge;
    }
}

MeshEdgeMap MeshGraph::_sortByCostValue(const MeshEdgeMap &originMap) {
    std::vector<std::pair<Anyfi::UUID, std::shared_ptr<MeshEdge>>> edgeList(originMap.begin(), originMap.end());

    auto sort = [](const std::pair<Anyfi::UUID, std::shared_ptr<MeshEdge>> &left, const std::pair<Anyfi::UUID, std::shared_ptr<MeshEdge>> &right) {
        if (left.second == nullptr)
            return true;
        else if (right.second == nullptr)
            return false;
        int costDiff = left.second->cost() - right.second->cost();
        return (costDiff > 0);
    };

    std::sort(edgeList.begin(), edgeList.end(), sort);

    MeshEdgeMap newMap(edgeList.begin(), edgeList.end());

    return newMap;
}

void MeshGraph::_runDijkstra() {
    _nodePath.clear();

    if (_edgeMatrix.empty() || _edgeMatrix.find(_myUid) == _edgeMatrix.end())
        return;

    auto edgeList = _edgeMatrix[_myUid];

    _nodePath.insert(std::make_pair(_myUid, nullptr));

    /* Sort directly connected nodes */
    auto edgeQueue = _sortByCostValue(edgeList);

    while (!edgeQueue.empty()) {
        auto closestNode = *edgeQueue.begin();
        /* Move from queue to determined path */
        edgeQueue.erase(edgeQueue.begin());
        _nodePath[closestNode.first] = closestNode.second;
        bool nodeAdded = false;
        auto closestEdgeNeighborIterator = _edgeMatrix.find(closestNode.first);
        if (closestEdgeNeighborIterator != _edgeMatrix.end()) {
            for (const auto & edge : closestEdgeNeighborIterator->second) {
                auto prevPathIterator = _nodePath.find(edge.first);

                if (prevPathIterator== _nodePath.end() || (prevPathIterator->second != nullptr && prevPathIterator->second->cost() > closestNode.second->cost() + edge.second->cost())) {
                    auto newEdge = std::make_shared<MeshEdge>();
                    newEdge->firstUid(closestNode.second->firstUid());
                    newEdge->secondUid(closestNode.second->secondUid());
                    newEdge->updateTime(edge.second->updateTime());
                    newEdge->cost(closestNode.second->cost() + edge.second->cost());

                    auto pathEdgeIterator = edgeQueue.find(edge.first);
                    if (pathEdgeIterator == edgeQueue.end() || pathEdgeIterator->second->cost() > newEdge->cost()) {
                        edgeQueue[edge.first] = newEdge;
                        nodeAdded = true;
                    }
                }else {
                    // ignore
                }
            }
        }
        if (nodeAdded) {
            edgeQueue = _sortByCostValue(edgeQueue);
        }
    }
}

void MeshGraph::_refreshGraph() {
    UuidSet coveredNodes;
    UuidSet nodeToCover;
    coveredNodes.insert(_myUid);
    auto originalMatrix = _edgeMatrix;
    _edgeMatrix.clear();
    if (originalMatrix.find(_myUid) != originalMatrix.end()) {
        _edgeMatrix[_myUid] = originalMatrix[_myUid];
        for (const auto &edgeList : _edgeMatrix[_myUid]) {
            nodeToCover.insert(edgeList.first);
        }

        while (!nodeToCover.empty()) {
            auto iterator = nodeToCover.begin();
            auto currNode = *iterator;
            nodeToCover.erase(iterator);
            coveredNodes.insert(currNode);

            if (originalMatrix.find(currNode) != originalMatrix.end()) {
                _edgeMatrix[currNode] = originalMatrix[currNode];
            }
            for (const auto &edgeList : _edgeMatrix[currNode]) {
                if (coveredNodes.find(edgeList.first) == coveredNodes.end()) {
                    nodeToCover.insert(edgeList.first);
                }
            }
        }
    }else {
//        std::cout << "originalMatrix.find(_myUid) == originalMatrix.end())" << std::endl;
    }
}

std::shared_ptr<GraphChangeInfo> MeshGraph::_updateMeshEdge(const std::shared_ptr<MeshEdge> &newEdge) {

    std::shared_ptr<GraphChangeInfo> changeInfo = nullptr;

    auto currentUpdateTime = _getUpdateTimeWithoutLock(newEdge);

    if (currentUpdateTime <= newEdge->updateTime()) {
        _setUpdateTime(newEdge);

        /**
         * Cost가 0보다 크면 Edge update, 0이면 erase
         */
        /**
         * update source to destination edge
         */
        auto edgeListIt = _edgeMatrix.find(newEdge->firstUid());
        if (newEdge->cost() > 0) {
            if (edgeListIt == _edgeMatrix.end()) {
                MeshEdgeMap edgeList;
                edgeListIt = _edgeMatrix.insert({newEdge->firstUid(), edgeList}).first;
            }
            auto prevEdge = edgeListIt->second[newEdge->secondUid()];
            if (prevEdge == nullptr) {
                changeInfo = std::make_shared<GraphChangeInfo>(true, newEdge);
            }
            edgeListIt->second[newEdge->secondUid()] = std::make_shared<MeshEdge>(newEdge->firstUid(), newEdge->secondUid(), newEdge->updateTime(), newEdge->cost());
        } else if (edgeListIt != _edgeMatrix.end()){
            auto edgeIt = edgeListIt->second.find(newEdge->secondUid());
            if (edgeIt != edgeListIt->second.end()) {
                edgeListIt->second.erase(edgeIt);
                changeInfo = std::make_shared<GraphChangeInfo>(false, newEdge);
            }
            if (edgeListIt->second.empty()) {
                _edgeMatrix.erase(edgeListIt);
            }
        }

        /**
         * update destination to source edge
         */
        edgeListIt = _edgeMatrix.find(newEdge->secondUid());
        if (newEdge->cost() > 0) {
            if (edgeListIt == _edgeMatrix.end()) {
                MeshEdgeMap edgeList;
                edgeListIt = _edgeMatrix.insert({newEdge->secondUid(), edgeList}).first;
            }
            auto prevEdge = edgeListIt->second[newEdge->firstUid()];
            if (prevEdge == nullptr) {
                changeInfo = std::make_shared<GraphChangeInfo>(true, newEdge);
            }
            edgeListIt->second[newEdge->firstUid()] = std::make_shared<MeshEdge>(newEdge->secondUid(), newEdge->firstUid(), newEdge->updateTime(), newEdge->cost());
        } else if (edgeListIt != _edgeMatrix.end()) {
            auto edgeIt = edgeListIt->second.find(newEdge->firstUid());
            if (edgeIt != edgeListIt->second.end()) {
                edgeListIt->second.erase(edgeIt);
                changeInfo = std::make_shared<GraphChangeInfo>(false, newEdge);
            }
            if (edgeListIt->second.empty()) {
                _edgeMatrix.erase(edgeListIt);
            }
        }
    }
    return changeInfo;
}

std::vector<std::shared_ptr<GraphChangeInfo>> MeshGraph::updateGraph(std::shared_ptr<GraphInfo> graphInfo) {
    Anyfi::Config::lock_type guard(_graphMutex);

    std::vector<std::shared_ptr<GraphChangeInfo>> graphChangeInfoList;
    if (graphInfo != nullptr) {
        // update graph edge
        for (const auto &meshEdge : graphInfo->edgeList()) {
            auto changeInfo = _updateMeshEdge(meshEdge);
            if (changeInfo != nullptr) {
                graphChangeInfoList.push_back(changeInfo);
            }
        }
        _refreshGraph();
    }
    _runDijkstra();

    // merge node list
    for (const auto &meshNode : graphInfo->nodeList()) {
        auto it = _nodeMap.find(meshNode->uid());
        if (it == _nodeMap.end() || it->second->updateTime() < meshNode->updateTime()) {
            _nodeMap[meshNode->uid()] = meshNode;
        }
    }

    // get useless node list
    UuidSet removedNodeList;
    for (const auto &meshNode : _nodeMap) {
        if (meshNode.second->uid() == _myUid)
            continue;

        auto nodePathIterator = _nodePath.find(meshNode.second->uid());
        if (nodePathIterator == _nodePath.end()) {
            removedNodeList.insert(meshNode.first);
        }
    }

    // erase node
    for (const auto &uid : removedNodeList) {
        _nodeMap.erase(uid);
    }

    _updateClosestMap();

    _printGraph();
    return graphChangeInfoList;
}

int MeshGraph::updateNodeList(std::vector<std::shared_ptr<MeshNode>> nodeList) {
    Anyfi::Config::lock_type guard(_graphMutex);
    int updateCount = 0;
    for (const auto &meshNode : nodeList) {
        auto it = _nodeMap.find(meshNode->uid());
        if (it != _nodeMap.end() && it->second->updateTime() < meshNode->updateTime()) {
            _nodeMap[meshNode->uid()] = meshNode;
            updateCount++;
        }
    }
    _updateClosestMap();

    _printGraph();
    return updateCount;
}

std::shared_ptr<MeshEdge> MeshGraph::getFastestEdge(Anyfi::UUID destUid) {
    Anyfi::Config::lock_type guard(_graphMutex);
    return _getFastestEdgeWithoutLock(destUid);
}

std::shared_ptr<MeshEdge> MeshGraph::_getFastestEdgeWithoutLock(Anyfi::UUID destUid) {
    auto fastEdgeIterator = _nodePath.find(destUid);
    if (fastEdgeIterator != _nodePath.end())
        return fastEdgeIterator->second;
    else
        return nullptr;
}

std::vector<std::shared_ptr<MeshEdge>> MeshGraph::_getGraphEdgeList() {
    std::vector<std::shared_ptr<MeshEdge>> graphEdgeList;
    for (const auto &nodeEdgeListIt : _edgeMatrix) {
        for (const auto &edgeIt : nodeEdgeListIt.second) {
            graphEdgeList.push_back(edgeIt.second);
        }
    }
    return graphEdgeList;
}

std::shared_ptr<GraphInfo> MeshGraph::getGraphInfo() {
    Anyfi::Config::lock_type guard(_graphMutex);

    auto graphInfo = std::make_shared<GraphInfo>();
    std::vector<std::shared_ptr<MeshEdge>> graphEdgeList = _getGraphEdgeList();
    std::vector<std::shared_ptr<MeshNode>> nodeList;

    for (const auto &meshNode : _nodeMap) {
        nodeList.push_back(meshNode.second);
    }

    auto sortMeshEdge = [](const std::shared_ptr<MeshEdge> &l, const std::shared_ptr<MeshEdge> &r) {
        auto lsum = l->firstUid() + l->secondUid();
        auto rsum = r->firstUid() + r->secondUid();
        if (lsum != rsum) {
            return lsum > rsum;
        }else {
            auto lmax = (l->firstUid() > l->secondUid())? l->firstUid() : l->secondUid();
            auto rmax = (r->firstUid() > r->secondUid())? r->firstUid() : r->secondUid();
            return lmax > rmax;
        }
    };

    auto compareMeshEdge = [](const std::shared_ptr<MeshEdge> &l, const std::shared_ptr<MeshEdge> &r) {
        return (l->firstUid() == r->firstUid() && l->secondUid() == r->secondUid())
            || (l->firstUid() == r->secondUid() && l->secondUid() == r->firstUid());
    };

    auto sortMeshNode = [](const std::shared_ptr<MeshNode> &l, const std::shared_ptr<MeshNode> &r) {
        return (l->uid() > r->uid());
    };

    auto compareMeshNode = [](const std::shared_ptr<MeshNode> &l, const std::shared_ptr<MeshNode> &r) {
        return (l->uid() == r->uid());
    };

    // getGraphInfo는 패킷 전송 등에 활용되어 왕복이 아닌 한 쪽으로 가는 Edge만 있어도 되므로 패킷 크기 최소화를 위해,
    // EdgeList 정렬 후 중복 제거
    std::sort(graphEdgeList.begin(), graphEdgeList.end(), sortMeshEdge);
    auto graphEdgeListEndIt = std::unique(graphEdgeList.begin(), graphEdgeList.end(), compareMeshEdge);
    graphEdgeList.erase(graphEdgeListEndIt, graphEdgeList.end());

    std::sort(nodeList.begin(), nodeList.end(), sortMeshNode);
    auto nodeListEndIt = std::unique(nodeList.begin(), nodeList.end(), compareMeshNode);
    nodeList.erase(nodeListEndIt, nodeList.end());

    graphInfo->edgeList(graphEdgeList);
    graphInfo->nodeList(nodeList);

    return graphInfo;
}

std::shared_ptr<GraphInfo> MeshGraph::getDisconnGraph(std::shared_ptr<GraphInfo> otherGraph) {
    Anyfi::Config::lock_type guard(_graphMutex);

    auto disconnGraph = std::make_shared<GraphInfo>();
    auto newEdgeList = otherGraph->edgeList();
    auto newNodeList = otherGraph->nodeList();
    for (const auto &edge : newEdgeList) {
        if (edge->cost() >= 0) {
            // Check if my information is more recent
            uint64_t myUpdateTime = _getUpdateTimeWithoutLock(edge);
            if (myUpdateTime != 0 && myUpdateTime > edge->updateTime()) {
                // Edge disconnected in my graph
                auto myCost = _getUpdateTimeCost(edge);
                if (myCost != nullptr && *myCost < 0) {
                    edge->cost(-1);
                    edge->updateTime(myUpdateTime);
                    disconnGraph->addEdge(edge);
                }
            }
        }
    }
    return disconnGraph;
}

UuidSet MeshGraph::getConnectedNodes() {
    Anyfi::Config::lock_type guard(_graphMutex);

    UuidSet connectedNodes;
    for (const auto &node : _nodeMap) {
        connectedNodes.insert(node.first);
    }
    return connectedNodes;
}

UuidSet MeshGraph::getRemovedNodes(const UuidSet prevNodes) {
    Anyfi::Config::lock_type guard(_graphMutex);

    UuidSet removedNodeList;
    // Get remvoed node list;
    UuidSet newNodes;
    for (const auto &node : _nodeMap) {
        newNodes.insert(node.first);
    }
    for (auto prevNode : prevNodes) {
        if (newNodes.find(prevNode) == newNodes.end()) {
            removedNodeList.insert(prevNode);
        }
    }
    // TODO: print removed nodes
    // "---- Removed Nodes ----"
    // for(auto node : removedNodeList) {
    //     "uid: " + node
    // }
    return removedNodeList;
}

std::shared_ptr<MeshEdge> MeshGraph::getNeighborEdge(Anyfi::UUID destUid) {
    Anyfi::Config::lock_type guard(_graphMutex);

    std::shared_ptr<MeshEdge> edge = nullptr;
    auto nodeMapIterator = _edgeMatrix.find(_myUid);
    if (nodeMapIterator != _edgeMatrix.end()) {
        for (const auto &entry : nodeMapIterator->second) {
            if (entry.first == destUid) {
                edge = std::make_shared<MeshEdge>(_myUid, entry.first, entry.second->cost(), entry.second->updateTime());
                break;
            }
        }
    }

    return edge;
}

void MeshGraph::_updateClosestMap() {
    Anyfi::Config::lock_type guard(_closestMapMutex);

    auto nodeMap = _nodeMap;
    _closestMap.clear();
    for (uint8_t i = 0; i < UINT8_MAX; i++) {
        std::shared_ptr<MeshNode> closestNode = nullptr;
        int32_t distance = INT32_MAX;
        for (const auto &node : nodeMap) {
            if (node.second->type() != i)
                continue;

            if (node.first == _myUid) {
                // 자기 자신일 경우.
                closestNode = node.second;
                distance = 0;
            }else {
                // 자기 자신이 아닐 경우.
                auto fastestEdge = _getFastestEdgeWithoutLock(node.second->uid());
                if (fastestEdge == nullptr)
                    continue;
                auto cost = fastestEdge->cost();
                if (closestNode == nullptr || (cost < distance)) {
                    closestNode = node.second;
                    distance = cost;
                }
            }
        }
        if (closestNode) {
            // TODO: WifiP2P 말고 BluetoothP2P 등도  사용할 경우 해당 구분 필요.
            Anyfi::Address addr;
            addr.connectionType(Anyfi::ConnectionType::WifiP2P);
            addr.type(Anyfi::AddrType::MESH_UID);
            addr.setMeshUID(closestNode->uid());
            _closestMap[i] = { addr, (uint8_t)distance };
        }
    }
}

std::shared_ptr<MeshNode> MeshGraph::setMeshNodeType(uint8_t type) {
    Anyfi::Config::lock_type guard(_graphMutex);

    std::shared_ptr<MeshNode> meshNode = nullptr;
    auto it = _nodeMap.find(_myUid);
    if (it != _nodeMap.end()) {
        meshNode = it->second;
        auto updateTime = getCurrentTimeMilliseconds();
        meshNode->type(type);
        meshNode->updateTime(updateTime);
    }else {
        auto updateTime = getCurrentTimeMilliseconds();
        meshNode = std::make_shared<MeshNode>(_myUid, Anyfi::Core::getInstance()->myDeviceName(), 0, updateTime);
        meshNode->type(type);
        _nodeMap[_myUid] = meshNode;
    }
    _updateClosestMap();
    return meshNode;
}

std::pair<Anyfi::Address, uint8_t> MeshGraph::getClosestMeshNode(uint8_t type) {
    Anyfi::Config::lock_type guard(_closestMapMutex);

    auto it = _closestMap.find(type);
    if (it != _closestMap.end()) {
        return it->second;
    }else {
        Anyfi::Address addr;
        return { addr, 0 };
    }
}

void MeshGraph::_printGraph() {
    Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "--------------------------------Closest Map----------------------------------");
    {
        for(auto pair: _closestMap){
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "{ type: "+to_string((int)pair.first)+" addr: "+pair.second.first.toString()+" distanceInMeters: "+to_string((int)pair.second.second)+" }");
        }
    }
    {
        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__,
                          "-------------------------------Node Path List---------------------------------");
        for(auto pair: _nodePath){
            auto edge = std::dynamic_pointer_cast<MeshEdge>(pair.second);
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "{ UUID: "+pair.first.toString()+" edge: "+ (edge==
                                                                          nullptr? "null" :edge->toString())+" }");
        }

        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__,
                          "---------------------------------Node Map-----------------------------------");
        for(auto pair: _nodeMap){
            auto node = std::dynamic_pointer_cast<MeshNode>(pair.second);
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "{ UUID: "+pair.first.toString()+" node: "+node->toString()+" }");
        }

        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__,
                          "------------------------------Edge Update Map--------------------------------");
        for(auto pair: _edgeUpdateMatrix){
            auto edge = std::dynamic_pointer_cast<MeshEdge>(pair.first);
            auto edge2 = std::dynamic_pointer_cast<MeshEdge>(pair.second);
            Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "{ edge1: "+edge->toString()+" edge2: "+edge2->toString()+" }");
        }

        Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__,
                          "--------------------------------Edge Matrix----------------------------------");
        for(auto pair: _edgeMatrix){
            for(auto pair2: pair.second){
                auto edge = std::dynamic_pointer_cast<MeshEdge>(pair2.second);
                Anyfi::Log::debug(Anyfi::Log::DebugFilter::L2_MESH, __func__, "{ UUID: "+pair.first.toString()+" UUID2: "+pair2.first.toString()+" edge: " + edge->toString() + " }");
            }
        }
    }

    // set object
    std::string jsonMeshGraph;

    std::string jsonMeshNodes;
    for(auto pair: _nodeMap){
        auto node = std::dynamic_pointer_cast<MeshNode>(pair.second);

        if(!jsonMeshNodes.empty()) {
            jsonMeshNodes += ", ";
        }
        jsonMeshNodes += std::string("{ \"uuid\": \"") + node->uid().toString() + "\", \"deviceName\": \"" + node->deviceName() + "\" }";
    }
    jsonMeshNodes = "[" + jsonMeshNodes + "]";

    std::string jsonMeshEdges;
    for(auto pair: _edgeMatrix){
        for(auto pair2: pair.second){
            auto edge = std::dynamic_pointer_cast<MeshEdge>(pair2.second);

            if(!jsonMeshEdges.empty()) {
                jsonMeshEdges += ", ";
            }
            jsonMeshEdges += std::string("{ \"source\": \"") + edge->firstUid().toString() + "\", \"target\": \"" + edge->secondUid().toString() + "\" }";
        }
    }
    jsonMeshEdges = "[" + jsonMeshEdges + "]";

    jsonMeshGraph = "{ \"nodes\": " + jsonMeshNodes + ", \"edges\": " + jsonMeshEdges + " }";

    Anyfi::Core::getInstance()->setWrapperObject("meshGraph", jsonMeshGraph);
}