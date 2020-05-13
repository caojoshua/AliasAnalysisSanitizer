
#include <stdio.h>

extern "C" void MemAllocHook(void *addr, __int64_t size) {
  printf("memory allocation\n\tstart address: %p\n\tsize in bytes: %lu\n", addr,
         size);
}

extern "C" void PointerDefHook(void *value, void *addr) {
  printf("pointer definition\n\tload into: %p\n\tload from: %p\n", value, addr);
}

extern "C" void StoreHook(void *value, void *addr) {
  printf("store\n\tvalue to store: %p\n\tstore into: %p\n", value, addr);
}
