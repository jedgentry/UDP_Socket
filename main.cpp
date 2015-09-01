#define _WINSOCKAPI_
#include "Windows.h"
#include <iostream>

#include "UDP_Socket.h"

int main(int argc, char* argv[])
{
	UDP_Socket* netA = new UDP_Socket();
	UDP_Socket* netB = new UDP_Socket();
	if (!netA->initialize(127, 0, 0, 1, 6789, 6790) || !netB->initialize(127,0,0,1,6790,6789))
	{
		printf("Unable to initialize the network!\n");
		return -1;
	}
	while (1)
	{
		printf("Sent!\n");
		netA->sendPacket("The_quick_brown_fox_jumped_over_the_lazy_dog_because_it_was_dead.\0");
		printf("Send2\n");
		netB->sendPacket("Test");
		netB->receivePackets();
		netA->receivePackets();
		std::cout << "RECV: " << netB->getReceivedMessageBuffer(0) << std::endl << "RECV" << netA->getReceivedMessageBuffer(0) << std::endl;
		Sleep(1000);
	}
	return 0;
}