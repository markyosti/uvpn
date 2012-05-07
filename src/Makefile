#CXX = llvm-g++-4.2
CPPFLAGS = -Wall -pedantic -ggdb3 -O0 -DPRINT_DEBUG -Wformat-nonliteral -Wextra -Wstrict-overflow=5 -Wfloat-equal -Wconversion -Wlogical-op -fstack-protector-all -Wno-unused-parameter -fno-exceptions #-Weffc++ -Wextra
# CPPFLAGS = -Wall -pedantic -O0 -DPRINT_DEBUG
# -Wunreachable-code -> causes lot of warnings due to standard libraries!
# -Wformat-zero-length -> C and objective C only.
# -Wshadow -> arguments like size and friends shadow globals :(
# TRY USING mudflap library!

LDFLAGS = -lssl -lcrypto -lm -lstdc++ -rdynamic -ggdb3 -lduma
ALLSOURCES = *.cc

all: deps uvpn-user uvpn-client uvpn-server uvpn-ip-config
	
-include deps

deps: Makefile
	fastdep --extraremakedep=Makefile --remakedeptarget=deps $(ALLSOURCES) > deps

uvpn-ip-config: uvpn-ip-config.o netlink-interfaces.o ip-addresses.o backtrace.o

uvpn-user: uvpn-user.o userdb.o base64.o srp-common.o srp-passwd.o openssl-helpers.o prng.o terminal.o backtrace.o password.o

uvpn-client: uvpn-client.o scramble-session-protector.o prng.o dispatcher.o openssl-helpers.o srp-client-authenticator.o scramble-session-protector.o srp-common.o srp-client.o srp-passwd.o base64.o socket-transport.o tun-tap-common.o tun-tap-client-channel.o packet-queue.o backtrace.o sockaddr.o openssl-protector.o password.o aes-session-protector.o terminal.o ip-addresses.o netlink-interfaces.o client-tcp-transcoder.o client-udp-transcoder.o user-chatter.o terminal-user-chatter.o client-simple-connection-manager.o

uvpn-server: uvpn-server.o scramble-session-protector.o prng.o dispatcher.o openssl-helpers.o srp-server-authenticator.o scramble-session-protector.o srp-common.o srp-server.o srp-passwd.o base64.o socket-transport.o tun-tap-common.o tun-tap-server-channel.o packet-queue.o backtrace.o sockaddr.o userdb.o base64.o terminal.o openssl-protector.o password.o aes-session-protector.o ip-addresses.o ip-manager.o netlink-interfaces.o server-tcp-transcoder.o server-simple-connection-manager.o server-udp-transcoder.o

%.o: %.cc
	@echo "  * COMPILING $<"
	@$(CXX) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -f ./*.o
	rm -f ./uvpn-user ./uvpn-client ./uvpn-server ./uvpn-ip-config
	# $(MAKE) -C tests/ clean
