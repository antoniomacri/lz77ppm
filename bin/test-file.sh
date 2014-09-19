./lz77ppm -stc "$1" -fo "$1.lz"
if [ $? -ne 0 ]; then
    exit
fi

echo
./lz77ppm -std "$1.lz" -fo "$1.dec"
if [ $? -ne 0 ]; then
    exit
fi

echo
echo Comparing \"$1\" with \"$1.dec\"...
diff "$1" "$1.dec"
if [ $? = 0 ]; then
    echo -e "\033[1;32mOK, the decompressed file is equal to the original.\033[0m"
else
    echo -e "\033[1;31mError! The decompressed file differs from the original.\033[0m"
fi
