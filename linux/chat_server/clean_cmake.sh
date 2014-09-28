find . -iname ".cproject" | xargs rm -rf
find . -iname ".project" | xargs rm -rf
find . -iname ".settings" | xargs rm -rf
find . -iname "*.cmake" | xargs rm -rf
find . -iname "CMakeCache.txt" | xargs rm -rf
find . -iname "CMakeFiles" | xargs rm -rf
find . -iname "*.a" | xargs rm -rf
find . -iname "lib" | xargs rm -rf
find . -iname "Makefile" | xargs rm -rf
# project specific
find . -iname "*.log" | xargs rm -rf
find . -iname "compiled_definitions.h" | xargs rm -rf
rm -rf ./out_*
rm -rf chat_server
