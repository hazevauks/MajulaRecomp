// Prints the XXH3-64 of each file given: "<16 hex>  <path>". The native renderer names a shader container by this
// hash in its logs ("library n502: VS container 594bac2ce494dd72") and in the files it dumps (vs_<hash>.bin), so the
// tools that pair containers with those names (tools/extract_shaders.py) need the same number. Python has no XXH3.
//
//   clang++ -O2 -Isdk/thirdparty/xxHash tools/xxh3sum.cpp -o out/tools/xxh3sum.exe
//   out/tools/xxh3sum.exe file [file ...]        (or: xxh3sum.exe @list.txt, one path per line)
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#define XXH_INLINE_ALL
#include <xxhash.h>

static int Sum(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    std::fprintf(stderr, "cannot read %s\n", path.c_str());
    return 1;
  }
  std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  std::printf("%016llx  %s\n", static_cast<unsigned long long>(XXH3_64bits(data.data(), data.size())), path.c_str());
  return 0;
}

int main(int argc, char** argv) {
  int failed = 0;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '@') {
      std::ifstream list(argv[i] + 1);
      for (std::string line; std::getline(list, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) failed |= Sum(line);
      }
    } else {
      failed |= Sum(argv[i]);
    }
  }
  return failed;
}
