#include "udp_listener.h"
#include "log.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <cstdint>
#include <climits>
#include <string>

#pragma comment(lib, "ws2_32.lib")

// ---- Packet layout (21 bytes, no padding) --------------------------------
#pragma pack(push, 1)
struct RawPacket {
    uint8_t  version;
    uint8_t  preamble[3];
    uint8_t  packetType;
    uint64_t frameId;
    float    pitch;
    float    roll;
};
#pragma pack(pop)
static_assert(sizeof(RawPacket) == 21, "RawPacket size mismatch");

// --------------------------------------------------------------------------

static bool ParseCIDR(const std::string& cidr, uint32_t& network, uint32_t& mask) {
    auto slash = cidr.find('/');
    std::string ipStr = (slash == std::string::npos) ? cidr : cidr.substr(0, slash);
    int prefix = (slash == std::string::npos) ? 32 : std::stoi(cidr.substr(slash + 1));

    in_addr addr{};
    if (inet_pton(AF_INET, ipStr.c_str(), &addr) != 1) return false;

    network = ntohl(addr.s_addr);
    mask    = (prefix == 0) ? 0u : (~0u << (32 - prefix));
    return true;
}

static bool IsMulticast(uint32_t hostOrderAddr) {
    return (hostOrderAddr & 0xF0000000u) == 0xE0000000u;  // 224.0.0.0/4
}

// --------------------------------------------------------------------------

UdpListener::UdpListener(const DriverConfig& cfg, FrameCallback onFrame, ResetCallback onReset)
    : m_cfg(cfg), m_onFrame(std::move(onFrame)), m_onReset(std::move(onReset))
{
    if (!ParseCIDR(cfg.clientCIDR, m_cidrNetwork, m_cidrMask))
        VRLog("[ArcadeCabVR] Invalid ClientIP CIDR '%s' — all sources will be accepted", cfg.clientCIDR.c_str());
}

UdpListener::~UdpListener() {
    Stop();
}

void UdpListener::Start() {
    m_running = true;
    m_thread  = std::thread(&UdpListener::ListenThread, this);
}

void UdpListener::Stop() {
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

bool UdpListener::MatchesCIDR(uint32_t hostOrderAddr) const {
    return (hostOrderAddr & m_cidrMask) == (m_cidrNetwork & m_cidrMask);
}

void UdpListener::ListenThread() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        VRLog("[ArcadeCabVR] WSAStartup failed");
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        VRLog("[ArcadeCabVR] socket() failed: %d", WSAGetLastError());
        WSACleanup();
        return;
    }

    // Allow multiple processes to share the port
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    // Receive timeout — always 500 ms so we can check m_running even if InvalidatePoseAfter is -1
    DWORD recvTimeout = 500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout));

    sockaddr_in bindAddr{};
    bindAddr.sin_family      = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port        = htons((u_short)m_cfg.listenPort);

    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR) {
        VRLog("[ArcadeCabVR] bind() failed on port %d: %d", m_cfg.listenPort, WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return;
    }

    // Join multicast group on the specified local interface
    in_addr multicastAddr{}, localAddr{};
    inet_pton(AF_INET, m_cfg.listenIP.c_str(), &multicastAddr);
    inet_pton(AF_INET, m_cfg.localIP.c_str(),  &localAddr);

    bool joinedMulticast = false;
    if (IsMulticast(ntohl(multicastAddr.s_addr))) {
        ip_mreq mreq{};
        mreq.imr_multiaddr = multicastAddr;
        mreq.imr_interface = localAddr;
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
            VRLog("[ArcadeCabVR] IP_ADD_MEMBERSHIP failed: %d — continuing without multicast", WSAGetLastError());
        } else {
            joinedMulticast = true;
            VRLog("[ArcadeCabVR] Joined multicast group %s on interface %s port %d",
                m_cfg.listenIP.c_str(), m_cfg.localIP.c_str(), m_cfg.listenPort);
        }
    } else {
        VRLog("[ArcadeCabVR] ListenIP %s is not multicast — listening on port %d",
            m_cfg.listenIP.c_str(), m_cfg.listenPort);
    }

    char buf[64];
    while (m_running) {
        sockaddr_in sender{};
        int senderLen = sizeof(sender);
        int received  = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&sender, &senderLen);

        if (received == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) continue;
            VRLog("[ArcadeCabVR] recvfrom error: %d", WSAGetLastError());
            break;
        }

        uint32_t senderHostAddr = ntohl(sender.sin_addr.s_addr);
        char senderStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender.sin_addr, senderStr, sizeof(senderStr));

        if (m_cfg.debug) {
            VRLog("[ArcadeCabVR] Packet received: %d bytes from %s", received, senderStr);
        }

        if (!MatchesCIDR(senderHostAddr)) {
            if (m_cfg.debug)
                VRLog("[ArcadeCabVR] Rejected packet from %s (not in %s)", senderStr, m_cfg.clientCIDR.c_str());
            continue;
        }

        if (received != (int)sizeof(RawPacket)) {
            VRLog("[ArcadeCabVR] Bad packet size from %s: %d (expected %d)", senderStr, received, (int)sizeof(RawPacket));
            continue;
        }

        RawPacket pkt;
        memcpy(&pkt, buf, sizeof(pkt));

        if (m_cfg.debug) {
            VRLog("[ArcadeCabVR] Pkt ver=%d pre=[%02x %02x %02x] type=%d frameId=%llu pitch=%.4f roll=%.4f",
                pkt.version,
                pkt.preamble[0], pkt.preamble[1], pkt.preamble[2],
                pkt.packetType, (unsigned long long)pkt.frameId,
                pkt.pitch, pkt.roll);
        }

        if (pkt.version != 1) {
            VRLog("[ArcadeCabVR] Error: unexpected version %d from %s (expected 1) — discarding", pkt.version, senderStr);
            continue;
        }

        if (pkt.preamble[0] != 0 || pkt.preamble[1] != 0 || pkt.preamble[2] != 0) {
            VRLog("[ArcadeCabVR] Error: bad preamble [%02x %02x %02x] from %s — discarding",
                pkt.preamble[0], pkt.preamble[1], pkt.preamble[2], senderStr);
            continue;
        }

        if (pkt.packetType == 1) {
            VRLog("[ArcadeCabVR] ResetPuckIdentity from %s", senderStr);
            m_lastFrameId  = 0;
            m_hasSeenFrame = false;
            m_onReset();

        } else if (pkt.packetType == 2) {
            // Accept if newer, or if frameId wrapped around (rollover detection)
            bool isNewer = !m_hasSeenFrame
                        || pkt.frameId > m_lastFrameId
                        || (m_lastFrameId - pkt.frameId) > (UINT64_MAX / 2);

            if (!isNewer) {
                if (m_cfg.debug)
                    VRLog("[ArcadeCabVR] Late frame %llu (last %llu) from %s — discarding",
                        (unsigned long long)pkt.frameId, (unsigned long long)m_lastFrameId, senderStr);
                continue;
            }

            m_lastFrameId  = pkt.frameId;
            m_hasSeenFrame = true;
            m_onFrame(pkt.pitch, pkt.roll);

        } else {
            VRLog("[ArcadeCabVR] Error: unknown packet type %d from %s — discarding", pkt.packetType, senderStr);
        }
    }

    if (joinedMulticast) {
        ip_mreq mreq{};
        inet_pton(AF_INET, m_cfg.listenIP.c_str(), &mreq.imr_multiaddr);
        inet_pton(AF_INET, m_cfg.localIP.c_str(),  &mreq.imr_interface);
        setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    }

    closesocket(sock);
    WSACleanup();
}
