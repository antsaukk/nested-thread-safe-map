#pragma once


#include <sstream>
#include <iostream>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <utility>
#include <algorithm>
#include <random>
#include <atomic>
using namespace std;

namespace cmap_dyn
{

void logMutexMapId(size_t mapId, size_t mutexId)
{
  stringstream ss;
  ss << "Hello, I am mutex " << mutexId << " and I am handling map" << mapId << "\n";
  cout << ss.str();
}

template <typename K, typename V, typename Hash = std::hash<K>>
class ConcurrentMap {
public:
  using MapType = unordered_map<K, V, Hash>;

private:
  struct WriteAccess {
    WriteAccess(
      const K& key,
      mutex& m,
      MapType& mp,
      atomic<int>& map_guard,
      atomic<int>& mutex_guard) :
    guard(m),
    ref_to_value(mp[key]),
    map_guard_(map_guard),
    mutex_guard_(mutex_guard)
    {}

    ~WriteAccess() {
      //mutex_guard_.store(0);
      //map_guard_.store(0);
    }

    lock_guard<mutex> guard;
    V& ref_to_value;

    atomic<int>& map_guard_;
    atomic<int>& mutex_guard_;
  };

  struct ReadAccess {
    ReadAccess(
      const K& key,
      mutex& m,
      const MapType& mp,
      atomic<int>& map_guard,
      atomic<int>& mutex_guard) :
    guard(m),
    ref_to_value(mp.at(key)),
    map_guard_(map_guard),
    mutex_guard_(mutex_guard)
    {}

    ~ReadAccess() {
      mutex_guard_.store(0);
      map_guard_.store(0);
    }

    lock_guard<mutex> guard;
    const V& ref_to_value;

    atomic<int>& map_guard_;
    atomic<int>& mutex_guard_;
  };

  struct ValuePresence {
    ValuePresence(
      const K& key,
      mutex& m,
      const MapType& mp,
      atomic<int>& map_guard,
      atomic<int>& mutex_guard) :
    guard(m),
    presence(mp.count(key)),
    map_guard_(map_guard),
    mutex_guard_(mutex_guard)
    {}

    ~ValuePresence() {
      mutex_guard_.store(0);
      map_guard_.store(0);
    }

    lock_guard<mutex> guard;
    const bool presence;

    atomic<int>& map_guard_;
    atomic<int>& mutex_guard_;
  };

public:
  explicit ConcurrentMap(
    size_t bucket_count,
    size_t mutex_number,
    bool log_flag = true
  ) :
  buckets_(bucket_count),
  map_collection_(bucket_count),
  mutexes_(mutex_number),
  mutex_table_(mutex_number),
  map_table_(bucket_count),
  log_(log_flag)
  {
    for (size_t i = 0; i < map_table_.size(); i++)
    {
      map_table_[i].store(0);
    }

    for (size_t i = 0; i < mutex_table_.size(); i++)
    {
      mutex_table_[i].store(0);
    }
  }

  WriteAccess operator[](const K& key)
  {
    // compute index of correct hash map
    size_t index_of_map = hasher_(key) % buckets_;

    // busy-waiting while the required map will be free
    acquireMapLock(index_of_map);

    // locking of first free mutex in interleaving fashion
    const size_t index_of_mutex = acquireFirstFreeMutex();

    // LOGGER
    if (log_) {
      logMutexMapId(index_of_map, index_of_mutex);
    }

    return WriteAccess(
      key,
      mutexes_[index_of_mutex],
      map_collection_[index_of_map],
      map_table_[index_of_map],
      mutex_table_[index_of_mutex]
    );
  }

  ReadAccess At(const K& key) const
  {
    // compute index of correct hash map
    size_t index_of_map = hasher_(key) % buckets_;

    // busy-waiting while the required map will be free
    acquireMapLock(index_of_map);

    // locking of first free mutex in interleaving fashion
    const size_t index_of_mutex = acquireFirstFreeMutex();

    // LOGGER
    if (log_) {
      logMutexMapId(index_of_map, index_of_mutex);
    }

    return ReadAccess(
      key,
      mutexes_[index_of_mutex],
      map_collection_[index_of_map],
      map_table_[index_of_map],
      mutex_table_[index_of_mutex]
    );
  }

  bool Has(const K& key) const
  {
    // compute index of correct hash map
    size_t index_of_map = hasher_(key) % buckets_;

    // busy-waiting while the required map will be free
    acquireMapLock(index_of_map);

    // locking of first free mutex in interleaving fashion
    const size_t index_of_mutex = acquireFirstFreeMutex();

    // LOGGER
    if (log_) {
      logMutexMapId(index_of_map, index_of_mutex);
    }

    return ValuePresence(
      key,
      mutexes_[index_of_mutex],
      map_collection_[index_of_map],
      map_table_[index_of_map],
      mutex_table_[index_of_mutex]
    ).presence;
  }

  MapType BuildOrdinaryMap() const
  {
    MapType result;
    for(size_t i = 0; i < buckets_; i++){

      // busy-waiting while the required map will be free
      acquireMapLock(i);

      // locking of first free mutex in interleaving fashion
      const size_t index_of_mutex = acquireFirstFreeMutex();

      {
        lock_guard<mutex> lock_guard(mutexes_[index_of_mutex]);
        result.insert(map_collection_[i].begin(), map_collection_[i].end());
      }

      mutex_table_[index_of_mutex].store(0);
      map_table_[i].store(0);
    }
    return result;
  }

private:
  Hash hasher_;

  size_t buckets_;
  vector<MapType> map_collection_;

  mutable vector<mutex> mutexes_;
  mutable vector<atomic<int>> mutex_table_;
  mutable vector<atomic<int>> map_table_;

  bool log_;

private:
  void acquireMapLock(size_t index_of_map) const
  {
    int expected = 0;
    int desired = 1;

    while(!map_table_[index_of_map].compare_exchange_weak(expected, desired));
  }

  const size_t acquireFirstFreeMutex() const
  {
    size_t counter = 0;

    int expected = 0;
    int desired = 1;

    while(!mutex_table_[counter].compare_exchange_weak(expected, desired))
    {
      counter++;
      counter %= mutex_table_.size();
    }

    return counter;
  }

};
}