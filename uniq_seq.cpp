#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <stdexcept>

using std::size_t;
typedef std::chrono::high_resolution_clock timer;
typedef std::vector<int> IntVector;

// See next_prime.cpp
size_t __next_prime(size_t n);


// Generate a uniq sequence based on using finite fields
// Inspired from http://preshing.com/20121224/how-to-generate-a-sequence-of-unique-random-integers
// The key point is to find a suitable prime
class UniqueSeq {
  unsigned int m_index;
  unsigned int m_intermediateOffset;
  unsigned int m_prime;
  unsigned int m_range;

  // A prime p must satisty p%4 = 3
  static size_t get_next_suitable_prime(size_t n) {
    size_t prime = n-1;
    do {
      prime = __next_prime(prime+1);
    } while(prime%4 != 3);
    return prime;
  }

  unsigned int permuteQPR(unsigned int x)
  {
    if (x >= m_prime)
      return x; // should only happen if range>4294967291u (see ctor)
    unsigned int residue = ((unsigned long long) x * x) % m_prime;
    return (x <= m_prime / 2) ? residue : m_prime - residue;
  }

public:

  UniqueSeq(unsigned int range, unsigned int seed=0x1) : m_range(range) {
    // Largest prime for 32 bits
    if(range>4294967291u) {
      m_prime = 4294967291u;
    } else {
      m_prime = get_next_suitable_prime(range);
    }
    // Kind of seeds
    m_index = permuteQPR(permuteQPR(seed) + 2*m_prime-m_range);
    m_intermediateOffset = m_prime-m_range;
  }

  // Get the next number in the sequence
  unsigned int next() {
    unsigned int res;
    do {
      res = permuteQPR((permuteQPR(m_index)));
      m_index=(m_index+1)%m_prime;
      // We are guaranteed to get a number lower than m_prime, but m_prime is a
      // little bit bigger than m_range, so sometimes we need to loop
    } while(res>m_range);
    return res;
  }
};



// Simple encapsulation for an integer random generator
template<typename generator_type = std::mt19937>
class Generator {
public:
  typedef std::uniform_int_distribution<int> distribution_type;

  // Gives number between 0 and universeSize
  Generator(int universeSize) : distribution(0, universeSize) {}

  int next() {
    return distribution(generator);
  }

  generator_type generator; // Random generator (template param)
  distribution_type distribution;
};



/* The most naive version
 * call the PRNG and verify in the whole array if the number is already present
 * if it is, then call the PRNG and verify again
 */
IntVector chooseNaive(int count, int universeSize) {
  IntVector res(count);
  Generator<> generator(universeSize); // Add a seed somehow
  int i=0;
  while(i<count) {
    int candidate = generator.next();
    bool not_in_seq = true;
    for(int j=0; j<i; j++) {
      if(res[j] == candidate)
        not_in_seq = false;
    }

    if(not_in_seq) {
      res[i] = candidate;
      i++;
    }
  }
  return res;
}


/* "Improved" version
 * keep a bitfield to track the numbers already returned by the PRNG to avoid
 * going through the whole array each time.
 * It requires more memory so if count is a lot smaller than universeSize it is
 * not interesting.
 */
IntVector chooseBitfield(int count, int universeSize) {
  IntVector res(count);
  std::vector<bool> alreadySeen(universeSize+1,0);
  Generator<> generator(universeSize); // Add a seed somehow
  int i=0;
  while(i<count) {
    int candidate = generator.next();
    bool not_in_seq = !alreadySeen.at(candidate);
    if(not_in_seq) {
      res[i] = candidate;
      alreadySeen[candidate] = true;
      i++;
    }
  }
  return res;
}


/* Best version, not a strong random but enough for most application
 * Benefit from a guaranteed period from the PRNG, no check required!
 */
IntVector chooseSmart(int count, int universeSize) {
  IntVector res(count);
  UniqueSeq generator(universeSize); // Add a seed somehow
  int i=0;
  for(auto &cur : res) {
    cur = generator.next();
  }
  return res;
}


// Check that a sequence is valid (each number is unique)
void check(IntVector seq) {
  auto count = seq.size();
  for(int i=0; i<count; i++) {
    for(int j=i+1; j<count; j++) {
      if(seq[i] == seq[j]) {
        std::cout << "Sequence mismatch: seq[" << i << "] == seq[" << j
                  << "] == " << seq[i] << "\n";
        throw std::exception();
      }
    }
  }

}


/* Test and time the sequence generation
 * Template parameters specifies:
 *
 * - the function called to get the sequence
 * - a boolean that trigger checking the validity of the sequences
 *
 */
template<IntVector (*choose)(int,int),
         bool checkValid=false>
void testRandom(int universeSize, int count_start, int count_end, int count_inc,
                std::string string_version) {

  auto start = timer::now();

  for(int count=count_start;count<count_end;count+=count_inc) {
    // Get a sequence of "count" numbers out of universeSize
    auto seq = choose(count,universeSize);
    if(checkValid)
      check(seq);
  }

  auto end = timer::now();
  auto time = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
  std::cout << "Time for '" << string_version << "' "
            << "(universeSize: " << universeSize << ", "
            << "count range " << count_start << ":"
                              << count_end << ":"
                              << count_inc
            << "): "
            << time.count() << "us\n";

}

/* Run the test for different ranges and for all the versions.
 * The boolean template parameter trigger checking the validity of the sequence
 */
template<bool CheckValid=true>
void runTests() {
  auto runTestsHelper = [](int universeSize, int start, int end, int inc) {
    testRandom<chooseSmart, CheckValid>(universeSize, start, end, inc, "smart");
    testRandom<chooseBitfield, CheckValid>(universeSize, start, end, inc,
                                           "bitfield");
    testRandom<chooseNaive, CheckValid>(universeSize, start, end, inc,"naive");
  };

  IntVector universeSizes = {1000, 10000, 100000};
  for(auto universeSize : universeSizes) {
    std::cout << "\n\nRun for universeSize " << universeSize;
    std::cout << "\n\n - with a relatively small count\n\n";
    runTestsHelper(universeSize,universeSize*0.01,universeSize*.1,
                   universeSize*0.001);

    std::cout << "\n\n - with a relatively medium count\n\n";
    runTestsHelper(universeSize,universeSize*0.4,universeSize*0.6,
                   universeSize*0.1);

    std::cout << "\n\n - with a relatively high count\n\n";
    runTestsHelper(universeSize,universeSize*0.8,universeSize,
                   universeSize*0.1);

    std::cout << "\n\n - with a full range count\n\n";
    runTestsHelper(universeSize,universeSize*0.05,universeSize,
                   universeSize*0.5);
  }

  // Extra test for a huge universeSize, not practicable with the naive version
  int universeSize = 1000000000;
  std::cout << "\n\nRun for a huge universeSize " << universeSize
            << " (naive version is non-practicable here)"
            << "\n\n - with a full range count\n\n";
  testRandom<chooseSmart, CheckValid>(universeSize,universeSize*0.05,
                                      universeSize, universeSize*0.5, "smart");
  testRandom<chooseBitfield, CheckValid>(universeSize,universeSize*0.05,
                                         universeSize, universeSize*0.5,
                                         "bitfield");

}

int main(int argc, char **argv) {
  int universeSize,start,end,inc;
  try {
    // If an argument is given (any), then run the tests with validity check
    if(argc>1) {
      std::cout << "!!! Attention: running with validity check will slow down "
                   "a lot !!!!";
      runTests<true>();
    } else {
      runTests<false>();
    }
  } catch(std::exception &e) {
    std::cout << "An exception occurred: " << e.what() << std::endl;
  }
}
