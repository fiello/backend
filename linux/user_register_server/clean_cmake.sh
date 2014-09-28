find . -iname ".cproject" | xargs rm -rf
find . -iname ".project" | xargs rm -rf
find . -iname "*.cmake" | xargs rm -rf
find . -iname "CMakeCache.txt" | xargs rm -rf
find . -iname "CMakeFiles" | xargs rm -rf
find . -iname "*.a" | xargs rm -rf
find . -iname "Makefile" | xargs rm -rf
