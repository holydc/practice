#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

/**
 * Preprocessing for the bad character heuristics.
 */
std::vector<int> buildDelta1(const std::string &pattern) {
  std::vector<int> delta1(256, -1);
  for (size_t i = 0; i < pattern.size(); ++i) {
    delta1[pattern[i]] = static_cast<int>(i);
  }
  return delta1;
}

/**
 * Preprocessing for the good suffix heuristics.
 */
std::vector<int> buildDelta2(const std::string &pattern) {
  const int m = static_cast<int>(pattern.size());
  std::vector<int> delta2(m + 1, 0), bpos(m + 1);

  // Case 1: The matching suffix occurs somewhere else in the pattern.
  bpos[m] = m + 1;
  for (int i = m, j = m + 1; i > 0;) {
    // If character at position i-1 is not equivalent to character at j-1,
    // then continue searching to right of the pattern for border.
    while ((j <= m) && (pattern[i - 1] != pattern[j - 1])) {
      // The character preceding the occurence of t in pattern P is different
      // than mismatching character in P, we stop skipping the occurences and
      // shift the pattern.
      if (delta2[j] == 0) {
        delta2[j] = j - i;
      }

      // Update the position of next border.
      j = bpos[j];
    }

    // p[i-1] matched with p[j-1], border is found.
    // Store the beginning position of border.
    --i;
    --j;
    bpos[i] = j;
  }

  // Case 2: Only a part of the matching suffix occurs at the beginning of the pattern.
  for (int i = 0, j = bpos[0]; i <= m; ++i) {
    // Set the border postion of first character of pattern to all indices
    // in array shift having delta2[i] = 0.
    if (delta2[i] == 0) {
      delta2[i] = j;
    }

    // Suffix become shorter than bpos[0], use the position of next widest
    // border as value of j.
    if (i == j) {
      j = bpos[j];
    }
  }

  return delta2;
}

int search(const std::string &text, const std::string &pattern, int start = 0) {
  auto delta1 = buildDelta1(pattern);
  auto delta2 = buildDelta2(pattern);
  const int n = static_cast<int>(text.size()), m = static_cast<int>(pattern.size());
  const int d = n - m;
  while (start < d) {
    int j = m - 1;

    // Keep reducing index j of pattern while characters of pattern and text
    // are matching at this shift s.
    while ((j >= 0) && (pattern[j] == text[start + j])) {
      --j;
    }

    // If the pattern is present at current shift, then index j will become -1
    // after the above loop.
    if (j < 0) {
      return start;
    }

    // Shift the pattern.
    start += std::max(j - delta1[text[start + j]], delta2[j + 1]);
  }
  return -1;
}

int main() {
  const std::string text("BABAAAABAACD");
  const std::string pattern("ABA");
  for (int i = 0;;) {
    i = search(text, pattern, i);
    if (i == -1) {
      break;
    }
    std::cout << i << std::endl;
    i += static_cast<int>(pattern.size());
  }
  return 0;
}
