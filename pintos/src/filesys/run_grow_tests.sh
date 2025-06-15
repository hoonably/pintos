# ~/pintos/src/filesys#
# 경로에서
# chmod +x run_grow_tests.sh
# ./run_grow_tests.sh

# make
make

# build directory로 이동
cd build

# grow-create
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --bochs --disk=tmp.dsk \
  -p tests/filesys/extended/grow-create -a grow-create \
  -- -q -f run grow-create < /dev/null
rm -f tmp.dsk

# grow-file-size
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --bochs --disk=tmp.dsk \
  -p tests/filesys/extended/grow-file-size -a grow-file-size \
  -p tests/filesys/extended/tar -a tar \
  -- -q -f run grow-file-size < /dev/null
rm -f tmp.dsk

# grow-seq-lg
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --bochs --disk=tmp.dsk \
  -p tests/filesys/extended/grow-seq-lg -a grow-seq-lg \
  -p tests/filesys/extended/tar -a tar \
  -- -q -f run grow-seq-lg < /dev/null
rm -f tmp.dsk

# grow-seq-sm
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --bochs --disk=tmp.dsk \
  -p tests/filesys/extended/grow-seq-sm -a grow-seq-sm \
  -p tests/filesys/extended/tar -a tar \
  -- -q -f run grow-seq-sm < /dev/null
rm -f tmp.dsk

# grow-sparse
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --bochs --disk=tmp.dsk \
  -p tests/filesys/extended/grow-sparse -a grow-sparse \
  -p tests/filesys/extended/tar -a tar \
  -- -q -f run grow-sparse < /dev/null
rm -f tmp.dsk

# grow-tell
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --bochs --disk=tmp.dsk \
  -p tests/filesys/extended/grow-tell -a grow-tell \
  -p tests/filesys/extended/tar -a tar \
  -- -q -f run grow-tell < /dev/null
rm -f tmp.dsk

# grow-two-files
pintos-mkdisk tmp.dsk --filesys-size=2
pintos -v -k -T 60 --bochs --disk=tmp.dsk \
  -p tests/filesys/extended/grow-two-files -a grow-two-files \
  -p tests/filesys/extended/tar -a tar \
  -- -q -f run grow-two-files < /dev/null
rm -f tmp.dsk
