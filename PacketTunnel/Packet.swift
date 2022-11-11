//
//  PacketProtocol.swift
//  PacketTunnel
//
//  Created by Claire Shin on 2022/11/11.
//

import Foundation

struct Packet {

    private mutating func setPayloadWithUInt8(_ value: UInt8, at: Int) {
        var v = value
        withUnsafeBytes(of: &v) {
            packetData.replaceSubrange(at..<at + 1, with: $0)
        }
    }

    private mutating func setPayloadWithUInt16(_ value: UInt16, at: Int, swap: Bool = true) {
        var v: UInt16
        if swap {
            v = CFSwapInt16HostToBig(value)
        } else {
            v = value
        }
        withUnsafeBytes(of: &v) {
            packetData.replaceSubrange(at..<at + 2, with: $0)
        }
    }

    private mutating func setPayloadWithUInt32(_ value: UInt32, at: Int, swap: Bool = true) {
        var v: UInt32
        if swap {
            v = CFSwapInt32HostToBig(value)
        } else {
            v = value
        }
        withUnsafeBytes(of: &v) {
            packetData.replaceSubrange(at..<at+4, with: $0)
        }
    }

    private mutating func setPayloadWithData(_ data: Data, at: Int, length: Int? = nil, from: Int = 0) {
        var length = length
        if length == nil {
            length = data.count - from
        }
        packetData.replaceSubrange(at..<at+length!, with: data)
    }

    private mutating func resetPayloadAt(_ at: Int, length: Int) {
        packetData.resetBytes(in: at..<at+length)
    }

    private func computePseudoHeaderChecksum() -> UInt32 {
        var result: UInt32 = 0
        result += sourceAddr.s_addr >> 16 + sourceAddr.s_addr & 0xffff
        result += destAddr.s_addr >> 16 + destAddr.s_addr & 0xffff
        result += UInt32(proto.rawValue) << 8
        switch proto {
        case .udp:
            result += CFSwapInt32(UInt32(payload.count + 8))
        default:
            break
        }
        return result
    }

    private mutating func buildSegment(_ pseudoHeaderChecksum: UInt32) {
        let offset = Int(headerLength)

        var sourcePort = sourcePort.bigEndian
        withUnsafeBytes(of: &sourcePort) {
            packetData.replaceSubrange(offset..<(offset + 2), with: $0)
        }
        var destinationPort = destinationPort.bigEndian
        withUnsafeBytes(of: &destinationPort) {
            packetData.replaceSubrange(offset + 2..<(offset + 4), with: $0)
        }

        var length = NSSwapHostShortToBig(UInt16(payload.count + 8))
        withUnsafeBytes(of: &length) {
            packetData.replaceSubrange(offset + 4..<offset + 6, with: $0)
        }

        packetData.replaceSubrange(offset + 8..<offset + 8 + payload.count, with: payload)

        packetData.resetBytes(in: offset + 6..<offset + 8)
        var checksum = Checksum.computeChecksum(packetData, from: offset, to: nil, withPseudoHeaderChecksum: pseudoHeaderChecksum)
        withUnsafeBytes(of: &checksum) {
            packetData.replaceSubrange(offset + 6..<offset + 8, with: $0)
        }
    }
}
