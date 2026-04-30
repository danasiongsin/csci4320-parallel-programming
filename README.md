# CSCI4320---Parallel-Programming

How to run:
1. Clone repository to local machine
- Make sure that all files from directory template-model-master are copied into ROSS/models
3. scp -r "path to repo on local machine" "path in personal CCI account (barn or scratch)"
4. ssh into personal CCI account and front end node
- (EXAMPLE) ssh PCPGnngs@blp01.ccni.rpi.edu
- ssh PCPGnngs@dcsfen01
5. Move to folder ROSS/models
- cd scratch/ROSS/models
7. module load xl_r spectrum-mpi cmake
8. cmake .. -DROSS_BUILD_MODELS=ON
9. make
10. Run one test
- mpirun -np 2 ./models/model --synch=3 --strategy=0 --lps=51 --rsi-low=50.0 --rsi-high=51.0
- synch=3 runs model in parallel
- strategy=0 runs all 3 trading strategies
- You can vary: lps, rsi-low, rsi-high, noise_probs, ordersize
10. Try running a script. Scripts will run several tests at a time
- chmod +x weakscaling.sh
- ./weakscaling.sh
