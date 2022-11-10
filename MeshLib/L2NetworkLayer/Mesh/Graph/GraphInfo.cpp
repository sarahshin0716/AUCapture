/* Copyright (C) 2018. Anyfi Inc. - All Rights Reserved
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

#include "GraphInfo.h"
#include "Common/Network/Buffer/NetworkBufferPool.h"

void GraphInfo::addEdge(std::shared_ptr<MeshEdge> edge) {
    if (edge) {
        _edgeList.push_back(edge);
    }
};

void GraphInfo::addEdge(std::vector<std::shared_ptr<MeshEdge>> edgeList) {
    for (auto &edge : edgeList) {
        if (edge) {
            _edgeList.push_back(edge);
        }
    }
}

void GraphInfo::addNode(std::shared_ptr<MeshNode> node) {
    if (node) {
        _nodeList.push_back(node);
    }
}

void GraphInfo::addNode(std::vector<std::shared_ptr<MeshNode>> nodeList) {
    for (auto &node : nodeList) {
        if (node) {
            _nodeList.push_back(node);
        }
    }
}

NetworkBufferPointer GraphInfo::toPacket() {
    auto buffer = NetworkBufferPool::acquire();
    buffer->setBackwardMode();
    toPacket(buffer);
    return buffer;
}

void GraphInfo::toPacket(NetworkBufferPointer buffer) {
    if (buffer->isBackwardMode()) {
            for (auto i = (int)_nodeList.size() - 1; i >= 0; --i) {
                _nodeList.at(i)->toPacket(buffer);
            }
        buffer->put<uint32_t>((uint32_t) _nodeList.size());
        if (!_edgeList.empty()) {
            for (auto i = (int)_edgeList.size() - 1; i >= 0; --i) {
                _edgeList.at(i)->toPacket(buffer);
            }
        }
        buffer->put<uint32_t>((uint32_t) _edgeList.size());
    } else {
        buffer->put<uint32_t>((uint32_t) _edgeList.size());
        for (const auto &edge : _edgeList) {
            edge->toPacket(buffer);
        }
        buffer->put<uint32_t>((uint32_t) _nodeList.size());
        for (const auto &node : _nodeList) {
            node->toPacket(buffer);
        }
    }
}

std::shared_ptr<GraphInfo> GraphInfo::toSerialize(NetworkBufferPointer buffer) {
    auto graphInfo = std::make_shared<GraphInfo>();

    auto edgeListSize = buffer->get<uint32_t>();

    for (int i = 0; i < edgeListSize; ++i) {
        auto edge = MeshEdge::toSerialize(buffer);
        graphInfo->addEdge(edge);
    }

    auto nodeListSize = buffer->get<uint32_t>();
    for (int j = 0; j < nodeListSize; ++j) {
        auto node = MeshNode::toSerialize(buffer);
        graphInfo->addNode(node);
    }

    return graphInfo;
}

std::string GraphInfo::toString() {
    std::string strEdgeList = to_string(_edgeList.size()) + " edges: [";
    for (int i = 0; i < _edgeList.size(); ++i) {
        if (_edgeList.at(i)) {
            strEdgeList += ", " + _edgeList.at(i)->toString();
        } else {
            strEdgeList += "null";
        }
        if (i + 1 != _edgeList.size()) {
            strEdgeList += ", ";
        }

    }
    strEdgeList += "]";

    std::string strNodeList = to_string(_nodeList.size()) + " nodes: [";
    for (int j = 0; j < _nodeList.size(); ++j) {
        if (_nodeList.at(j)) {
            strNodeList += ", " + _nodeList.at(j)->toString();
        } else {
            strNodeList += "null";
        }
        if (j + 1 != _nodeList.size()) {
            strNodeList += ", ";
        }
    }
    strNodeList += "]";

    return "{ " + strEdgeList + ", " + strNodeList + " }";
}
