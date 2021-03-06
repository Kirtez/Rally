#include "Rally.h"

#include "view/RallyNetView.h"

#ifdef PLATFORM_WINDOWS
#include <WinSock2.h>
//#include <fcntl.h>
#pragma comment(lib, "wsock32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
// Not needed here
//#include <sys/types.h>
#include <arpa/inet.h>
//#include <netdb.h>
// close() is in here for newer gcc
#include <unistd.h>
#include <errno.h>
#endif

#include <stdexcept>
#include <map>
#include <climits>


namespace Rally { namespace View {

    namespace {
        const unsigned int MAX_PACKET_SIZE = 255;
        const unsigned int CLIENT_TIMEOUT_DELAY = 40; // seconds
        const float SEND_RATE_LIMIT = 0.01f; // seconds, how often to update to server (max).

        // Some ugly code to fill in a vector3 to a packet. Should endian-convert from
        // host to network byte order if necessary. (Usually it isn't necessary for float.)
        void writeVector3toPacket(unsigned char* packet, const Rally::Vector3 & vector) {
            memcpy(packet + 0*4, &vector.x, 4);
            memcpy(packet + 1*4, &vector.y, 4);
            memcpy(packet + 2*4, &vector.z, 4);
        }

        // Some ugly code to fill in a vector3 to a packet. Should endian-convert from
        // host to network byte order if necessary. (Usually it isn't necessary for float.)
        void writeQuaternionToPacket(unsigned char* packet, const Rally::Quaternion& quaternion) {
            memcpy(packet + 0*4, &quaternion.w, 4);
            memcpy(packet + 1*4, &quaternion.x, 4);
            memcpy(packet + 2*4, &quaternion.y, 4);
            memcpy(packet + 3*4, &quaternion.z, 4);
        }

        // Takes a packet with offset pre-added and returns a Vector3. This should
        // convert from network to host byte order if necessary (usually not for float).
        Rally::Vector3 packetToVector3(unsigned char* packet) {
            float* a = reinterpret_cast<float*>(packet + 0*4);
            float* b = reinterpret_cast<float*>(packet + 1*4);
            float* c = reinterpret_cast<float*>(packet + 2*4);

            return Rally::Vector3(*a, *b, *c);
        }

        // Takes a packet with offset pre-added and returns a Vector3. This should
        // convert from network to host byte order if necessary (usually not for float).
        Rally::Quaternion packetToQuaternion(unsigned char* packet) {
            float* w = reinterpret_cast<float*>(packet + 0*4);
            float* x = reinterpret_cast<float*>(packet + 1*4);
            float* y = reinterpret_cast<float*>(packet + 2*4);
            float* z = reinterpret_cast<float*>(packet + 3*4);

            return Rally::Quaternion(*w, *x, *y, *z);
        }

        // Windows does it another way. Actually a better way too...
        int getErrno() {
#ifdef PLATFORM_WINDOWS
            return ::WSAGetLastError();
#else
            return errno;
#endif
        }

        bool isNonblockErrno() {
            int error = getErrno();
#ifdef PLATFORM_WINDOWS
            return error == WSAEWOULDBLOCK; // There's no WSAEAGAIN
#else
            return (error == EWOULDBLOCK || error == EAGAIN);
#endif
        }

        bool isBadConnErrno() {
            // Typically we get WSAECONNRESET on windows, ECONNREFUSED on *nix
            int error = getErrno();
#ifdef PLATFORM_WINDOWS
            return (error == WSAECONNREFUSED || error == WSAECONNABORTED || error == WSAECONNRESET);
#else
            return (error == ECONNREFUSED || error == ECONNABORTED || error == ECONNRESET);
#endif
        }

		void cleanupSocket(int socket) {
#ifdef PLATFORM_WINDOWS
			if(socket) ::closesocket(socket);
#else
			if(socket) ::close(socket);
#endif
		}
    }

    RallyNetView::RallyNetView(RallyNetView_NetworkCarListener & listener)
        : socket(0),
        lastSentPacketId(0),
        listener(listener) {
    }

    RallyNetView::~RallyNetView() {
		cleanupSocket(socket);
#ifdef PLATFORM_WINDOWS
		::WSACleanup();
#endif
    }

    void RallyNetView::initialize(const std::string & serverAddress, unsigned short serverPort, const Model::Car* playerCar) {
        this->playerCar = playerCar;

    #ifdef PLATFORM_WINDOWS
        WSADATA WsaData;
        if(::WSAStartup(MAKEWORD(2, 2), &WsaData) != NO_ERROR) {
            throw std::runtime_error("WSAStartup failed!");
        }
    #endif

        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(socket <= 0) {
			std::cerr << "Could not create socket for RallyNet. Starting without multiplayer support!" << std::endl;
			cleanupSocket(socket);
			socket = 0;
			return;
            //throw std::runtime_error("Could not connect!");
        }

        sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        // TODO: use getaddrinfo() if we need more flexibility in the future
        address.sin_addr.s_addr = inet_addr(serverAddress.c_str());
        address.sin_port = htons(serverPort);

        // Note that connect assigns the local server:port choosen by the OS to this socket!
        if(::connect(socket, (const sockaddr*) & address, sizeof(sockaddr_in)) < 0) {
            // Throwing here is ok, since we're using UDP so no connection will actually happen,
            // meaning it won't fail because of network problems.
            // throw std::runtime_error("Could not connect to server!");

			// The above is out-commented, but still applies. There was suspicion that
			std::cerr << "Could not connect to RallyNet. Starting without multiplayer support!" << std::endl;
			cleanupSocket(socket);
			socket = 0;
			return;
        }

        bool nonBlockSucceded = false;
#ifdef PLATFORM_WINDOWS
        DWORD nonBlocking = 1;
        nonBlockSucceded = (ioctlsocket(socket, FIONBIO, &nonBlocking) == 0);
#else
        int flags = fcntl(socket, F_GETFL, 0);
        //flags &= ~O_NONBLOCK;
        flags |= O_NONBLOCK;
        nonBlockSucceded = (fcntl(socket, F_SETFL, flags) == 0);
#endif

        if(!nonBlockSucceded) {
            throw std::runtime_error("Could not make socket non-blocking!");
        }

        rateLimitTimer.reset();
    }

    void RallyNetView::pullRemoteChanges() {
		if(!socket) return;

        pullCars();
    }

    void RallyNetView::pushLocalChanges() {
		if(!socket) return;

        if(rateLimitTimer.getElapsedSeconds() >= SEND_RATE_LIMIT) {
            rateLimitTimer.reset();
            pushCar();
        }
    }

    void RallyNetView::pushCar() {
        unsigned char packet[48];

        packet[0] = 1; // Type = 1

        // sequence id
        unsigned short packetId = htons(++lastSentPacketId);
        memcpy(packet + 1, &packetId, 2);

        packet[3]  = playerCar->getCarType();

        writeVector3toPacket(packet + 4 + 0*3*4, playerCar->getPosition());
        writeVector3toPacket(packet + 4 + 1*3*4, playerCar->getVelocity());
        writeQuaternionToPacket(packet + 4 + 2*3*4, playerCar->getOrientation());

        Rally::Vector4 tractionVector = playerCar->getTractionVector();
        packet[44] = static_cast<unsigned char>(tractionVector.x*255.0f);
        packet[45] = static_cast<unsigned char>(tractionVector.y*255.0f);
        packet[46] = static_cast<unsigned char>(tractionVector.z*255.0f);
        packet[47] = static_cast<unsigned char>(tractionVector.w*255.0f);

        int status = ::send(socket, reinterpret_cast<char*>(packet), sizeof(packet), 0x00000000);
        if(status < 0) {
            if(isNonblockErrno()) {
                // Since we have a non-blocking socket, we may use this. This means we
                // didn't throttle the messages well enough for our own system, so just
                // skip this update and try again with fresh data next time...
            } else if(isBadConnErrno()) {
                // We may actually get this if the server replies with an ICMP packet
                // (i.e. server application not started, so the port is not in use).
            } else {
                // Note that we don't care if we couldn't send all bytes above (status >= 0)...
                throw std::runtime_error("Socket error when sending position update to server.");
            }
        }
    }

    void RallyNetView::pullCars() {
        unsigned char packet[MAX_PACKET_SIZE];
        while(true) {
            int receivedBytes = ::recv(socket, reinterpret_cast<char*>(packet), MAX_PACKET_SIZE, 0x00000000);
            if(receivedBytes < 0) {
                if(isNonblockErrno()) {
                    // No more messages, goto remote client cleanup below
                    break;
                } else if(isBadConnErrno()) {
                    // We may actually get this if the server replies with an ICMP packet
                    // (i.e. server application not started, so the port is not in use).
                } else {
                    throw std::runtime_error("Socket error when receiving packet.");
                }
            } if(receivedBytes == 50 && packet[0] == 1) { // complete packetType == 1
                unsigned short sequenceId;
                memcpy(&sequenceId, packet+1, 2);
                sequenceId = ntohs(sequenceId);

                unsigned short playerId;
                memcpy(&playerId, packet + 4, 2);
                playerId = ntohs(playerId);

                std::map<unsigned short, RallyNetView_InternalClient>::iterator sendingClientIterator = clients.find(playerId);

                // If client was found as previous sender, make sure this packet is relevant
                if(sendingClientIterator != clients.end()) {
                     if(sendingClientIterator->second.lastPositionSequenceId > sequenceId) {
                        // Possible wrap-around?
                        if(sendingClientIterator->second.lastPositionSequenceId > USHRT_MAX/2 && sequenceId < USHRT_MAX/2) {
                            // If we use local 32-bit seq.id like the server in the future: lastPositionPacketId += USHRT_MAX + 1;
                        } else {
                            // Drop packet!
                            continue;
                        }
                    }
                }

                // Lazily create the internal client if not already existant.
                RallyNetView_InternalClient& internalClient = (sendingClientIterator == clients.end()) ? clients[playerId] : sendingClientIterator->second;

                internalClient.lastPositionSequenceId = sequenceId;
                internalClient.lastPacketArrival = ::time(0);

                char carType = packet[3];
                Rally::Vector4 tractionVector = Rally::Vector4(packet[46], packet[47], packet[48], packet[49]) / 255.0f;

                listener.carUpdated(
                    playerId,
                    packetToVector3(packet + 6 + 0*4*3),
                    packetToQuaternion(packet + 6 + 2*4*3),
                    packetToVector3(packet + 6 + 1*4*3),
                    carType,
                    tractionVector);
            }
        }

        // Clean up clients that has not been responding for a while, e.g. timed out.
        cleanupClients();
    }

    void RallyNetView::cleanupClients() {
        time_t now = ::time(0);
        for(std::map<unsigned short, RallyNetView_InternalClient>::iterator clientIterator = clients.begin();
                clientIterator != clients.end();
                /*++clientIterator*/) {
            RallyNetView_InternalClient& internalClient = clientIterator->second;

            // Remove client if it hasn't responded the last CLIENT_TIMEOUT_DELAY seconds.
            // difftime(end, start) -> double seconds
            if(internalClient.lastPacketArrival != 0 &&
                    difftime(now, internalClient.lastPacketArrival) >= CLIENT_TIMEOUT_DELAY) {
                listener.carRemoved(clientIterator->first);
                clients.erase(clientIterator++); // Copy iterator, advance, then delete from copied iterator
            } else {
                ++clientIterator;
            }
        }
    }
} }
