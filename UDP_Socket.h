/*
TODO: Forward Error Correcting.
Get rid of undef mac and change it to macintosh or something, undef mac is bad with winsock using mac.
CONCERNS:
S_un may not be required on linux and unix, so that may need a platform check.
*/

#ifndef UDP_Socket_H
#define UDP_Socket_H

#define WINDOWS 1
#define MAC 2
#define LINUX 3
#define UDP_PACKET_BUFFER_SIZE 1024

//FIX RPC issue.
#undef MAC
//Defines the number of duplicate packets to send.

#include <stdio.h> //Used for printing.
#include <stdint.h> //Used for integer types.
#include <string> //Concactinating char*'s
#include <chrono> //used for time.
#include <vector> 

//Find the platform.
#if defined(_WIN32)
#define PLATFORM WINDOWS
#elif defined(__APPLE__)
#define PLATFORM MAC
#else
#define PLATFORM LINUX
#endif

//Include based on platform.
#if PLATFORM == WINDOWS
#include <WinSock2.h>
#pragma comment(lib, "wsock32.lib")
#define WIN32_LEAN_AND_MEAN
#elif PLATFORM == MAC || PLATFORM == LINUX
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h> //For close.
#include <cstdlib> //For malloc on linux
#endif

const uint8_t MAX_PACKET_SIZE = 30;
const uint16_t PACKET_IDENTIFIER = 77;
const uint16_t ACK_IDENTIFIER = 78;
const uint8_t ACK_BODY_SIZE = 3; //A + 2 bytes from UINT16_t for the protocol acked.
const uint8_t NUM_PACKET_UINT32 = 2;
const uint8_t NUM_PACKET_UINT16 = 2;
const uint8_t NUM_PACKET_UINT8 = 0;
//TODO: REPLACE THIS TO MAX_REPEAT_SENDS WHEN CONGESTION IS IMPLEMENTED.
const uint8_t REPEAT_SENDS = 1;
//Max amount of packets in a sequence
const uint8_t SEQUENCE_MAX = 100;
const uint8_t DEBUG = 1;
const uint8_t MAX_ACK_REPEAT_SENDS = 10;
//Used to optimize sorting.
const uint8_t NUM_DIGITS_UINT16 = 6;
//The packet header size
const uint8_t PACKET_HEADER_SIZE = sizeof(uint32_t) * 2 + sizeof(uint16_t) * 2;

//Packet Definition
struct Packet
{
	//Size of the packet.
	uint32_t packetSize;
	//This is the sequence number, could also be a timestamp.
	uint32_t sequenceIdentifier;
	//This says that its our program and not some junk data.
    uint16_t protocolIdentifier;
	//This is how many parts there are in the packet.
	uint16_t partIdentifier;
	//This is the message.
	char message[MAX_PACKET_SIZE];
};

class UDP_Socket
{

public:
	//Default constructor.
	UDP_Socket();
	//Constructs and connects
	UDP_Socket(int a, int b, int c, int d, int recvPort, int sendPort);
	//Initializes the socket.
	//@return true if the initialization was successful, false otherwise.
	bool initialize(unsigned int port);
	//Initializes a socket and sets the destination of the message to be received.
	//@param a - the first 3 digits of an IP.
	//@param b - the second 3 digits of an IP.
	//@param c - the third 3 digits of an IP.
	//@param d - the fourth 3 digits of an IP.
	//@param port - the port to send the message to.
	//@return - True if the message was sent, false otherwise. (NOTE: SENT not RECEIVED!)
	bool initialize(int a, int b, int c, int d, int recvPort, int destPort);
	//Sends the message to the destination.
	//@param msg - The message to be sent.
	void sendMessage(const char* msg);
	//Sends a packet to the destination.
	//@param msg - The message to be sent.
	void sendPacket(char* msg);
	//Receives a message from the server.
	void receivePackets();
	//Sets the destination of the message to be received.
	//@param a - the first 3 digits of an IP.
	//@param b - the second 3 digits of an IP.
	//@param c - the third 3 digits of an IP.
	//@param d - the fourth 3 digits of an IP.
	//@param port - the port to send the message to.
	void setDestination(int a, int b, int c, int d, int port);
	//Shuts the socket down.
	void shutdown();
	//Gets the received message buffer.
	//@param i the index at which to access the message.
	//@return string - the final message
	std::string getReceivedMessageBuffer(const int i);
	//Default destructor.
	~UDP_Socket();
private:
	//The handle to the socket.
	int socketHandle;
	//Initializes the socket specifically.
	//@return true if the socket init was sucessful, false otherwise.
	bool initializeSocket();
	//Shuts the socket down.
	void shutdownSocket();
	//Creates the socket based on the platform.
	//@param unsigned int - port to bind to.
	//@return true if the socket creation was successful, false otherwise.
	bool createSocket(unsigned int port);
	//Binds the socket with the handle.
	//@param unsigned int - port to bind the socket to.
	//@return true if the binding was successful, false otherwise.
	bool bindSocket(unsigned int port);
	//Sets the socket to non blocking through a dup.
	//@return true if successful, otherwise false.
	bool makeSocketNonBlocking();
	//Adds the packetdata to the message to be sent.
	//@param PacketData* PacketData to serialize.
	//@param char* the buffer in which to add characters to.
	//@param int - the buffer index to continue at.
	//@return - the total size of the packet to be sent.
	int addPacketData(Packet* mPacketData, char* buffer, int bufferIndex);
	//Deserialize and copy into the bufer.
	void parseMessages(char* buffer, int index, int bytes);
	//Serializes the header function.
	//@param PacketHeader* pointer to the header to serialize.
	//@param char* buffer to add the serialized data to.
	//@param const unsigned int packetSize - size of the packet thats about to be sent.
	//@return int - index of where the buffer was left off.
	int serializeHeader(Packet* header);
	//Serialize the packet and send
	void serializeAndSend(char* message);
	//Sends an ack for the received protocol.
	//@param rawPacket - the packet in its big endian form.
	void sendACK();
	//Sort the message buffer for out of order packets.
	void sortMessageBuffer();
	//ByteTok
	//@param char* input - the string to find.
	//@return char* - the length of the string in bytes.
	//Header Buffer.
	char* byteTok(char* input);
	//Quicksort
	//@param lowerBound - The start of the packets to sort.
	//@param upperBound - The end of the packet to sort.
	void quicksort(int lowerBound, int upperBound);
	//PivotPartIdentifiers - Quicksort helper function.
	//@param lowerBound - The start of the packets to sort.
	//@param upperBound - The end of the packets to sort.
	//@return - returns the new bound.
	int pivot(int lowerBound, int upperBound);
	//Helper for setting up destination
	//@param index - index of the buffer to set up.
	void setupPacketBuffer(unsigned int index);
	//Checks that the packet is valid.
	//@return int - not zero if a valid packet.
	int checkProtocol();
	//The buffer for the header files.
	char* headerBuffer;
	//Destination addresses.
	sockaddr_in destination;
	//Where we're receiving the message from.
	sockaddr_in receive;
	//Length of the receive.
	//Keep these private since theres no need to keep redeclaring.
	char packetData[MAX_PACKET_SIZE];
	//Current packet sequence being sent.
	unsigned int currentPacketSequence;
	//This is the minimum transmission unit.
	int minimumTransmissionUnit;
	//The packet struct that is about to be serialized and sent.
	Packet* mSendingPacket;
	/*	The received, deserialized packet.
	TODO: Turn this into a circular array for packets.
	TODO: Make this a 2d array so that each sequence is a slot, however if
	theres a part have it go down to that slot.*/
	Packet* receivedPacketsBuffer[UDP_PACKET_BUFFER_SIZE];
	//Recycled buffer variable for htons
	uint8_t u8Htons;
	//Recycled buffer variable for htons
	uint16_t u16Htons;
	//Recycled buffer variable for htonl
	uint32_t u32Htonl;
	//This is the size of the header after serialization.
	unsigned int serializedPacketHeaderSize;
	//This is the sending buffer used to serialize the header and packet data.
	char sendingBuffer[MAX_PACKET_SIZE];
	//Buffer to keep track of things acked.
	Packet* ackBuffer;
	//This is the sending ack buffer, to avoid repeat mallocs.
	char sendingAckBuffer[PACKET_HEADER_SIZE + sizeof(char) + sizeof(uint16_t)];
	//This is the return buffer to avoid resizing cost.
	std::string messageReturnBuffer;
	//Port the socket is receving on.
	unsigned short recvPort;
	//Port the socket is sending to
	unsigned short destPort;
	//Result from byteTok
	char* byteTokResult;
	//Raw incoming message buffer
	char messageBuffer[MAX_PACKET_SIZE];
};

#endif