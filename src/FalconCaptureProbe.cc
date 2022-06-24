/*
 * Copyright (c) 2019 Robert Falkenberg.
 *
 * This file is part of FALCON 
 * (see https://github.com/falkenber9/falcon).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 */
#include "capture_probe/ArgManager.h"
#include "capture_probe/CaptureProbeCore.h"

#include "falcon/meas/AuxModem.h"
#include "falcon/meas/AuxModemGPS.h"
#include "falcon/meas/TrafficGenerator.h"
#include "falcon/meas/TrafficResultsToFile.h"
#include "falcon/meas/NetsyncSlave.h"
#include "falcon/common/BufferedFileSink.h"
#include "falcon/common/AsyncFileSink.h"
#include "falcon/common/SignalManager.h"

#include "falcon/common/SystemInfo.h"
#include "falcon/common/Version.h"

#include <iostream>
#include <memory>
#include <cstdlib>
#include <unistd.h>

using namespace std;

int main(int argc, char** argv) {
  cout << "FalconCaptureProbe, version " << Version::gitVersion() << endl;
  cout << "Copyright (C) 2020 Robert Falkenberg" << endl;
  cout << endl;

  Args args;
  ArgManager::parseArgs(args, argc, argv);

  NetsyncSlave netsync(args.port);
  //netsync.attachExtraStopFlag(&go_exit);
  netsync.launchReceiver();

  SierraWirelessAuxModem swModem;
  AuxModemGPS gps;

  DummyAuxModem dummyModem;
  DummyGPS gpsDummy;

  AuxModem* modem = &dummyModem;
  GPS* gpsRef = &gpsDummy;

  if(!swModem.init()) {
    cout << "Could not initialize SierraWirelessAuxModem, continue without AuxModem" << endl;
    args.no_auxmodem = true;
  }
  else if(!swModem.configure()) {
    cout << "Could not configure SierraWirelessAuxModem, continue without AuxModem" << endl;
    args.no_auxmodem = true;
  }
  else {
    // use the SierraWirelessAuxModem as modem instead of dummy
    modem = &swModem;
  }

  if(!gps.init(&swModem)) {
    cout << "Warning: Could not initialize GPS from SierraWirelessAuxModem - continue without GPS" << endl;
  }
  else {
    // use the GPS from SierraWirelessAuxModem instead of dummy
    gpsRef = &gps;
  }

  netsync.init(modem);

  TrafficGenerator trafficGenerator;

// #define USE_BUFFERED_SINK
#define USE_AYNC_SINK
#ifdef USE_BUFFERED_SINK
  BufferedFileSink<cf_t> sink;
  size_t sz = SystemInfo::getAvailableRam() - 512*1024*1024;  //allocate all RAM but 512MB
  cout << "Allocating memory sample buffer of " << sz << " B..." << endl;
  sink.allocate(sz);
#endif
#ifdef USE_AYNC_SINK
  AsyncFileSink<cf_t> sink;
  cout << "Allocating async call for file write" << endl;
#else
  FileSink<cf_t> sink;
#endif

  //attach signal handlers (for CTRL+C)
  SignalGate& signalGate(SignalGate::getInstance());
  signalGate.init();
  signalGate.attach(netsync);

  bool runSuccess = false;
  int nof_runs = 1;
  do {
    CaptureProbeCore core(args);
    core.init(&netsync, modem, &trafficGenerator, &sink, gpsRef);
    signalGate.attach(core);
    runSuccess = core.run();
    signalGate.detach(core);
    if(args.repeat_pause > 0) {
      sleep(args.repeat_pause);
    }
  } while(runSuccess && (args.nof_runs != nof_runs++));

  cout << "Exit" << endl;
  return runSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
}
