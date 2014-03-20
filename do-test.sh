
if [ `grep -R -P '\t' *.cpp *.h | wc -l` -gt 0 ]
then
    echo "There is a tab in one of the files";
    grep -R -P '\t' *.cpp *.h;
    exit 1;
fi

#  Get rid of race condition where binderoutput file already has stuff in it and causes startup to continue
#  before server address has been output.
:> binderoutput
#  Ignore rules for valgrind
SUPPRESSIONS="--gen-suppressions=all"
#  Start the binder and pipe the output to a file to a file so we can automatically set the environment variables
valgrind ${SUPPRESSIONS} -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes  ./binder | tee binderoutput & 
#  Give the binder a second to bind and get address and port
echo "Waiting for binder to start up..."
while [ `cat binderoutput | grep BINDER_ADDRESS | wc -l` -lt 1 ]; do :; done
BINDER_ADDRESS=`cat binderoutput | head -n 1 | sed 's/BINDER_ADDRESS //'`
BINDER_PORT=`cat binderoutput | tail -n 1 | sed 's/BINDER_PORT //'`
export BINDER_ADDRESS=${BINDER_ADDRESS}
export BINDER_PORT=${BINDER_PORT}
echo "Binder started..."
HOSTS=("ubuntu1204-002" "ubuntu1204-004" "ubuntu1204-006")
for i in {1..5}
do
    #host=${HOSTS[$RANDOM % 3]}
    #echo "Launching server on host ${host}"
    #ssh -i ~/.ssh/foo ${host} "cd /u0/`whoami`/cs454/a3 && setenv BINDER_ADDRESS ${BINDER_ADDRESS}; setenv BINDER_PORT ${BINDER_PORT}; valgrind --gen-suppressions=all -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes ./custom_server" &
    valgrind --gen-suppressions=all -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes ./custom_server &
    ./server &
    sleep 1
done
#  Wait a couple seconds for the servers to register
echo "Launching custom client..."
#  Run our custom client
for i in {1..5}
do
    #host=${HOSTS[$RANDOM % 3]}
    #echo "Launching client on host ${host}"
    #ssh -i ~/.ssh/foo ${host} "cd /u0/`whoami`/cs454/a3 && setenv BINDER_ADDRESS ${BINDER_ADDRESS}; setenv BINDER_PORT ${BINDER_PORT}; valgrind --gen-suppressions=all -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes  ./custom_client " &
    valgrind --gen-suppressions=all -q --suppressions=valgrind-suppressions --leak-check=full --show-reachable=yes --track-origins=yes  ./custom_client &
    ./client &
    #ssh -i ~/.ssh/foo ${host} "cd /u0/`whoami`/cs454/a3 && setenv BINDER_ADDRESS ${BINDER_ADDRESS}; setenv BINDER_PORT ${BINDER_PORT}; echo \"n\" | ./client" &
done
sleep 5
#  Run the client provided in the assignemnt code
echo "Launching assignment verion of client..."
echo "y" | ./client
echo "\n"
