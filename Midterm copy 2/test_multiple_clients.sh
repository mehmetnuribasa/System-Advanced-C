#!/bin/bash
# Start the server in the background
./bank_server &
SERVER_PID=$!

# Wait for the server to start
sleep 2

# Start multiple clients with delays between them
./bank_client Client01.file &
CLIENT1_PID=$!
sleep 5  # Wait for the first client to finish its first transaction

./bank_client Client02.file &
CLIENT2_PID=$!
sleep 5  # Wait for the second client to finish its first transaction

./bank_client Client03.file &
CLIENT3_PID=$!

# Wait for all clients to finish
wait $CLIENT1_PID $CLIENT2_PID $CLIENT3_PID

# Kill the server
kill $SERVER_PID

# Show the log file
echo "Log file contents:"
cat AdaBank.bankLog 