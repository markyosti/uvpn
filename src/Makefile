#### TOOLS
IPCGENERATOR = ../tools/ipc/generator.py
#CXX = llvm-g++-4.2

#### FLAGS
CPPFLAGS = -Wall -pedantic -ggdb3 -O0 -DPRINT_DEBUG -Wformat-nonliteral -Wextra -Wstrict-overflow=5 -Wfloat-equal -Wconversion -Wlogical-op -fstack-protector-all -Wno-unused-parameter -fno-exceptions #-Weffc++ -Wextra
# CPPFLAGS = -Wall -pedantic -O0 -DPRINT_DEBUG
# -Wunreachable-code -> causes lot of warnings due to standard libraries!
# -Wformat-zero-length -> C and objective C only.
# -Wshadow -> arguments like size and friends shadow globals :(
# TRY USING mudflap library!
LDFLAGS = -lssl -lcrypto -lm -lstdc++ -rdynamic -ggdb3 -lduma -lrt

#### INTERNAL LIBRARIES
LIBYAARG = ../lib/yaarg/config-parser-argv.o ../lib/yaarg/config-parser-options.o ../lib/yaarg/config-parser.o

#### SYSTEM DEPENDENCIES
SYSLINUX = ./linux/netlink-interfaces.o ./linux/epoll-dispatcher.o ./linux/clock-timers.o

#### VARIOUS CONSTANTS
SYSTEM = linux

all: $(SYSTEM)

linux: LDFLAGS += $(SYSLINUX)
linux: CPPFLAGS += -DUVPN_SYSTEM=LINUX
linux: $(SYSLINUX) targets

targets: deps uvpn-user uvpn-client uvpn-server uvpn-ip-config uvpn-ctl
	
#-include deps

deps: Makefile
	fastdep --extraremakedep=Makefile --remakedeptarget=deps *.cc > deps

uvpn-ip-config: uvpn-ip-config.o ip-addresses.o backtrace.o

uvpn-user: uvpn-user.o userdb.o base64.o srp-common.o ip-addresses.o srp-passwd.o openssl-helpers.o prng.o terminal.o backtrace.o password.o

uvpn-client: event-scheduler.o uvpn-client-main.o uvpn-client.o scramble-session-protector.o prng.o openssl-helpers.o srp-client-authenticator.o scramble-session-protector.o srp-common.o srp-client.o srp-passwd.o base64.o socket-transport.o tun-tap-common.o tun-tap-client-channel.o packet-queue.o backtrace.o sockaddr.o openssl-protector.o password.o aes-session-protector.o terminal.o ip-addresses.o client-tcp-transcoder.o client-udp-transcoder.o user-chatter.o terminal-user-chatter.o client-simple-connection-manager.o daemon-controller-server.o daemon-controller.o $(LIBYAARG)

uvpn-server: event-scheduler.o uvpn-server-main.o uvpn-server.o scramble-session-protector.o prng.o openssl-helpers.o srp-server-authenticator.o scramble-session-protector.o srp-common.o srp-server.o srp-passwd.o base64.o socket-transport.o tun-tap-common.o tun-tap-server-channel.o packet-queue.o backtrace.o sockaddr.o userdb.o base64.o terminal.o openssl-protector.o password.o aes-session-protector.o ip-addresses.o ip-manager.o server-tcp-transcoder.o server-simple-connection-manager.o server-udp-transcoder.o daemon-controller-server.o daemon-controller.o $(LIBYAARG)

uvpn-ctl: event-scheduler.o daemon-controller.o daemon-controller-client.o sockaddr.o ip-addresses.o ipc-client.o uvpn-ctl-main.o socket-transport.o backtrace.o $(LIBYAARG)

ipc/%-client.h ipc/%-server.h: ipc/%.ipc
	@$(IPCGENERATOR) $<

%.o: %.cc
	@echo "  * COMPILING $<"
	@$(CXX) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

clean:
	rm -f ./*/*.o ./*.o ../lib/yaarg/*.o
	rm -f ./uvpn-user ./uvpn-client ./uvpn-server ./uvpn-ip-config ./uvpn-ctl
	# $(MAKE) -C tests/ clean
