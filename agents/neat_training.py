import neat
import pickle
import os 
import random
import json
import subprocess
from typing import Dict, List, Tuple

ENGINE_PATH = ""
NEAT_AGENT_PATH = "./build/agents/Release/neat_bridge.dll"
OPPONENT_AGENTS = [
    "./build/agents/Release/neat_bridge.dll",
    "./build/agents/Release/neat_bridge.dll",
]

class NeatTraining:
    def __init__(self, config_path='neat_config.txt' , num_opponents: int = 2, num_games: int = 50):
        self.config_path = config_path
        self.num_opponents = num_opponents
        self.num_games = num_games
        self.generation = 0

        self.config = neat.Config(
            neat.DefaultGenome,
            neat.DefaultReproduction,
            neat.DefaultSpeciesSet,
            neat.DefaultStagnation,
            config_path
        )
        self.population = neat.Population(self.config)

    def create_game_config(self, genome_path: str, game_id: int) -> Dict:
        # Neat agent config
        neat_config = {
            'genome_path': genome_path,
            'neat_config_path': self.config_path
        }

        # agent specs
        agents = [{
            'path': NEAT_AGENT_PATH,
            'config': json.dumps(neat_config),
            'name': 'NEATAgent'
        }]

        # Opponent agents
        for i, opponent_path in enumerate(OPPONENT_AGENTS[:self.num_opponents]):
            agents.append({
                'path': opponent_path,
                'config_json': {},
                'name': f'Opponent{i+1}'
            })
        
        # Game config
        game_config = {
            'game_id': game_id,
            'seed': random.randint(0, int(1e9)),
            'max_turns': 1000,
            'agents': agents
        }
        return game_config
    
    def run_game(self, genome_path: str, game_id: int) -> Tuple[int, Dict]:
        game_config = self.create_game_config(genome_path, game_id)
        # Save game config to temporary file
        config_path = f'temp_game_config_{game_id}.json'
        with open(config_path, 'w') as f:
            json.dump(game_config, f)

        try:
            result = subprocess.run(
                [ENGINE_PATH, '--config', config_path],
                capture_output=True,
                text=True,
                timeout=30
            )
            # Parse result
            if result.returncode == 0:
                lines = result.stdout.strip().split('\n')
                winner = -1
                turns = 0
                for line in lines:
                    if line.startswith('{') and 'winner' in line:
                        game_result = json.loads(line)
                        winner = game_result.get('winner', -1)
                        # Extract game statistics
                        stats = {
                            'turns': game_result.get('turns', 0),
                            'winner': winner,
                            'game_id': game_id
                        }

                        return winner, stats
                return -1, {'turns': 0, 'winner': -1, 'game_id': game_id}
            else:
                print(f"Game {game_id} failed with error: {result.stderr}")
                return -1, {'turns': 0, 'winner': -1, 'game_id': game_id}
        except subprocess.TimeoutExpired:
            print(f"Game {game_id} timed out.")
            return -1, {'turns': 0, 'winner': -1, 'game_id': game_id}
        except Exception as e:
            print(f"Game {game_id} encountered an error: {e}")
            return -1, {'turns': 0, 'winner': -1, 'game_id': game_id}
        finally:
            os.remove(config_path)

    def evaluate_genome(self, genome, config) -> float: 
        # Save genome to temporary file
        genome_path = f'temp_genome_{genome.key}.pkl'
        with open(genome_path, 'wb') as f:
            pickle.dump(genome, f)

        wins = 0
        total_turns = 0
        valid_games = 0
        try:
            for i in range(self.num_games):
                game_id = genome.key * 1000 + i
                winner, stats = self.run_game(genome_path, game_id)
                if winner != -1:
                    valid_games += 1
                    if winner == 0:
                        wins += 1
                    total_turns += stats['turns']
                    # other stats (reward/penalty)
        finally:
            os.remove(genome_path)

        if valid_games == 0:
            return 0.0
        win_rate = wins / valid_games
        avg_turns = total_turns / valid_games
        # Win rate weighted most heavily, longer games when winning are better
        if win_rate > 0:
            fitness = win_rate * 1000 + (avg_turns / 1000) * 100
        else:
            fitness = max(0, 100 - avg_turns / 10)
        return fitness
    
    