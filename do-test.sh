#  Start the binder and pipe the output to a file to a file so we can automatically set the environment variables
./binder | tee binderoutput & 
#  Give the binder a second to bind and get address and port
sleep 1

export SERVER_ADDRESS=`cat binderoutput | head -n 1 | sed 's/SERVER_ADDRESS //'`
export SERVER_PORT=`cat binderoutput | tail -n 1 | sed 's/SERVER_PORT //'`
for i in {1..10}
do
    ./server &
done
valgrind -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes  ./client
