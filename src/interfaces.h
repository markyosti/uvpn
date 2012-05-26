#ifndef INTERFACES_H
# define INTERFACES_H

# if UVPN_SYSTEM == LINUX
#  include "linux/netlink-interfaces.h"
CLASS_ALIAS(NetworkConfig, NetlinkNetworking);
# endif

#endif /* INTERFACES_H */
