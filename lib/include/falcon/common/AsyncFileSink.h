#pragma once

#include "FileSink.h"
#include <cstring>
#include <atomic>
#include <aio.h>
#include <stdlib.h> 
#include <cassert>
#include <math.h>
#include <future>
#include <vector>
#include <functional>
#include <mutex>

struct aiocb asyncRequest(int fd,  
                            off_t offset, 
                            volatile void * content, 
                            size_t length){
    // create and initialize the aiocb structure.
    // If we don't init to 0, we have undefined behavior.
    // E.g. through sigevent op.aio_sigevent there could be 
    //      a callback function being set, that the program
    //      tries to call - which will then fail.
    struct aiocb ret = {0};
    {
        ret.aio_fildes = fd;
        ret.aio_offset = offset;
        ret.aio_buf = content;
        ret.aio_nbytes = length;            
    }
    return ret;
}

template <typename SampleType>
class AsyncFileSink : public FileSink<SampleType> {
public:
  AsyncFileSink(): FileSink<SampleType>() {}
  AsyncFileSink(const AsyncFileSink&) = delete; //prevent copy
  AsyncFileSink& operator=(const AsyncFileSink&) = delete; //prevent copy
  virtual ~AsyncFileSink() {
    for (auto& obj : aiolist) {
      obj.get();
      // delete[] obj.second;
    }
    aiolist.clear();
  }

  virtual size_t write(SampleType* buffer, size_t nSamples) override {
    SampleType* async_buffer = new SampleType[nSamples];
    memcpy(async_buffer, buffer, sizeof(SampleType) * nSamples);
    auto run_io = [this, &async_buffer, &nSamples]() {
      FileSink<SampleType>::write(async_buffer, nSamples);
      delete[] async_buffer;
    };
    std::future<void> fut = std::async(std::launch::async, run_io);
    aiolist.push_back(std::move(fut));
    return nSamples; // async call nothing to return now
  }

private:
  std::vector<std::future<void>> aiolist;
  // std::vector<SampleType*> bufferlist;
};
