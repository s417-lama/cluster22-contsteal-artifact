#pragma once
#ifndef DNPPATTERN_HPP_
#define DNPPATTERN_HPP_

#include <cstdlib>
#include <cstdint>
#include <unistd.h>

#include "uth.h"

class dnppattern;

template <class T>
struct dnpretbase { typedef std::tuple<dnppattern, T> type; };
template <>
struct dnpretbase<void> { typedef dnppattern type; };

template <class T>
using dnpret = typename dnpretbase<T>::type;

class dnppattern {
#ifndef DNPPATTERN_SERIAL
#define DNPPATTERN_SERIAL 0
#endif

  inline uint64_t get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec;
  }

public:
  uint64_t work;
  uint64_t span;
  uint64_t n_tasks;

  dnppattern() : work(0), span(0), n_tasks(0) {}

  template <class T, class F>
  T do_serial(F&& f) {
    uint64_t t0 = get_time();
    T ret = f();
    uint64_t t1 = get_time();

    work += t1 - t0;
    span += t1 - t0;
    n_tasks += 1;

    return ret;
  }

  template <class F>
  void do_serial(F&& f) {
    uint64_t t0 = get_time();
    f();
    uint64_t t1 = get_time();

    work += t1 - t0;
    span += t1 - t0;
    n_tasks += 1;
  }

  template <class T, class F>
  T invoke(F&& f) {
    auto [d, r] = f();

    work += d.work;
    span += d.span;
    n_tasks += d.n_tasks;

    return r;
  }

  template <class F>
  void invoke(F&& f) {
    auto d = f();

    work += d.work;
    span += d.span;
    n_tasks += d.n_tasks;
  }

  dnpret<void> ret() {
    return *this;
  }

  template <class T>
  dnpret<T> ret(T r) {
    return std::make_tuple(*this, r);
  }

  template <class F0, class F1>
  void parallel_invoke(F0&& f0, F1&& f1) {
#if DNPPATTERN_SERIAL
    auto d0 = f0();
    auto d1 = f1();
#else
    madm::uth::thread<dnppattern> th0(f0);
    auto d1 = f1();
    auto d0 = th0.join();
#endif

    work += d0.work + d1.work;
    span += std::max(d0.span, d1.span);
    n_tasks += d0.n_tasks + d1.n_tasks;
  }

  template <class F0, class F1, class T, class Reduce>
  T parallel_invoke_reduce(F0&& f0, F1&& f1, const T& init, Reduce&& reduce) {
#if DNPPATTERN_SERIAL
    auto [d1, r1] = f0();
    auto [d0, r0] = f1();
#else
    madm::uth::thread< dnpret<T> > th0(f0);
    auto [d1, r1] = f1();
    auto [d0, r0] = th0.join();
#endif

    work += d0.work + d1.work;
    span += std::max(d0.span, d1.span);
    n_tasks += d0.n_tasks + d1.n_tasks;

    return reduce(reduce(init, r0), r1);
  }

  template <class Iterator, class F>
  void parallel_for(Iterator first, Iterator last, F&& f) {
    if (last - first == 1) {
      invoke([=] { return f(first); });
    } else {
      auto mid = first + (last - first) / 2;
      parallel_invoke(
        [=] {
          dnppattern d;
          d.parallel_for(first, mid, f);
          return d;
        },
        [=] {
          dnppattern d;
          d.parallel_for(mid, last, f);
          return d;
        }
      );
    }
  }

  template <class Iterator, class T, class F, class Reduce>
  T parallel_reduce(Iterator first, Iterator last, const T& init,
                    F&& f, Reduce&& reduce) {
    if (last - first == 1) {
      return invoke<T>([=] { return f(first); });
    } else {
      auto mid = first + (last - first) / 2;
      return parallel_invoke_reduce(
        [=] {
          dnppattern d;
          T r = d.parallel_reduce(first, mid, init, f, reduce);
          return d.ret(r);
        },
        [=] {
          dnppattern d;
          T r = d.parallel_reduce(mid, last, init, f, reduce);
          return d.ret(r);
        },
        init, reduce
      );
    }
  }
};

#endif
