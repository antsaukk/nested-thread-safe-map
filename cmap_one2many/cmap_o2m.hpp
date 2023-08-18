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
using namespace std;

namespace cmap_o2m
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

  struct WriteAccess {
    WriteAccess(const K& key, mutex& m, MapType& mp) :
    guard(m),
    ref_to_value(mp[key])
    {}

    lock_guard<mutex> guard;
    V& ref_to_value;
  };

  struct ReadAccess {
    ReadAccess(const K& key, mutex& m, const MapType& mp) :
    guard(m),
    ref_to_value(mp.at(key))
    {}

    lock_guard<mutex> guard;
    const V& ref_to_value;
  };

  struct ValuePresence {
    ValuePresence(const K& key, mutex& m, const MapType& mp) :
    guard(m),
    presence(mp.count(key))
    {}

    lock_guard<mutex> guard;
    const bool presence;
  };

  explicit ConcurrentMap(
    size_t bucket_count,
    size_t mutex_number,
    bool log_flag = true
  ) :
  buckets_(bucket_count),
  map_collection_(bucket_count),
  mutexes_(mutex_number),
  log_(log_flag)
  {}

  WriteAccess operator[](const K& key)
  {
    size_t index = hasher_(key) % buckets_;

    // LOGGER
    if (log_)
      logMutexMapId(index, ComputeIndexOfMutex(index));

    return WriteAccess(
      key,
      mutexes_[ComputeIndexOfMutex(index)],
      map_collection_[index]
    );
  }

  ReadAccess At(const K& key) const
  {
    size_t index = hasher_(key) % buckets_;

    // LOGGER
    if (log_)
      logMutexMapId(index, ComputeIndexOfMutex(index));

    return ReadAccess(
      key,
      mutexes_[ComputeIndexOfMutex(index)],
      map_collection_[index]
    );
  }

  bool Has(const K& key) const
  {
    size_t index = hasher_(key) % buckets_;

    // LOGGER
    if (log_)
      logMutexMapId(index, ComputeIndexOfMutex(index));

    return ValuePresence(
      key,
      mutexes_[ComputeIndexOfMutex(index)],
      map_collection_[index]
    ).presence;
  }

  MapType BuildOrdinaryMap() const
  {
    MapType result;
    for(size_t i = 0; i < buckets_; i++){
      lock_guard<mutex> lock_guard(mutexes_[ComputeIndexOfMutex(i)]);
      result.insert(map_collection_[i].begin(), map_collection_[i].end());
    }
    return result;
  }

private:
  Hash hasher_;

  size_t buckets_;
  vector<MapType> map_collection_;
  mutable vector<mutex> mutexes_;

  bool log_;

  const size_t ComputeIndexOfMutex(size_t indexOfMap) const
  {
    return indexOfMap + 1 <= mutexes_.size() ? indexOfMap : (indexOfMap + 1) % mutexes_.size() - 1;
  }
};
}