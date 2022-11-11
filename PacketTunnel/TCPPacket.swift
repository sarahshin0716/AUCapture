//
//  TCPPacket.swift
//  PacketTunnel
//
//  Created by Claire Shin on 2022/11/10.
//

import Foundation


struct TCPPacket {
    var sourcePort: UInt16
    var destinationPort: UInt16
    var seqNum: UInt32
    var ackNum: UInt32
    // Offset: 4 + Reserved: 4
    var offset: UInt8
    var flags: UInt8
    var window: UInt16
    var checksum: UInt16
    var urgendPtr: UInt16
    var options: UInt32
    
    var source: String {sourceAddr.asString }
    private var sourceAddr: in_addr
    var destination: String { destAddr.asString }
    private var destAddr: in_addr
    
    var packetData: Data
    var payload: Data
    
    init(source: String, destination: String, sourcePort: UInt16, destinationPort: UInt16, payload: Data) {
        sourceAddr = in_addr(string: source)
        destAddr = in_addr(string: destination)
        
        self.sourcePort = sourcePort
        self.destinationPort = destinationPort
    }
}
