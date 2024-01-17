#!/bin/bash

bash scripts/build.sh

date


# 
echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Starting Exchange..."
echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"

# Start the exchange_main program in the background, redirecting both stdout and stderr to a log file
./cmake-build-release/exchange_main 2>&1 &
sleep 10

# Run the run_clients.sh script
bash ./scripts/run_clients.sh

sleep 5

echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Stopping Exchange..."
echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"

# Send a SIGINT signal to the exchange_main process to stop it gracefully
pkill -2 exchange

sleep 10
# Wait for all background processes to finish
wait
date