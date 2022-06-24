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

template <typename SampleType>
class AsyncFileSink : public FileSink<SampleType> {
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

  virtual size_t write(SampleType* buffer, size_t nSamples) override {
    std::future<long unsigned int> fut = std::async(std::launch::async, &FileSink<SampleType>::write, this, buffer, nSamples);
    aiolist.push_back(std::move(fut));
    return nSamples; // async call nothing to return now
  }

private:
  std::vector<std::future<long unsigned int>> aiolist;
};
