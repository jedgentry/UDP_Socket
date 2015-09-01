#include "UDP_Socket.h"

//Initializes all memory.
UDP_Socket::UDP_Socket()
{
	socketHandle = -1;
	destination.sin_addr.s_addr = -1;
	destination.sin_port = -1;
	receive.sin_addr.s_addr = -1;
	receive.sin_port = -1;
	currentPacketSequence = 0;
	mSendingPacket = new Packet;
	mSendingPacket->packetSize = 0;
	mSendingPacket->partIdentifier = 0;
	mSendingPacket->sequenceIdentifier = 0;
	headerBuffer = new char[sizeof(Packet)];
	u8Htons = 0;
	u16Htons = 0;
	u32Htonl = 0;
	ackBuffer = new Packet[UDP_PACKET_BUFFER_SIZE];
	for (unsigned int i = 0; i < UDP_PACKET_BUFFER_SIZE; i++)
	{
		setupPacketBuffer(i);
	}
	messageReturnBuffer.reserve(MAX_PACKET_SIZE);
	byteTokResult = (char*)malloc(MAX_PACKET_SIZE);
}

UDP_Socket::UDP_Socket(int a, int b, int c, int d, int recvPort, int sendPort)
{
	UDP_Socket();
	if (!initialize(a, b, c, d, recvPort, sendPort))
	{
		perror("Unable to initialize.");
		exit(1);
	}
	this->recvPort = recvPort;
	destPort = sendPort;
}

bool UDP_Socket::initialize(unsigned int port)
{
	if (!initializeSocket())
	{
		printf("Unable to start the socket!\n");
		return false;
	}
	if (!createSocket(port))
	{
		printf("Unable to create the socket!\n");
		return false;
	}
	if (!makeSocketNonBlocking())
	{
		printf("Unable to make socket not block!\n");
		return false;
	}
	//This is old and will need to be depricated.
	serializedPacketHeaderSize = sizeof(uint32_t) * NUM_PACKET_UINT32 + sizeof(uint16_t) * NUM_PACKET_UINT16 + sizeof(uint8_t) * NUM_PACKET_UINT8;
	return true;
}

bool UDP_Socket::initialize(int a, int b, int c, int d, int recvPort, int destPort)
{
	initialize(recvPort);
	setDestination(a, b, c, d, destPort);
	return true;
}

void UDP_Socket::setDestination(int a, int b, int c, int d, int port)
{
	unsigned int destinationAddress = (a << 24) | (b << 16) | (c << 8) | d;
	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = htonl(destinationAddress);
	destination.sin_port = htons(port);
}

void UDP_Socket::sendPacket(char* msg)
{
	char* currentMessage = nullptr;
	size_t msgLength = strlen(msg);
	if (msgLength > MAX_PACKET_SIZE - serializedPacketHeaderSize)
	{
		mSendingPacket->sequenceIdentifier = currentPacketSequence;
		mSendingPacket->packetSize = MAX_PACKET_SIZE - serializedPacketHeaderSize;
		mSendingPacket->partIdentifier = 1;
		mSendingPacket->protocolIdentifier = PACKET_IDENTIFIER;
		while ((currentMessage = byteTok(msg)) != nullptr && msgLength > MAX_PACKET_SIZE - serializedPacketHeaderSize)
		{
			serializeAndSend(currentMessage);
			mSendingPacket->partIdentifier++;
			msgLength -= (MAX_PACKET_SIZE - serializedPacketHeaderSize);
		}
		mSendingPacket->packetSize = serializedPacketHeaderSize + (uint32_t)strlen(currentMessage);
		serializeAndSend(currentMessage);
		++currentPacketSequence;
		mSendingPacket->partIdentifier = 0;
	}
	else
	{
		mSendingPacket->sequenceIdentifier = currentPacketSequence;
		mSendingPacket->packetSize = serializedPacketHeaderSize + (uint32_t)msgLength;
		serializeAndSend(msg);
		++currentPacketSequence;
	}
}
//Raw Send message, no packet header.
void UDP_Socket::sendMessage(const char* msg)
{
	//Calculates the number of bytes sent in order to function.
	//Packets need to be padded if theyre a constant size.
	size_t sent_bytes = sendto(socketHandle, msg, strlen(msg), 0, (sockaddr*)&destination, sizeof(sockaddr_in));
	printf("SENT_BYTES: %d", (int)sent_bytes);
#if PLATFORM == WINDOWS
	if (DEBUG)
	{
		//Check if there is an error on the send call.
		if (sent_bytes == SOCKET_ERROR)
		{
			//Print the error.
			printf("Sendto has failed with error: %d\n", WSAGetLastError());
			Sleep(1000);
		}
	}
#endif
	//Todo: Larger than buffer size of packets.
}

//NOTE: UDP will drop anything that does not fit in the buffer size! so the buffer size must be the maximum
//possible size that this program will send a packet!
void UDP_Socket::receivePackets()
{
	int i = 0;
	while (true)
	{
		int protocolHandler = 0;
#if PLATFORM == WINDOWS
		int receiveLength = sizeof(receive);
#else
		unsigned int receiveLength = sizeof(receive);
#endif
		int bytes = recvfrom(socketHandle, messageBuffer, MAX_PACKET_SIZE, 0, (sockaddr*)&receive, &receiveLength);
		if (bytes <= 0 || (protocolHandler = checkProtocol()) == 0)
			break;
		messageBuffer[bytes] = '\0';
		sendACK();
		//TODO: Save these!
		unsigned int receiveAddress = ntohl(receive.sin_addr.s_addr);
		unsigned int receivePort = ntohs(receive.sin_port);
		parseMessages(messageBuffer, i, bytes);
		i++;
	}
	sortMessageBuffer();
}

std::string UDP_Socket::getReceivedMessageBuffer(int input)
{
	//Piece together message if it comes in parts.
	messageReturnBuffer.erase();
	if (receivedPacketsBuffer[input]->partIdentifier)
	{
		int currentSize = receivedPacketsBuffer[input]->packetSize, i = 0;
		while (receivedPacketsBuffer[input + i]->packetSize == currentSize)
		{
			messageReturnBuffer += receivedPacketsBuffer[input + i]->message;
			i++;
		}
		messageReturnBuffer += receivedPacketsBuffer[input + i]->message;
		return messageReturnBuffer;
	}
	else
	{
		messageReturnBuffer += receivedPacketsBuffer[input]->message;
		return messageReturnBuffer;
	}
}

void UDP_Socket::shutdown()
{
	shutdownSocket();
	free(messageBuffer);
	free(byteTokResult);
	free(packetData);
	delete[] headerBuffer;
	delete[] ackBuffer;
	for (unsigned int i = 0; i < UDP_PACKET_BUFFER_SIZE; i++)
	{
		delete receivedPacketsBuffer[i];
	}
	delete mSendingPacket;
}

UDP_Socket::~UDP_Socket()
{
	shutdown();
}

/////////////////////////////
//    Private Functions    //
/////////////////////////////
bool UDP_Socket::initializeSocket()
{
	//TODO: Add in other platform initializers
	//Startup winsock.
#if PLATFORM == WINDOWS
	WSADATA WsaData;
	return (WSAStartup(MAKEWORD(2, 2), &WsaData) == NO_ERROR);
#endif
	return true;
}

bool UDP_Socket::createSocket(unsigned int port)
{
	socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketHandle < 1)
	{
		printf("Failed to create the socket\n");
#if PLATFORM == WINDOWS
		if (socketHandle == SOCKET_ERROR)
		{
			printf("Winsock had an error in socket call with code: %d\n", WSAGetLastError());
			Sleep(5000);
			exit(-1);
		}
#endif
		return false;
	}
	if (!bindSocket(port))
	{
		printf("Unable to bind the socket!\n");
		return false;
	}
	//Handle has been acquired.
	return true;
}

bool UDP_Socket::bindSocket(unsigned int port)
{
	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = htonl(INADDR_ANY);
	destination.sin_port = htons((unsigned short)port); //This should be port
	//Bind the socket
	if (bind(socketHandle, (const sockaddr*)&destination, sizeof(sockaddr_in)) < 0)
	{
		printf("Was unable to bind the socket in bindSocket!\n");
		return false;
	}
	recvPort = port;
	return true;
}

void UDP_Socket::parseMessages(char* message, int i, int bytes)
{
	//Move packet into memory and deserialize.
	memcpy(&receivedPacketsBuffer[i]->packetSize, message, sizeof(uint32_t));
	receivedPacketsBuffer[i]->packetSize = htonl(receivedPacketsBuffer[i]->packetSize);
	memcpy(&receivedPacketsBuffer[i]->sequenceIdentifier, message + sizeof(uint32_t), sizeof(uint32_t));
	receivedPacketsBuffer[i]->sequenceIdentifier = htonl(receivedPacketsBuffer[i]->sequenceIdentifier);
	memcpy(&receivedPacketsBuffer[i]->protocolIdentifier, message + sizeof(uint32_t) * 2, sizeof(uint16_t));
	receivedPacketsBuffer[i]->protocolIdentifier = htons(receivedPacketsBuffer[i]->protocolIdentifier);
	memcpy(&receivedPacketsBuffer[i]->partIdentifier, message + sizeof(uint32_t) * 2 + sizeof(uint16_t), sizeof(uint16_t));
	receivedPacketsBuffer[i]->partIdentifier = htons(receivedPacketsBuffer[i]->partIdentifier);
	memcpy(&receivedPacketsBuffer[i]->message, message + sizeof(uint32_t) * NUM_PACKET_UINT32 + sizeof(uint16_t) * NUM_PACKET_UINT16, bytes);
}

bool UDP_Socket::makeSocketNonBlocking()
{
#if PLATFORM == MAC || PLATFORM == LINUX
	int nonBlocking = 1;
	if (fcntl(socketHandle, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
	{
		printf("Unable to set socket to non blocking on mac or linux\n");
		return false;
	}
#elif PLATFORM == WINDOWS
	DWORD nonBlocking = 1;
	if (ioctlsocket(socketHandle, FIONBIO, &nonBlocking) != 0)
	{
		printf("Failed to set windows socket to non blocking!");
		return false;
	}
#endif

	return true;
}

int UDP_Socket::serializeHeader(Packet* packet)
{
	//Serializing by order of variables in packet.
	u32Htonl = htonl(packet->packetSize);
	memcpy(sendingBuffer + 0, &u32Htonl, sizeof(uint32_t));
	u32Htonl = htons(packet->sequenceIdentifier);
	memcpy(sendingBuffer + sizeof(uint32_t), &u32Htonl, sizeof(uint32_t)); //Serialize sequence
	u16Htons = htons(packet->protocolIdentifier);
	memcpy(sendingBuffer + sizeof(uint32_t) * 2, &u16Htons, sizeof(uint16_t)); //Serialize protocolIdentifier
	u16Htons = htons(packet->partIdentifier);
	memcpy(sendingBuffer + sizeof(uint32_t) * 2 + sizeof(uint16_t), &u16Htons, sizeof(uint16_t));
	return sizeof(uint32_t) * NUM_PACKET_UINT32 + sizeof(uint16_t) * NUM_PACKET_UINT16 + sizeof(uint8_t) * NUM_PACKET_UINT8;
}

int UDP_Socket::addPacketData(Packet* packet, char* message, int bufferIndex)
{
	memcpy(sendingBuffer + bufferIndex, message, strlen(message));
	return bufferIndex + strlen(message);
}

void UDP_Socket::sendACK()
{
	//Copy the header over for the ack.
	memcpy(sendingAckBuffer, messageBuffer, serializedPacketHeaderSize + ACK_BODY_SIZE);
	//Overwrite the size bytes. Add in the fact that it will be A + PROTOCOL_IDENTIFIER that was acked.
	uint32_t ackSize = htonl(serializedPacketHeaderSize + ACK_BODY_SIZE);
	memcpy(sendingAckBuffer, &ackSize, sizeof(uint32_t));
	//Copy the protocol Identifier to the Ack mesage.
	char message = 'A';
	//Copy the A over.
	memcpy(sendingAckBuffer + serializedPacketHeaderSize, &message, sizeof(char));
	//Copy the protocol acked over.
	memcpy(sendingAckBuffer + serializedPacketHeaderSize + sizeof(char), sendingAckBuffer + sizeof(uint32_t) * 2, sizeof(uint16_t));
	//Overwrite the protocolIdentifier to the ack one
	uint16_t htonAckSize = htons(ACK_IDENTIFIER);
	memcpy(sendingAckBuffer + sizeof(uint32_t) * 2, &htonAckSize, sizeof(uint16_t));
	//Null terminate the string.
	message = '\0';
	memcpy(sendingAckBuffer + sizeof(sendingAckBuffer), &message ,sizeof(char));
	sendMessage(sendingAckBuffer);
}

void UDP_Socket::serializeAndSend(char* message)
{
	for (int i = 0; i < REPEAT_SENDS; i++)
	{
		int headerLength = serializeHeader(mSendingPacket);
		printf("htonLength: %d \n headerSize: %d\n", headerLength, serializedPacketHeaderSize);
		int sendingBufferSize = addPacketData(mSendingPacket, message, headerLength);
		int sent_bytes = sendto(socketHandle, sendingBuffer, sendingBufferSize, 0, (sockaddr*)&destination, sizeof(sockaddr_in));
		printf("Sent_bytes: %d\n", sent_bytes);
#if PLATFORM == WINDOWS
		if (DEBUG)
		{
			//Check if there is an error on the send call.
			if (sent_bytes == SOCKET_ERROR)
			{
				//Print the error.
				printf("Sendto has failed with error: %d\n", WSAGetLastError());
				Sleep(1000);
			}
		}
#endif
	}
}

//This is similar to strtok, however it works in bytes on the string to split the message.
char* UDP_Socket::byteTok(char* input)
{
	//Setup tokens.
	static int currentToken = 0;
	//Unlike strtok we dont know if the string changed, so this is needed to verify.
	static char* currentStr = input;
	//Error handling in case values are NULL.
	if (!input || input[currentToken] == '\0')
	{
		currentToken = 0;
		return nullptr;
	}
	//Allocate memory for our return.
	unsigned int i = 0;
	//Loop through and fill return buffer.
	while (input[currentToken + i] != '\0' && i < MAX_PACKET_SIZE - serializedPacketHeaderSize)
	{
		byteTokResult[i] = input[currentToken + i];
		i++;
	}
	//Null terminate our buffer.
	byteTokResult[i] = '\0';
	//If we are done partitioning reset our tokens.
	if (i != MAX_PACKET_SIZE - serializedPacketHeaderSize)
	{
		currentToken = 0;
		currentStr = nullptr;
	}
	//Otherwise we continue at the left off place.
	else
	{
		currentToken += i;
	}
	return byteTokResult;
}

void UDP_Socket::sortMessageBuffer()
{
	int lowerBound = 0;
	int upperBound = 1;
	//Go until the end of the array.
	while (receivedPacketsBuffer[lowerBound]->sequenceIdentifier * NUM_DIGITS_UINT16 + receivedPacketsBuffer[lowerBound]->partIdentifier
		< receivedPacketsBuffer[upperBound]->sequenceIdentifier * NUM_DIGITS_UINT16 + receivedPacketsBuffer[upperBound]->partIdentifier
		&& upperBound < UDP_PACKET_BUFFER_SIZE)
	{
		lowerBound++;
		upperBound++;
	}
	//Nothing is out of order.
	if (upperBound == UDP_PACKET_BUFFER_SIZE)
		return;
	//This means something is out of order from this point on.
	quicksort(lowerBound, upperBound);
}

void UDP_Socket::quicksort(int lowerBound, int upperBound)
{
	//End case recursion.
	if (lowerBound >= upperBound)
		return;
	int current = pivot(lowerBound, upperBound);
	quicksort(lowerBound, current - 1);
	quicksort(current + 1, upperBound);
}

int UDP_Socket::pivot(int lowerBound, int upperBound)
{
	int middle = lowerBound + (upperBound - lowerBound) / 2;
	std::swap(receivedPacketsBuffer[middle], receivedPacketsBuffer[lowerBound]);
	int i = lowerBound + 1;
	int j = upperBound;
	while (i <= j)
	{
		while (i <= j && (receivedPacketsBuffer[i]->sequenceIdentifier * NUM_DIGITS_UINT16 + receivedPacketsBuffer[i]->partIdentifier)
			<= receivedPacketsBuffer[middle]->sequenceIdentifier * NUM_DIGITS_UINT16 + receivedPacketsBuffer[middle]->partIdentifier)
		{
			i++;
		}
		while (i <= j && (receivedPacketsBuffer[j]->sequenceIdentifier * NUM_DIGITS_UINT16 + receivedPacketsBuffer[j]->partIdentifier)
			<= receivedPacketsBuffer[middle]->sequenceIdentifier * NUM_DIGITS_UINT16 + receivedPacketsBuffer[middle]->partIdentifier)
		{
			j--;
		}
		if (i < j)
		{
			std::swap(receivedPacketsBuffer[i], receivedPacketsBuffer[j]);
		}
	}
	std::swap(receivedPacketsBuffer[i - 1], receivedPacketsBuffer[lowerBound]);
	return (i - 1);
}

void UDP_Socket::shutdownSocket()
{
#if PLATFORM == WINDOWS
	WSACleanup();
	if (socketHandle)
		closesocket(socketHandle);
#else
	if (close(socketHandle) == -1)
	{
		fprintf(stderr, "Unable to close socket on Port:%d\n", PORT);
		exit(-1);
	}
#endif
}

void UDP_Socket::setupPacketBuffer(unsigned int index)
{
	receivedPacketsBuffer[index] = new Packet;
	receivedPacketsBuffer[index]->packetSize = -1;
	receivedPacketsBuffer[index]->partIdentifier = 0;
	receivedPacketsBuffer[index]->sequenceIdentifier = -1;
	memset(receivedPacketsBuffer[index]->message, '\0', MAX_PACKET_SIZE);
	*ackBuffer[index].message = 'A';
	ackBuffer[index].packetSize = -1;
	ackBuffer[index].partIdentifier = 0;
	ackBuffer[index].sequenceIdentifier = -1;
}

int UDP_Socket::checkProtocol()
{
	uint16_t protocolCheck = 0;
	memcpy(&protocolCheck, messageBuffer + sizeof(uint32_t) * 2, sizeof(uint16_t));
	protocolCheck = ntohs(protocolCheck);
	switch (protocolCheck)
	{
	case ACK_IDENTIFIER:
		return ACK_IDENTIFIER;
	}
	if (protocolCheck != ACK_IDENTIFIER || protocolCheck != PACKET_IDENTIFIER)
	{
		return true;
	}
	return false;
}