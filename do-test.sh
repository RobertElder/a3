#  Start the binder and pipe the output to a file to a file so we can automatically set the environment variables
valgrind -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes  ./binder | tee binderoutput & 
#  Give the binder a second to bind and get address and port
echo "Waiting for binder to start up..."
while [ `cat binderoutput | wc -l` -lt 2 ]; do :; done
export SERVER_ADDRESS=`cat binderoutput | head -n 1 | sed 's/SERVER_ADDRESS //'`
export SERVER_PORT=`cat binderoutput | tail -n 1 | sed 's/SERVER_PORT //'`
echo "Binder started..."
for i in {1..10}
do
    valgrind -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes ./server &
done
valgrind -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes  ./client
#--gen-suppressions=all
