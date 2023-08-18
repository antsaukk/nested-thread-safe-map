#pragma once

#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <utility>
#include <algorithm>
#include <random>
using namespace std;

namespace cmap_one2one 
{

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

  explicit ConcurrentMap(size_t bucket_count) :
  buckets_(bucket_count),
  map_collection_(bucket_count),
  mutexes_(bucket_count)
  {}

  WriteAccess operator[](const K& key)
  {
    size_t index = hasher_(key) % buckets_;
    return WriteAccess(key, mutexes_[index], map_collection_[index]);
  }

  ReadAccess At(const K& key) const
  {
    size_t index = hasher_(key) % buckets_;
    return ReadAccess(key, mutexes_[index], map_collection_[index]);
  }

  bool Has(const K& key) const
  {
    size_t index = hasher_(key) % buckets_;
    return ValuePresence(key, mutexes_[index], map_collection_[index]).presence;
  }

  MapType BuildOrdinaryMap() const
  {
    MapType result;
    for(size_t i = 0; i < buckets_; i++){
      lock_guard<mutex> lock_guard(mutexes_[i]);
      result.insert(map_collection_[i].begin(), map_collection_[i].end());
    }
    return result;
  }

private:
  Hash hasher_;

  size_t buckets_;
  vector<MapType> map_collection_;
  mutable vector<mutex> mutexes_;
};

}