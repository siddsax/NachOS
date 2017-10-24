cd ../test
make
echo -e "\n\n\n\n\n"
cd ../threads
make depend
make
echo -e "\n\n\n\n\n"
cd ../userprog
make depend
make