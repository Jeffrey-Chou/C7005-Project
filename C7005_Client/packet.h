#ifndef PACKET_H
#define PACKET_H

#define PAYLOADLEN 16
#define HEADERLEN 16
#define ACK 1
#define DATA 2
#define EOT 4
#define URG 8
#define FIN 16

struct DataPacket
{
    int packetType;
    int seqNum;
    char data[PAYLOADLEN];
    int windowSize;
    int ackNum;
};

struct ControlPacket
{
    int packetType;
    int seqNum;
    int windowSize;
    int ackNum;
};

#endif // PACKET_H
