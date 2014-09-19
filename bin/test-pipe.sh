./lz77ppm "$1" | ./lz77ppm -d | diff "$1" -
if [ $? = 0 ]; then
    echo -e "\033[1;32mOK, the decompressed file is equal to the original.\033[0m"
else
    echo -e "\033[1;31mError! The decompressed file differs from the original.\033[0m"
fi
