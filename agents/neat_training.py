import neat
import pickle
import os 
import random
import json
import subprocess
import argparse
import multiprocessing as mp
from typing import Dict, List, Tuple

ENGINE_PATH = "./build/Release/monopoly_engine.exe"
NEAT_AGENT_PATH = "./build/agents/Release/neat_bridge.dll"
OPPONENT_AGENTS = [
    "./build/agents/Release/neat_bridge.dll",
    "./build/agents/Release/neat_bridge.dll",
    "./build/agents/Release/neat_bridge.dll",
]
NAIVE_OPPONENT_AGENTS = [
    "./build/agents/Release/greedy_agent.dll",
    "./build/agents/Release/random_agent.dll",
    "./build/agents/Release/greedy_agent.dll",
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

    def create_game_config(self, genome_path: str, game_id: int, test: bool = False) -> Dict:
        # Neat agent config
        neat_config = {
            'genome_path': genome_path,
            'config_path': self.config_path
        }

        # agent specs
        agents = [{
            'path': NEAT_AGENT_PATH,
            'config': json.dumps(neat_config),
            'name': 'NEATAgent'
        }]

        # Opponent agents
        if test:
            for i, opponent_path in enumerate(NAIVE_OPPONENT_AGENTS[:self.num_opponents]):
                agents.append({
                    'path': opponent_path,
                    'config_json': {},
                    'name': f'Opponent{i+1}'
                })
        else:
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
    
    def run_game(self, genome_path: str, game_id: int, test: bool = False) -> Tuple[int, Dict]:
        # Prepare agent configs
        agents = []
        neat_config = {
            'genome_path': genome_path,
            'config_path': self.config_path
        }
        agents.append({
            'path': NEAT_AGENT_PATH,
            'config': neat_config,
            'name': 'NEATAgent'
        })

        if test:
            for i, opponent_path in enumerate(NAIVE_OPPONENT_AGENTS[:self.num_opponents]):
                agents.append({
                    'path': opponent_path,
                    'config': {},
                    'name': f'Opponent{i+1}'
                })
        else:
            for i, opponent_path in enumerate(OPPONENT_AGENTS[:self.num_opponents]):
                agents.append({
                    'path': opponent_path,
                    'config': {},
                    'name': f'Opponent{i+1}'
                })

        # Write each agent's config to a temp file
        temp_files = []
        try:
            agent_args = []
            for idx, agent in enumerate(agents):
                config_path = f'temp_agent_config_{game_id}_{idx}.json'
                with open(config_path, 'w') as f:
                    json.dump(agent['config'], f)
                temp_files.append(config_path)
                agent_args += [
                    '--agent',
                    agent['path'],
                    config_path,
                    agent['name']
                ]

            # Build the engine command
            seed = random.randint(0, int(1e9))
            max_turns = 1000
            cmd = [
                ENGINE_PATH,
                str(game_id),
                str(seed),
                str(max_turns),
            ] + agent_args

            print("Running command:", ' '.join(cmd))
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30
            )
            print("STDOUT:", result.stdout)
            print("STDERR:", result.stderr)
            print("Return code:", result.returncode)

            # Parse result
            if result.returncode == 0:
                lines = result.stdout.strip().split('\n')
                winner = -1
                turns = 0
                for line in lines:
                    if line.startswith('{') and 'winner' in line:
                        game_result = json.loads(line)
                        winner = game_result.get('winner', -1)
                        stats = {
                            'turns': game_result.get('turns', 0),
                            'winner': winner,
                            'game_id': game_id,
                            'penalties': game_result.get('penalties', {}),
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
            for f in temp_files:
                if os.path.exists(f):
                    os.remove(f)

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
                    penalties = stats.get('penalties', {})
                    
        finally:
            os.remove(genome_path)

        if valid_games == 0:
            return 0.0
        win_rate = wins / valid_games
        avg_turns = total_turns / valid_games
        # Win rate weighted most heavily, longer games when winning are better
        if win_rate > 0:
            fitness = win_rate * 1000 + (avg_turns / 1000) * 100 - sum(penalties.values())
        else:
            fitness = max(0, 100 - avg_turns / 10) - sum(penalties.values())
        return fitness
    
    # Parallel evaluation of genomes generated by Copilot
    def eval_genomes_parallel(self, genomes, config):
        """Evaluate all genomes in parallel"""
        print(f"\n=== Generation {self.generation} ===")
        
        # Use multiprocessing for parallel evaluation
        with mp.Pool(processes=mp.cpu_count() // 2) as pool:
            fitness_results = pool.starmap(
                self.evaluate_genome,
                [(genome, config) for genome_id, genome in genomes]
            )
        
        # Assign fitness to genomes
        for (genome_id, genome), fitness in zip(genomes, fitness_results):
            genome.fitness = fitness
            print(f"Genome {genome_id}: fitness = {fitness:.2f}")
        
        # Print statistics
        fitnesses = [g.fitness for _, g in genomes]
        print(f"\nGeneration {self.generation} Stats:")
        print(f"  Max fitness: {max(fitnesses):.2f}")
        print(f"  Avg fitness: {sum(fitnesses)/len(fitnesses):.2f}")
        print(f"  Min fitness: {min(fitnesses):.2f}")
        
        self.generation += 1

    def eval_genomes_sequential(self, genomes, config):
        # Sequential evaluation of genomes, for debugging
        print(f"\nGeneration {self.generation} ")
        for genome_id, genome in genomes:
            fitness = self.evaluate_genome(genome, config)
            genome.fitness = fitness
            print(f"Genome {genome_id}: fitness = {fitness:.2f}")
        
        # Print statistics
        fitnesses = [g.fitness for _, g in genomes]
        print(f"\nGeneration {self.generation} Stats:")
        print(f"  Max fitness: {max(fitnesses):.2f}")
        print(f"  Avg fitness: {sum(fitnesses)/len(fitnesses):.2f}")
        print(f"  Min fitness: {min(fitnesses):.2f}")
        
        self.generation += 1

    def train(self, generations: int = 50, checkpoint = None, parallel = False):
        if checkpoint:
            print(f"Restoring from checkpoint {checkpoint}...")
            p = neat.Checkpointer.restore_checkpoint(checkpoint)
            self.generation = int(checkpoint.split('-')[-1])
        else:
            p = self.population 
        
        # Reporters to monitor and log progress
        p.add_reporter(neat.StdOutReporter(True))
        stats = neat.StatisticsReporter()
        p.add_reporter(stats)
        p.add_reporter(neat.Checkpointer(generation_interval=5, filename_prefix='checkpoints/neat-checkpoint-'))

        # Run the NEAT algorithm for a given number of generations
        best_genome = None
        if parallel:
            print("Running parallel genome evaluation...")
            best_genome = p.run(self.eval_genomes_parallel, generations)
        else:
            print("Running sequential genome evaluation...")
            best_genome = p.run(self.eval_genomes_sequential, generations)
        
        # Save the best genome
        with open('genomes/best_genome.pkl', 'wb') as f:
            pickle.dump(best_genome, f)
        print("Best genome saved to genomes/best_genome.pkl")
        print(f"Final fitness: {best_genome.fitness:.2f}")

        return best_genome, stats
    
    def test_genome(genome_path='genomes/best_genome.pkl', num_games: int = 20):
        trainer = NeatTraining(num_opponents=2, num_games=num_games)
        wins = 0
        total_turns = 0
        valid_games = 0
        for i in range(num_games):
            best_genome, stats = trainer.run_game(genome_path, i, test=True)
            if best_genome != -1:
                valid_games += 1
                if best_genome == 0:
                    wins += 1
                total_turns += stats['turns']
                penalties = stats.get('penalties', {})
                #print stats based on output


        if valid_games > 0:
            win_rate = wins / valid_games
            avg_turns = total_turns / valid_games
            print(f"Test Results over {valid_games} valid games:")
            print(f"  Wins: {wins}")
            print(f"  Win Rate: {win_rate * 100:.2f}%")
            print(f"  Average Turns: {avg_turns:.2f}")
            print(f"  Total Penalties: {sum(penalties.values())}")
        else:
            print("No valid games were played during testing.")

                

def main():
    parser = argparse.ArgumentParser(description="Train NEAT agent for MonopolyAI")
    parser.add_argument('--train', action='store_true', help='Train the NEAT agent')
    parser.add_argument('--test', action='store_true', help='Test the NEAT agent against naive opponents')
    parser.add_argument('--generations', type=int, default=50, help='Number of generations')
    parser.add_argument('--opponents', type=int, default=2, help='Number of opponent agents')
    parser.add_argument('--num_games', type=int, default=20, help='Number of games per genome evaluation')
    parser.add_argument('--checkpoint', type=str, default=None, help='Path to checkpoint file')
    parser.add_argument('--genome', type=str, default='genomes/best_genome.pkl', help='Path to genome file for testing')
    parser.add_argument('--parallel', action='store_true', help='Use parallel evaluation of genomes')
    args = parser.parse_args()

    if args.train:
        # Train NEAT agent (parallel for faster training)
        trainer = NeatTraining(num_opponents=args.opponents, num_games=args.num_games)
        trainer.train(generations=args.generations, checkpoint=args.checkpoint, parallel=args.parallel)
    elif args.test:
        # Test against naive opponents
        NeatTraining.test_genome(genome_path=args.genome, num_games=args.num_games)
    else:
        print("Use --train to train or --test to test an agent")
        print(f"Example: python {__file__} --train --generations 30 --num_games 5")
        print(f"Example: python {__file__} --test --genome genomes/best_genome.pkl")
        
if __name__ == "__main__":
    main()


