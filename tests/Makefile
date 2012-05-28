CXX = g++-4.4
BASE = ..
SRC = $(BASE)/src
CPPFLAGS = -iquote . -iquote $(BASE) -Wall -std=c++0x -ggdb3 -O0 -DPRINT_DEBUG 
# Note: password uses openssl pbkdf functions, which allocate memory and
# free memory in a tight loop, which ... drives duma to run out of memory.
# TODO: enable duma for all targets but not password.
# TODO: is there any parameter for duma we can use to get rid of the
# expensive checks for password?
LDFLAGS = -lssl -lcrypto -lm -lstdc++ -rdynamic -ggdb3 # -lduma
GTEST = gtest-main.o gtest.o
COMMON = $(SRC)/backtrace.o

TARGETS = $(patsubst %.cc,%,$(wildcard test-*.cc))
ALLSOURCES = *.cc

all: dependencies $(TARGETS)

dependencies: Makefile
	fastdep -I ../ --extraremakedep=Makefile --remakedeptarget=dependencies $(ALLSOURCES) > dependencies

test: $(TARGETS)
	set -e; for file in $(TARGETS); do echo "* TESTING $$file"; ./$$file; done;

test-scramble-session-protector: $(GTEST) $(COMMON) test-scramble-session-protector.o $(SRC)/prng.o $(SRC)/scramble-session-protector.o $(SRC)/openssl-protector.o
test-aes-session-protector: $(GTEST) $(COMMON) test-aes-session-protector.o $(SRC)/prng.o $(SRC)/aes-session-protector.o $(SRC)/openssl-protector.o $(SRC)/password.o $(SRC)/openssl-helpers.o
test-buffer: $(GTEST) $(COMMON) test-buffer.o
test-prng: $(GTEST) $(COMMON) test-prng.o $(SRC)/prng.o $(SRC)/prng.o
test-password: $(GTEST) $(COMMON) test-password.o $(SRC)/password.o $(SRC)/prng.o $(SRC)/openssl-helpers.o $(SRC)/password.o
test-serializers: $(GTEST) $(COMMON) test-serializers.o
test-sockaddr: $(GTEST) $(COMMON) test-sockaddr.o $(SRC)/sockaddr.o $(SRC)/sockaddr.o
test-openssl-helpers: $(GTEST) $(COMMON) test-openssl-helpers.o $(SRC)/openssl-helpers.o $(SRC)/openssl-helpers.o
test-base64: $(GTEST) $(COMMON) test-base64.o $(SRC)/base64.o $(SRC)/base64.o
test-userdb: $(GTEST) $(COMMON) test-userdb.o $(SRC)/userdb.o $(SRC)/userdb.o $(SRC)/base64.o
test-srp-common: $(GTEST) $(COMMON) test-srp-common.o $(SRC)/srp-common.o $(SRC)/openssl-helpers.o $(SRC)/srp-client.o $(SRC)/srp-server.o $(SRC)/srp-passwd.o $(SRC)/prng.o $(SRC)/base64.o $(SRC)/srp-server.o $(SRC)/srp-client.o
test-netlink-interfaces: $(GTEST) $(COMMON) test-netlink-interfaces.o $(SRC)/linux/netlink-interfaces.o $(SRC)/linux/netlink-interfaces.o $(SRC)/ip-addresses.o
test-ip-addresses: $(GTEST) $(COMMON) test-ip-addresses.o $(SRC)/ip-addresses.o
test-ip-manager: $(GTEST) $(COMMON) test-ip-manager.o $(SRC)/ip-addresses.o $(SRC)/ip-manager.o $(SRC)/linux/netlink-interfaces.o
test-connection-key: $(GTEST) $(COMMON) test-connection-key.o

$(SRC)/%.o:
	@$(MAKE) --no-print-directory -C $(SRC) $*.o

clean:
	rm -f $(TARGETS)
	rm -f ./*.o
	$(MAKE) -C $(SRC) clean
