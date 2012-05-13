#include "gtest.h"
#include "src/srp-common.h"
#include "src/buffer.h"
#include "src/srp-client.h"
#include "src/srp-server.h"

TEST(SecurePrimes, VerifyAll) {
  SecurePrimes primes;
  char buffer[100];
  for (int i = 0; primes.ValidIndex(i); i++) {
    const BigNumber& prime = primes.GetPrime(i);
    const BigNumber& generator = primes.GetGenerator(i);
    const SecurePrimes::Prime& descriptor = primes.GetDescriptor(i);

    string hex;
    prime.ExportAsHex(&hex);
    EXPECT_EQ(descriptor.value, hex);

    // FIXME: this is horrible. I need some safe replacement for snprintf.
    snprintf(buffer, sizeof_array(buffer), "%d", descriptor.generator);
    string ascii;
    generator.ExportAsAscii(&ascii);
    EXPECT_EQ(buffer, ascii);
  }
}

// How does the key size affect performance?
// The numbers below are from my laptop. Note
// that this test runs both the client code and
// server code serially. Authentication time is
// approximately close to ~half the times here,
// + network latency.
//   
// Key#  Run time   Prime size        Key size
//   0      24 ms    1024 bits        128 bits
//   1      33 ms    1536 bits        192 bits
//   2      49 ms    2048 bits        256 bits
//   3      83 ms    3072 bits        384 bits
//   4     139 ms    4096 bits        512 bits
//   5     257 ms    6144 bits        768 bits
//   6     433 ms    8192 bits       1024 bits

TEST(SrpInteractions, VerifyHandshake) {
  Buffer buffer;
  BigNumberContext bnctx;
  DefaultPrng prng;

  SrpClientSession srp_client(&prng, &bnctx, "foouser");
  SrpServerSession srp_server(&prng, &bnctx);

  srp_client.FillClientHello(buffer.Input());
  LOG_DEBUG("client hello size: %d", buffer.Output()->LeftSize());

  string username;
  EXPECT_TRUE(srp_server.ParseClientHello(buffer.Output(), &username));
  EXPECT_EQ("foouser", username);

  // Build a valid secret for the user.
  SecurePrimes primes;
  ScopedPassword password(STRBUFFER("this-is-his-password"));
  SrpSecret secret(primes, 6);
  EXPECT_TRUE(secret.FromPassword("foouser", password));
  string encodedpassword;
  secret.ToSecret(&encodedpassword);

  EXPECT_TRUE(srp_server.InitSession("foouser", encodedpassword));
  srp_server.FillServerHello(buffer.Input());
  LOG_DEBUG("server hello size: %d", buffer.Output()->LeftSize());

  EXPECT_EQ(0, srp_client.ParseServerHello(buffer.Output()));
  EXPECT_TRUE(srp_client.FillClientPublicKey(buffer.Input()));
  LOG_DEBUG("client public key: %d", buffer.Output()->LeftSize());

  EXPECT_TRUE(srp_server.ParseClientPublicKey(buffer.Output()));
  EXPECT_TRUE(srp_server.FillServerPublicKey(buffer.Input()));
  LOG_DEBUG("server public key: %d", buffer.Output()->LeftSize());

  EXPECT_EQ(0, srp_client.ParseServerPublicKey(buffer.Output()));

  ScopedPassword server_private_key, client_private_key;
  EXPECT_TRUE(srp_server.GetPrivateKey(&server_private_key));
  EXPECT_TRUE(srp_client.GetPrivateKey(password, &client_private_key));

  LOG_DEBUG("server private key: %d", server_private_key.Used());
  LOG_DEBUG("client private key: %d", client_private_key.Used());

  EXPECT_TRUE(server_private_key.SameAs(client_private_key));
}
