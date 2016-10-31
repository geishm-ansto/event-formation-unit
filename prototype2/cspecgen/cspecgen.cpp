/** Copyright (C) 2016 European Spallation Source ERIC */

#include <cspec/CSPECData.h>
#include <cspecgen/CspecArgs.h>
#include <cstring>
#include <inttypes.h>
#include <iostream>
#include <libs/include/Socket.h>
#include <libs/include/TSCTimer.h>
#include <libs/include/Timer.h>
#include <unistd.h>

using namespace std;

const int TSC_MHZ = 2900;

int main(int argc, char *argv[]) {
  DGArgs opts(argc, argv); // Parse command line opts

  char buffer[9000];
  unsigned int seqno = 1;

  const int B1M = 1000000;
  Socket::Endpoint local("0.0.0.0", 0);
  Socket::Endpoint remote(opts.dest_ip.c_str(), opts.port);

  UDPClient DataSource(local, remote);
  DataSource.buflen(opts.buflen);
  DataSource.setbuffers(opts.sndbuf, 0);
  DataSource.printbuffers();

  CSPECData cspec;
  int ndata = 100;
  int size = cspec.generate(buffer, 9000, ndata);
  assert(size == ndata * cspec.datasize);

  uint64_t tx_total = 0;
  uint64_t tx = 0;
  uint64_t txp = 0;

  TSCTimer report_timer, throttle_timer;
  Timer us_clock;
  for (;;) {
    if (unlikely((tx_total + tx) >=
                 (long unsigned int)opts.txGB * 1000000000)) {
      cout << "Sent " << tx_total + tx << " bytes." << endl;
      cout << "done" << endl;
      exit(0);
    }

    // Sleep to throttle down speed
    if (unlikely(throttle_timer.timetsc() >= TSC_MHZ * 10000)) {
      usleep(opts.speed_level * 1000);
      throttle_timer.now();
    }

    // Send data
    int wrsize = DataSource.send(buffer, size);
    if (wrsize > 0) {
      tx += wrsize;
      txp++;
      seqno++;
    } else {
      cout << "unable to send" << endl;
    }

    if (unlikely((report_timer.timetsc() / TSC_MHZ) >= opts.updint * 1000000)) {
      auto usecs = us_clock.timeus();
      tx_total += tx;
      printf("Tx rate: %8.2f Mbps, tx %5" PRIu64 " MB (total: %7" PRIu64
             " MB) %ld usecs\n",
             tx * 8.0 / usecs, tx / B1M, tx_total / B1M, usecs);
      tx = 0;
      us_clock.now();
      report_timer.now();
    }
  }
}
