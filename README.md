To run the game agent:

In the root directory, compile and build the game engine and game agents with CMake:
mkdir build
cd build
cmake ..
cmake --build . --config Release

In the root directory, to test the agent against naive agents:
python agents/neat_training.py --test --num_games (num games) 
To evaluate a specific checkpoint/genome:
Optional: --checkpoint (checkpoint path) --genome (genome path)

In the root directory, to train the agent against other neat agents:
python agents/neat_training.py --train --generations (num generations) --num_games (num games) 
To resume training from a specific checkpoint:
--checkpoint (checkpoint path)
To set number of opponents to train against:
--opponents (num opponents)

