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

#ifndef ANYFIMESH_CORE_GRAPHINFO_H
#define ANYFIMESH_CORE_GRAPHINFO_H

#include <vector>

#include "../../../Common/Network/Buffer/NetworkBuffer.h"
#include "../../../L2NetworkLayer/Mesh/MeshNode.h"
#include "MeshEdge.h"


/**
 * Device간에 특정한 MeshNetwork 그래프 정보를 주고받을 때 활용되는 클래스입니다.
 */
class GraphInfo {
public:
    GraphInfo() = default;
    explicit GraphInfo(std::vector<std::shared_ptr<MeshEdge>> edgeList, std::vector<std::shared_ptr<MeshNode>> nodeList) {
        _edgeList = edgeList;
        _nodeList = nodeList;
    }

    /**
     * Getters
     */
    void edgeList(std::vector<std::shared_ptr<MeshEdge>> edgeList) { _edgeList = edgeList; }
    void nodeList(std::vector<std::shared_ptr<MeshNode>> nodeList) { _nodeList = nodeList; }

    /**
     * Setters
     */
    std::vector<std::shared_ptr<MeshEdge>> edgeList() const { return _edgeList; }
    std::vector<std::shared_ptr<MeshNode>> nodeList() const { return _nodeList; }

    void addEdge(std::shared_ptr<MeshEdge> edge);
    void addEdge(std::vector<std::shared_ptr<MeshEdge>> edgeList);
    void addNode(std::shared_ptr<MeshNode> node);
    void addNode(std::vector<std::shared_ptr<MeshNode>> nodeList);

    /**
     * GraphInfo를 NetworkBuffer 형태로 패킹
     * 패킷 형태
     * |---------------------------------------------------|
     * | 4 byte       | N Byte   | 4 byte       | N Byte   |
     * |---------------------------------------------------|
     * | EdgeListSize | EdgeList | NodeListSize | NodeList |
     * @return NetworkBufferPointer 패킹된 NetworkBuffer
     */
    NetworkBufferPointer toPacket();
    void toPacket(NetworkBufferPointer buffer);
    /**
     * NetworkBuffer 형태로 패킹된 GraphInfo를 다시 unpacking
     * @param buffer 패킹된 GraphInfo가 포함된 NetworkBuffer
     * @return
     */
    static std::shared_ptr<GraphInfo> toSerialize(NetworkBufferPointer buffer);

    std::string toString();

private:
    std::vector<std::shared_ptr<MeshEdge>> _edgeList;
    std::vector<std::shared_ptr<MeshNode>> _nodeList;
};

#endif //ANYFIMESH_CORE_GRAPHINFO_H
