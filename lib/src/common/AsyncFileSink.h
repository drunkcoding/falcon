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

struct aiocb createIoRequest(int fd,  
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
private:
  SampleType* mem;
  size_t memSize;
  size_t sfn;
  std::string filename;
public:
  AsyncFileSink(): FileSink<SampleType>() {}
  AsyncFileSink(const AsyncFileSink&) = delete; //prevent copy
  AsyncFileSink& operator=(const AsyncFileSink&) = delete; //prevent copy
  virtual ~AsyncFileSink() {
    for (auto& fut : aiolist) {
      fut.get();
    }
    aiolist.clear();
  }

  // virtual void open(const std::string& filename) override {
  //   char tmp[1024];  /* WTF! srslte_filesink_init takes char*, not const char* ! */
  //   strncpy(tmp, filename.c_str(), 1024);
  //   srslte_filesink_init(&filesink, tmp, type);
  //   isOpen = true;
  //   assert(aiolist == nullptr);
  //   aiolist = malloc(2*sizeof(aiocb));
  // }

  // virtual void close() override {
  //   if(isOpen) {
  //     srslte_filesink_free(&filesink);
  //     isOpen = false;
  //     aio_suspend(aiolist, sfn, NULL);
  //     if aiolist != nullptr {
  //       free(aiolist);
  //       aiolist = nullptr;
  //     }
  //   }
  //   sfn = 0;
  // }

  virtual size_t write(SampleType* buffer, size_t nSamples) override {
    std::future<long unsigned int> fut = std::async(std::launch::async, &FileSink<SampleType>::write, this, buffer, nSamples);
    aiolist.push_back(std::move(fut));
    return 0; // async call nothing to return now
  }

private:
  std::vector<std::future<long unsigned int>> aiolist;
};
