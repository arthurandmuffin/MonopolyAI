To run the game agent:

Compile & build the game engine and agents (from project root), run the following:
rm -r build; mkdir build; cd build; cmake ..; cmake --build . --config release

The location of binary files may vary based on OS
    For Windows, likely under build/Release/*.exe/dll
    For macOS/Linux, likely under build/monopoly_engine & build/agents/*.dylib

Update the location of the binary files in neat_training.py

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

