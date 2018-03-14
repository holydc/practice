/**
 * Given m lists of an average of n integers in sorted order with no duplicates, find the top k integers that occur the most frequently. 
 * Example:
 *
 *   [2, 3, 6]
 *   [1, 3, 4, 5, 6, 8]
 *   [1, 3, 5, 6, 9, 10]
 *   [3, 5, 7, 11]
 *
 * Write out answers for several cases of k, like
 *
 *   k = 1 => [3]
 *   k = 3 => [3, 5, 6]
 *   k = 4 => [3, 5, 6, 1]
 */
#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <queue>
#include <vector>

struct ListNode {
  int val;
  ListNode *next;
  ListNode(int x) : val(x), next(nullptr) {
  }
};

/**
 * Overall time complexity: O(m*n*(logm+logk))
 * Overall space complexity: O(m+k), where k is for output and is probably negligible
 */
std::vector<int> kMostFrequent(std::vector<ListNode *> lists, int k) {
  // build heap, O(m*logm)
  std::vector<ListNode *> heap; // O(m)
  for (auto list : lists) {
    heap.push_back(list);
    for (int i = heap.size() - 1; i > 0;) {
      int parent = (i - 1) / 2;
      if (heap[parent]->val <= heap[i]->val) {
        break;
      }
      std::swap(heap[parent], heap[i]);
      i = parent;
    }
  }
  // merge, traverse all m*n nodes
  ListNode dummy(5566);
  int count = 0;
  std::priority_queue<
      std::pair<int, int>,
      std::vector<std::pair<int, int>>,
      std::greater<std::pair<int, int>>> frequency; // O(k)
  for (ListNode *cur = &dummy; !heap.empty(); cur = cur->next) {
    cur->next = heap[0];
    heap[0] = heap[0]->next;
    // adjust heap, O(logm)
    if (heap[0] == nullptr) {
      heap[0] = heap.back();
      heap.pop_back();
    }
    for (int i = 0, n = heap.size();;) {
      int l = (i * 2) + 1, r = (i + 1) * 2;
      if (((l >= n) || (heap[i]->val <= heap[l]->val))
          && ((r >= n) || (heap[i]->val <= heap[r]->val))) {
        break;
      }
      int child = ((r < n) && (heap[r]->val < heap[l]->val)) ? r : l;
      std::swap(heap[child], heap[i]);
      i = child;
    }
    // adjust frequency, O(logk)
    if (heap.empty()) {
      if (cur->val == cur->next->val) {
        frequency.emplace(count + 1, cur->val);
      } else {
        frequency.emplace(count, cur->val);
        frequency.emplace(1, cur->next->val);
      }
    } else if ((cur != &dummy) && (cur->val != cur->next->val)) {
      frequency.emplace(count, cur->val);
      count = 1;
    } else {
      ++count;
    }
    while (frequency.size() > static_cast<size_t>(k)) {
      frequency.pop();
    }
  }
  // output, O(k*logk)
  // this will be negligible if we don't care the order of the output
  std::vector<int> k_most_frequent;
  while (!frequency.empty()) {
    k_most_frequent.push_back(frequency.top().second);
    frequency.pop();
  }
  std::reverse(k_most_frequent.begin(), k_most_frequent.end());
  return k_most_frequent;
}

int main() {
  std::vector<std::vector<ListNode>> lists{
    {2, 3, 6},
    {1, 3, 4, 5, 6, 8},
    {1, 3, 5, 6, 9, 10},
    {3, 5, 7, 11}
  };
  for (auto &list : lists) {
    for (size_t i = 1; i < list.size(); ++i) {
      list[i - 1].next = &list[i];
    }
  }
  auto make_lists = [] (const std::vector<std::vector<ListNode>> &lists) -> std::vector<ListNode *> {
    std::vector<ListNode *> heads;
    for (auto &list : lists) {
      heads.push_back(const_cast<ListNode *>(&list[0]));
    }
    return heads;
  };
  // k = 1 => [3]
  // k = 3 => [3, 5, 6]
  // k = 4 => [3, 5, 6, 1]
  auto ret = kMostFrequent(make_lists(lists), 4);
  std::copy(ret.begin(), ret.end(), std::ostream_iterator<int>(std::cout, ","));
  return 0;
}
