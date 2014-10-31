#ifndef POSEIDON_IP_PORT_HPP_
#define POSEIDON_IP_PORT_HPP_

#include "cxx_ver.hpp"
#include <iosfwd>
#include "shared_ntmbs.hpp"

namespace Poseidon {

struct IpPort {
	SharedNtmbs ip;
	unsigned port;

	IpPort()
		: ip(), port()
	{
	}
	IpPort(SharedNtmbs ip_, unsigned port_, bool owning = false)
		: ip(STD_MOVE(ip_), owning), port(port_)
	{
	}
};

extern std::ostream &operator<<(std::ostream &os, const IpPort &rhs);
extern std::wostream &operator<<(std::wostream &os, const IpPort &rhs);

}

#endif
