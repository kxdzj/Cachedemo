file(REMOVE_RECURSE
  "libcache_lib.a"
  "libcache_lib.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/cache_lib.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
