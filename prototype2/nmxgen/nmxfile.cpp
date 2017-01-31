/** Copyright (C) 2016 European Spallation Source ERIC */

#include <cassert>
#include <nmxgen/NMXArgs.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <libs/include/Socket.h>
#include <unistd.h>
#include <nmxgen/ReaderVMM.h>

// const int TSC_MHZ = 2900;




int main(int argc, char *argv[])
{
  NMXArgs opts(argc, argv);

  char buffer[9000];

  Socket::Endpoint local("0.0.0.0", 0);
  Socket::Endpoint remote(opts.dest_ip.c_str(), opts.port);

  UDPClient DataSource(local, remote);
  DataSource.buflen(opts.buflen);
  DataSource.setbuffers(opts.sndbuf, 0);
  DataSource.printbuffers();

  ReaderVMM file(opts.filename);

  int readsz;

  uint64_t pkt = 0;
  uint64_t bytes = 0;

  while ((pkt < opts.txPkt) && ((readsz = file.read(buffer)) > 0))
  {
    DataSource.send(buffer, readsz);
    bytes += readsz;
    pkt++;

    usleep(opts.speed_level * 1000);
  }

  printf("Sent: %" PRIu64 " packets\n", pkt);
  printf("Sent: %" PRIu64 " bytes\n", bytes);

  return 0;
}
