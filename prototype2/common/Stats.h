/** Copyright (C) 2016 European Spallation Source ERIC */

/** @file
 *
 *  @brief runtime statistics
 */

#pragma once

#include <algorithm>
#include <cinttypes>
#include <libs/include/Timer.h>

class Stats {
private:
  Timer usecs_elapsed, time;   /**< used for rate calculations */
  unsigned int report_mask{0}; /**< bitmask for active statistics */

  /** @brief samples the current counter values */
  void sample() {
    ib = i;
    pb = p;
    ob = o;
    usecs_elapsed.now();
  }

  /** @brief print out packet related statistics */
  void packet_stats() {
    auto usecs = usecs_elapsed.timeus();
    uint64_t ipps = (i.rx_packets - ib.rx_packets) * 1000000 / usecs;
    uint64_t iMbps = 8 * (i.rx_bytes - ib.rx_bytes) / usecs;
    uint64_t pkeps = (p.rx_events - pb.rx_events) * 1000 / usecs;
    uint64_t oMbps = 8 * (o.tx_bytes - ob.tx_bytes) / usecs;

    printf(" | I - %12" PRIu64 " B, %8" PRIu64 " pkt/s, %5" PRIu64 " Mb/s"
           " | P - %5" PRIu64 " kev/s"
           " | O - %5" PRIu64 " Mb/s",
           i.rx_bytes, ipps, iMbps, pkeps, oMbps);
  }

  /** @brief print out event related statistics */
  void event_stats() {
    auto usecs = usecs_elapsed.timeus();
    uint64_t pkeps = (p.rx_events - pb.rx_events) * 1000 / usecs;

    printf(" | I - %12" PRIu64 " pkts"
           " | P - %12" PRIu64 " events, %8" PRIu64 " kev/s, %12" PRIu64
           " discards, %12" PRIu64 " pix_err, %12" PRIu64 " errors",
           i.rx_packets, p.rx_events, pkeps, p.rx_discards, p.geometry_errors, p.rx_error_bytes);
  }

  /** @brief print out fifo related statistics */
  void fifo_stats() {
    printf(" | Fifo I - %6" PRIu64 " free, %10" PRIu64 " pusherr"
           " | P - %12" PRIu64 " fifo free, %10" PRIu64 " pusherr",
           i.fifo_free, i.fifo_push_errors, p.fifo_free, p.fifo_push_errors);
  }

public:
  /** @brief Constructor starts by clear()'ing all counters */
  Stats() { clear(); }

  /** @brief clear counters and also sample() */
  void clear() {
    std::fill_n((char *)&i, sizeof(i), 0);
    std::fill_n((char *)&p, sizeof(p), 0);
    std::fill_n((char *)&o, sizeof(o), 0);
    sample();
  }

  /** @brief set the mask determining what output is printed
   * @param mask bit mask
   */
  void set_mask(unsigned int mask) { report_mask = mask; }

  /** @brief print out all active statistics */
  void report() {
    if (report_mask)
      printf("%5" PRIu64, time.timeus() / 1000000);

    if (report_mask & 0x01)
      packet_stats();

    if (report_mask & 0x02)
      event_stats();

    if (report_mask & 0x04)
      fifo_stats();

    if (report_mask)
      printf("\n");
    sample();
  }

  typedef struct {
    // Counters
    uint64_t rx_packets;
    uint64_t rx_bytes;
    uint64_t fifo_push_errors;
    uint64_t fifo_free;
  } input_stat_t;

  typedef struct {
    // Counters
    uint64_t rx_events;
    uint64_t rx_error_bytes;
    uint64_t rx_discards;
    uint64_t rx_idle;
    uint64_t geometry_errors;
    uint64_t fifo_push_errors;
    uint64_t fifo_free;
  } processing_stat_t;

  typedef struct {
    // Counters
    uint64_t rx_events;
    uint64_t rx_idle;
    uint64_t tx_bytes;
  } output_stat_t;

  input_stat_t i, ib;
  processing_stat_t p, pb;
  output_stat_t o, ob;
};