#include "cmap.hpp"

#include "test_runner.h"
#include "profile.h"

using uri = std::string;
using cmap_fold = unordered_map<uri, class ConcurrentMap<int, int>>;

void TestSimple()
{
  cmap_fold testMap;
  testMap.insert({"one", ConcurrentMap<int, int>(1)});

  ASSERT_EQUAL(1, testMap.size());

  testMap.at("one")[1].ref_to_value = 1;

  ASSERT_EQUAL(1, testMap.at("one").At(1).ref_to_value);
}

void RunConcurrentUpdates(
    cmap_fold& cm, size_t thread_count, int key_count
) {
  auto kernel = [&cm, key_count](int seed)
  {
    vector<int> updates(key_count);
    iota(begin(updates), end(updates), -key_count / 2);
    shuffle(begin(updates), end(updates), default_random_engine(seed));
    stringstream ss;

    for (int j = 0; j < 1000; j++)
    {
      for (int i = 0; i < 2; ++i)
      {
        for (auto key : updates)
        {
          cm.at(std::to_string(j))[key].ref_to_value++;
        }
      }
    }

    ss << "I am thread: " << this_thread::get_id() << "\n";
    cout << ss.str() << std::endl;
    
  };

  vector<future<void>> futures;
  for (size_t i = 0; i < thread_count; ++i) {
    futures.push_back(async(std::launch::async, kernel, i));
  }
}

void TestAsync()
{
  const size_t thread_count = 3;
  const size_t key_count = 50000;
  const size_t experiments = 1000;

  cmap_fold cm;
  for (int i = 0; i < experiments; i++)
  {
    cm.insert({std::to_string(i), ConcurrentMap<int, int>(thread_count)});
  }
  
  RunConcurrentUpdates(cm, thread_count, key_count);

  for (int i = 0; i < experiments; i++)
  {
    const auto result = std::as_const(cm.at(std::to_string(i))).BuildOrdinaryMap();
    ASSERT_EQUAL(result.size(), key_count);

    for (auto& [k, v] : result) {
      AssertEqual(v, 6, "Key = " + to_string(k));
    }
  }
}

int main() {
  TestRunner tr;
  RUN_TEST(tr, TestSimple);
  RUN_TEST(tr, TestAsync);
  return 0;
}