#include "xx_proxy.h"

#include <base/log.h>
#include <base/net.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/shared/config.h>
#include <engine/shared/network.h>
#include <engine/shared/packer.h>
#include <engine/shared/uuid_manager.h>
#include <engine/shared/xx_netdump.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace
{
	struct CPeer
	{
		NETADDR m_Address;
		NETSOCKET m_Socket;
		int64_t m_LastActivity;
		bool m_Sixup = false;
	};

	void DumpPacket(CPeer &Peer, unsigned char *pData, int Size, bool FromServer, int Level)
	{
		CNetPacketConstruct Packet;
		bool Sixup = Peer.m_Sixup;
		SECURITY_TOKEN Token, ResponseToken;
		char aWho[NETADDR_MAXSTRSIZE];
		net_addr_str(&Peer.m_Address, aWho, sizeof(aWho), true);
		const char *pDirection = FromServer ? "S->C" : "C->S";
		if(CNetBase::UnpackPacket(pData, Size, &Packet, Sixup, true, &Token, &ResponseToken) != 0)
		{
			log_info("proxy", "%s %s undecoded UDP packet (%d bytes)", pDirection, aWho, Size);
			return;
		}
		if(Packet.m_Flags & NET_PACKETFLAG_CONNLESS)
		{
			log_info("proxy", "%s %s connless (%d bytes)", pDirection, aWho, Size);
			return;
		}
		Peer.m_Sixup = Sixup;
		if(Packet.m_Flags & NET_PACKETFLAG_CONTROL)
		{
			if(Level >= 2 || Packet.m_aChunkData[0] != NET_CTRLMSG_KEEPALIVE)
				log_info("proxy", "%s %s control=%d protocol=0.%d (%d bytes)", pDirection, aWho, Packet.m_aChunkData[0], Sixup ? 7 : 6, Size);
			return;
		}

		// Decode a copy only. Forwarded bytes, tokens, ACKs and retransmissions stay intact.
		int Offset = 0;
		for(int i = 0; i < Packet.m_NumChunks; ++i)
		{
			if(Packet.m_DataSize - Offset < 2)
				return;
			unsigned char *pHeader = Packet.m_aChunkData + Offset;
			const int HeaderSize = (pHeader[0] >> 6) & NET_CHUNKFLAG_VITAL ? 3 : 2;
			if(Packet.m_DataSize - Offset < HeaderSize)
				return;
			CNetChunkHeader Header;
			unsigned char *pMessage = Header.Unpack(pHeader, Sixup ? 6 : 4);
			Offset += HeaderSize;
			if(Header.m_Size > Packet.m_DataSize - Offset)
				return;
			Offset += Header.m_Size;

			CUnpacker Unpacker;
			Unpacker.Reset(pMessage, Header.m_Size);
			const int MsgAndSys = Unpacker.GetInt();
			if(Unpacker.Error() || MsgAndSys < 0 || (MsgAndSys >> 1) >= OFFSET_UUID)
				continue;
			int Msg = MsgAndSys >> 1;
			if(Msg == 0)
			{
				CUuid Uuid;
				Msg = g_UuidManager.UnpackUuid(&Unpacker, &Uuid);
				if(Msg == UUID_INVALID)
					continue;
			}
			XxNetDump(nullptr, Level, pDirection, aWho, MsgAndSys & 1, Msg, pMessage, Header.m_Size, Header.m_Flags & NET_CHUNKFLAG_VITAL, Sixup);
		}
	}
}

int RunXxProxy(const CConfig *pConfig, bool (*pfnInterrupted)())
{
	NETADDR Destination = {};
	if(!pConfig->m_XxProxyDst[0] || net_host_lookup(pConfig->m_XxProxyDst, &Destination, NETTYPE_IPV4 | NETTYPE_IPV6) != 0)
	{
		log_error("proxy", "set xx_proxy_dst to a valid server address, e.g. 127.0.0.1:8303");
		return -1;
	}
	if(!Destination.port)
		Destination.port = 8303;

	NETADDR Bind = {};
	// Default to loopback; LAN access can be enabled with bindaddr.
	if(net_host_lookup(pConfig->m_Bindaddr[0] ? pConfig->m_Bindaddr : "127.0.0.1", &Bind, pConfig->m_SvIpv4Only ? NETTYPE_IPV4 : NETTYPE_IPV4 | NETTYPE_IPV6) != 0)
	{
		log_error("proxy", "could not resolve bindaddr '%s'", pConfig->m_Bindaddr);
		return -1;
	}
	Bind.port = pConfig->m_SvPort ? pConfig->m_SvPort : 8303;
	if(net_addr_comp(&Bind, &Destination) == 0)
	{
		log_error("proxy", "proxy and destination addresses must differ (use a different sv_port)");
		return -1;
	}

	NETSOCKET Listener = net_udp_create(Bind);
	if(!Listener)
	{
		log_error("proxy", "could not listen on port %d: %s", Bind.port, net_error_message().c_str());
		return -1;
	}
	CNetBase::Init();
	char aBind[NETADDR_MAXSTRSIZE], aDestination[NETADDR_MAXSTRSIZE];
	net_addr_str(&Bind, aBind, sizeof(aBind), true);
	net_addr_str(&Destination, aDestination, sizeof(aDestination), true);
	log_info("proxy", "listening on %s -> %s, console dump level %d", aBind, aDestination, pConfig->m_XxProxyNet);

	std::vector<CPeer> vPeers;
	while(!pfnInterrupted())
	{
		bool Received = false;
		NETADDR From;
		unsigned char *pData;
		int Size;
		// Bound each batch so a busy socket cannot starve other clients or shutdown.
		for(int Batch = 0; Batch < 256 && (Size = net_udp_recv(Listener, &From, &pData)) > 0; ++Batch)
		{
			Received = true;
			auto It = std::find_if(vPeers.begin(), vPeers.end(), [&](const CPeer &Peer) { return net_addr_comp(&Peer.m_Address, &From) == 0; });
			if(It == vPeers.end())
			{
				if(vPeers.size() >= (size_t)pConfig->m_SvMaxClients)
					continue;
				NETADDR UpstreamBind = {};
				UpstreamBind.type = Destination.type;
				NETSOCKET Upstream = net_udp_create(UpstreamBind);
				if(!Upstream)
				{
					log_error("proxy", "could not create upstream socket: %s", net_error_message().c_str());
					continue;
				}
				vPeers.push_back({From, Upstream, time_get()});
				It = vPeers.end() - 1;
			}
			It->m_LastActivity = time_get();
			if(net_udp_send(It->m_Socket, &Destination, pData, Size) < 0)
				log_error("proxy", "send to server failed: %s", net_error_message().c_str());
			DumpPacket(*It, pData, Size, false, pConfig->m_XxProxyNet);
		}

		for(auto It = vPeers.begin(); It != vPeers.end();)
		{
			for(int Batch = 0; Batch < 256 && (Size = net_udp_recv(It->m_Socket, &From, &pData)) > 0; ++Batch)
			{
				Received = true;
				if(net_addr_comp(&From, &Destination) != 0)
					continue;
				It->m_LastActivity = time_get();
				if(net_udp_send(Listener, &It->m_Address, pData, Size) < 0)
					log_error("proxy", "send to client failed: %s", net_error_message().c_str());
				DumpPacket(*It, pData, Size, true, pConfig->m_XxProxyNet);
			}
			if(time_get() - It->m_LastActivity > 60 * time_freq())
			{
				net_udp_close(It->m_Socket);
				It = vPeers.erase(It);
			}
			else
				++It;
		}
		if(!Received)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	for(const auto &Peer : vPeers)
		net_udp_close(Peer.m_Socket);
	net_udp_close(Listener);
	return 0;
}
