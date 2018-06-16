#include <cstdint>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <iostream>
#include <sys/auxv.h>
#include <time.h>
#include <sys/time.h>
#include <type_traits>
#include <linux/getcpu.h>

#if __x86_64__
#define BITS 64
#else
#define BITS 32
#endif
#define MY_HELPER2(name, bits) using Elf_ ## name = Elf ## bits ## _ ## name
#define MY_HELPER1(name, bits) MY_HELPER2(name, bits) 
#define MY_HELPER(name) MY_HELPER1(name, BITS) 
MY_HELPER(Ehdr);
MY_HELPER(Phdr);
MY_HELPER(Shdr);
MY_HELPER(Sym);
#undef MY_HELPER
#undef MY_HELPER1
#undef MY_HELPER12
#undef BITS

template <typename T1, typename T2,
          typename T3 = std::add_pointer_t<std::remove_pointer_t<T1>>>
T3 as(T2 v) {
  return reinterpret_cast<T3>(v);
}

void dump_strtab(uintptr_t addr, size_t size) {
  uintptr_t end = addr + size;
  for (uintptr_t addr1 = addr + 1; addr1 <= end; ++addr1) {
    char c = *reinterpret_cast<char*>(addr1);
    if (!c) {
      std::cout << reinterpret_cast<char*>(addr + 1) << std::endl;
      addr = addr1;
    }
  }
}

void dump_section_header(Elf_Shdr & shdr, uintptr_t shstr) {
  std::cout << "name: " << as<char*>(shstr) + shdr.sh_name << std::endl;
  std::cout << "type: " << shdr.sh_type << std::endl;
  std::cout << "flags: " << shdr.sh_flags << std::endl;
  std::cout << "addr: " << shdr.sh_addr << std::endl;
  std::cout << "offset: " << shdr.sh_offset << std::endl;
  std::cout << "size: " << shdr.sh_size << std::endl;
  std::cout << "link: " << shdr.sh_link << std::endl;
  std::cout << "info: " << shdr.sh_info << std::endl;
  std::cout << "addralign: " << shdr.sh_addralign << std::endl;
  std::cout << "entsize: " << shdr.sh_entsize << std::endl;
}

int main() {
  uintptr_t vdso = getauxval(AT_SYSINFO_EHDR);
  {
    auto p = as<char*>(vdso);
    if (p[0] != ELFMAG0 || p[1] != ELFMAG1 || p[2] != ELFMAG2 || p[3] != ELFMAG3) {
      std::cout << "wrong vdso magic number\n";
      for (int i = 0; i < 4; ++i) {
        std::cout << int(p[i]) << ' ';
      }
      std::cout << std::endl;
      return 1;
    } else {
      std::cout << "verified ELF magic number\n";
    }
  }
  //uintptr_t phdr = vdso + as<Elf_Ehdr>(vdso)->e_phoff;
  uintptr_t shdr = vdso + as<Elf_Ehdr>(vdso)->e_shoff;
  auto phnum = as<Elf_Ehdr>(vdso)->e_phnum;
  auto shnum = as<Elf_Ehdr>(vdso)->e_shnum;
  auto shstrndx = as<Elf_Ehdr>(vdso)->e_shstrndx;
  std::cout << "program header table entry number: " << phnum << std::endl;
  std::cout << "section header table entry number: " << shnum << std::endl;
  std::cout << "section name string table section index: " << shstrndx << std::endl;
  auto& str_shdr = as<Elf_Shdr>(shdr)[shstrndx];
  if (str_shdr.sh_type != SHT_STRTAB) {
    std::cout << "wrong string table\n";
    return 1;
  }
  std::cout << "dump section name string table:\n";
  uintptr_t shstr = vdso + str_shdr.sh_offset;
  dump_strtab(shstr, str_shdr.sh_size);
  uintptr_t dynsym;
  Elf_Shdr* dynsym_str;
  size_t dynsym_size;
  for (int i = 0; i < shnum; ++i) {
    std::cout << "dump section header " << i << ":\n";
    auto & shdri = as<Elf_Shdr>(shdr)[i];
    dump_section_header(shdri, shstr);
    if (shdri.sh_type == SHT_DYNSYM) {
      dynsym = vdso + shdri.sh_offset;
      dynsym_str = as<Elf_Shdr>(shdr) + shdri.sh_link;
      dynsym_size = shdri.sh_size;
    }
  }
  std::cout << "dynamic symbol table size: " << dynsym_size << std::endl;
  std::cout << "dynamic symbol string table: " << std::endl;
  uintptr_t dynstr = vdso + as<Elf_Shdr>(dynsym_str)->sh_offset;
  dump_strtab(dynstr, as<Elf_Shdr>(dynsym_str)->sh_size);
  uintptr_t clock_gettime_ptr = 0, gettimeofday_ptr = 0, time_ptr = 0, getcpu_ptr = 0;
  const char * clock_gettime_str = "clock_gettime";
  const char * gettimeofday_str = "__vdso_gettimeofday";
  const char * time_str = "__vdso_time";
  const char * getcpu_str = "__vdso_getcpu";

  for (uintptr_t p = dynsym, e = dynsym + dynsym_size; p < e; p += sizeof(Elf_Sym)) {
    const char* sym_name = as<char*>(dynstr) + as<Elf_Sym>(p)->st_name;
    uintptr_t sym_value = as<Elf_Sym>(p)->st_value;
    std::cout << "symbol name: " << sym_name << std::endl;
    std::cout << "symbol value: " << sym_value << std::endl;
    std::cout << "symbol size: " << as<Elf_Sym>(p)->st_size << std::endl;
    std::cout << "symbol section index: " << as<Elf_Sym>(p)->st_shndx<< std::endl;
    sym_value += vdso;
    if (!strcmp(clock_gettime_str, sym_name)) {
      clock_gettime_ptr = sym_value;
    } else if (!strcmp(gettimeofday_str, sym_name)) {
      gettimeofday_ptr = sym_value;
    } else if (!strcmp(time_str, sym_name)) {
      time_ptr = sym_value;
    } else if (!strcmp(getcpu_str, sym_name)) {
      getcpu_ptr = sym_value;
    }
  }
  if (clock_gettime_ptr) {
    auto my_clock_gettime = reinterpret_cast<int (* )(clockid_t, timespec*)>(clock_gettime_ptr);
    std::cout << (void*)clock_gettime_ptr << std::endl;
    std::cout << (void*)my_clock_gettime << std::endl;
    timespec tp;
    my_clock_gettime(CLOCK_MONOTONIC, &tp);
    std::cout << "clock_gettime: " << tp.tv_sec << ' ' << tp.tv_nsec << std::endl;
  } else {
    std::cout << "no clock_gettime\n";
  }
  if (gettimeofday_ptr) {
    auto my_gettimeofday =
        reinterpret_cast<int (*)(struct timeval *, struct timezone *)>(gettimeofday_ptr);
    struct timeval tv;
    struct timezone tz;
    my_gettimeofday(&tv, &tz);
    std::cout << "gettimeofday:\n";
    std::cout << tv.tv_sec << ' ' << tv.tv_usec << std::endl;
    std::cout << tz.tz_minuteswest << ' ' << tz.tz_dsttime << std::endl;
  } else {
    std::cout << "no gettimeofday\n";
  }
  if (time_ptr) {
    auto my_time =
        reinterpret_cast<time_t (*)(time_t*)>(time_ptr);
    std::cout << "time: " << my_time(nullptr) << std::endl;
  } else {
    std::cout << "no time\n";
  }
  if (getcpu_ptr) {
    auto my_getcpu = 
        reinterpret_cast<int(*)(unsigned*, unsigned*, getcpu_cache*)>(getcpu_ptr);
    unsigned cpu, nodestruct;
    getcpu_cache tcache;
    my_getcpu(&cpu, &nodestruct, &tcache);
    std::cout << "getcpu: " << cpu << ' ' << nodestruct << std::endl;
  } else {
    std::cout << "no getcpu\n";
  }
  /*
  if (as<char*>(vdso
  std::cout << vdso->e_ident[1] << vdso->e_ident[2] << vdso->e_ident[3]
            << std::endl;
  auto* phdr = (Elf64_Phdr*)((void*)vdso + vdso->e_phoff);
  Elf64_Addr p_vaddr;
  for (int i = 0; i < vdso->e_phnum; ++i) {
    if (phdr[i].p_type == PT_DYNAMIC) {
      p_vaddr = phdr[i].p_vaddr;
      break;
    }
  }
  std::cout << vdso << std::endl;
  p_vaddr += (Elf64_Addr)vdso;
  auto* dyn = (Elf64_Dyn*)p_vaddr;
  char* strtab;
  Elf64_Sym *symtab;
Elf64_Xword strsz;
  while (true) {
    if (!dyn->d_tag) {
      break;
    } else if (dyn->d_tag == DT_SYMTAB) {
      symtab = (Elf64_Sym*)(dyn->d_un.d_ptr);
    } else if (dyn->d_tag == DT_STRTAB) {
      strtab = (char*)(dyn->d_un.d_ptr);
    } else if (dyn->d_tag == DT_STRSZ) {
      strsz = dyn->d_un.d_val;
    } else if (dyn->d_tag == DT_SYMENT) {
      std::cout << "size: " << dyn->d_un.d_val << std::endl;
    }
    ++dyn;
  }
  add(symtab, vdso);
  add(strtab, vdso);
  for (int i = 0; i < strsz; ++i) {
    if (strtab[i]) {
      std::cout << strtab[i];
    } else {
      std::cout << '|';
    }
  }
  std::cout << std::endl;
  std::cout << std::hex << symtab[1].st_value << std::endl;
  void* vdso1 = vdso;
  add(vdso1, symtab[1].st_value);
  std::cout << vdso1 << std::endl;
  int (* vdso_clockgettime )(clockid_t, struct timespec *);
  vdso_clockgettime = reinterpret_cast<decltype(vdso_clockgettime)>(vdso1);
  std::cout << (void*)vdso_clockgettime << std::endl;
  printf("%x\n", vdso_clockgettime);
  void* vdso2 = reinterpret_cast<void*>(vdso_clockgettime);
  std::cout << vdso2 << std::endl;
  timespec tp;
vdso_clockgettime(CLOCK_REALTIME, &tp);
std::cout << std::dec;
std::cout << tp.tv_sec << std::endl;
std::cout << tp.tv_nsec << std::endl;
*/
}
