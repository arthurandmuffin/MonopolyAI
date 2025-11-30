import neat
import pickle
import os 
import random
import json
import subprocess
import argparse
import multiprocessing as mp
import _socket
from typing import Dict, List, Tuple
import os
os.environ['PYTHONHOME'] = r'C:\Users\xiayi\AppData\Local\Programs\Python\Python311'
os.environ['PYTHONPATH'] = r'C:\Users\xiayi\AppData\Local\Programs\Python\Python311\DLLs'

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
    def __init__(self, config_path='agents/neat_config.txt' , num_opponents: int = 2, num_games: int = 50):
        self.config_path = config_path
        self.num_opponents = num_opponents
        self.num_games = num_games
        self.generation = 0
        self.game_counter = 0

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
            'config': neat_config,
            'name': 'NEATAgent'
        }]

        # Opponent agents
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
                    'config': neat_config,
                    'name': f'Opponent{i+1}'
                })
        
        # Game config
        game_config = {
            'game_id': game_id,
            'seed': random.randint(0, int(1e9)),
            'max_turns': 300,
            'agents': agents
        }
        return game_config
    
    def create_tournament_config(self, genome_paths: List[str], game_id: int) -> Dict:
        agents = []
        for i, genome_path in enumerate(genome_paths):
            neat_config = {
                'genome_path': genome_path,
                'config_path': self.config_path
            }
            agents.append({
                'path': NEAT_AGENT_PATH,
                'config': neat_config,
                'name': f'Agent{i+1}'
            })

        tournament_config = {
            'game_id': game_id,
            'seed': random.randint(0, int(1e9)),
            'max_turns': 300,
            'agents': agents
        }
        return tournament_config
    
    def run_game(self, genome_path: str, game_id: int, test: bool = False) -> Tuple[int, Dict]:
        # Prepare agent configs
        config = self.create_game_config(genome_path, game_id, test)

        agents = config['agents']

        # Write each agent's config to a temp file
        temp_files = []
        try:
            agent_args = []
            for index, agent in enumerate(agents):
                config_path = f'temp_agent_config_{game_id}_{index}.json'
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
            max_turns = 300
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
                timeout=60,
            )
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
                            'winner': winner,
                            'game_id': game_id,
                            'penalties': game_result.get('penalties', {}),
                            'player_scores': game_result.get('player_scores', []),
                        }
                        return winner, stats
                return -1, {'winner': -1, 'game_id': game_id}
            else:
                print(f"Game {game_id} failed with error: {result.stderr}")
                return -1, {'winner': -1, 'game_id': game_id}
        except subprocess.TimeoutExpired:
            print(f"Game {game_id} timed out.")
            return -1, {'winner': -1, 'game_id': game_id}
        except Exception as e:
            print(f"Game {game_id} encountered an error: {e}")
            return -1, {'winner': -1, 'game_id': game_id}
        finally:
            for f in temp_files:
                if os.path.exists(f):
                    os.remove(f)

    def run_tournament_game(self, genome_path: List[str], game_id: int) -> Tuple[int, Dict]:
        # Prepare agent configs
        config = self.create_tournament_config(genome_path, game_id)

        agents = config['agents']

        # Write each agent's config to a temp file
        temp_files = []
        try:
            agent_args = []
            for index, agent in enumerate(agents):
                config_path = f'temp_agent_config_{game_id}_{index}.json'
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
            max_turns = 300
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
                timeout=60,
            )
            print("STDOUT:", result.stdout)
            print("Return code:", result.returncode)

            # Parse result
            if result.returncode == 0:
                lines = result.stdout.strip().split('\n')
                winner = -1
                for line in lines:
                    if line.startswith('{') and 'winner' in line:
                        game_result = json.loads(line)
                        winner = game_result.get('winner', -1)
                        stats = {
                            'winner': winner,
                            'game_id': game_id,
                            'penalties': game_result.get('penalties', {}),
                            'player_scores': game_result.get('player_scores', []),
                        }
                        return winner, stats
                return -1, {'winner': -1, 'game_id': game_id}
            else:
                print(f"Game {game_id} failed with error: {result.stderr}")
                return -1, {'winner': -1, 'game_id': game_id}
        except subprocess.TimeoutExpired:
            print(f"Game {game_id} timed out.")
            return -1, {'winner': -1, 'game_id': game_id}
        except Exception as e:
            print(f"Game {game_id} encountered an error: {e}")
            return -1, {'winner': -1, 'game_id': game_id}
        finally:
            for f in temp_files:
                if os.path.exists(f):
                    os.remove(f)



    def evaluate_genome(self, genome, config) -> float: 
        genome_path = f'temp_genome_{genome.key}.pkl'
        with open(genome_path, 'wb') as f:
            pickle.dump(genome, f)

        wins = 0
        valid_games = 0
        total_score = 0
        total_opponent_score = 0  # Track across all games
        total_penalty = 0
        
        try:
            for i in range(self.num_games):
                game_id = self.game_counter
                self.game_counter += 1
                winner, stats = self.run_game(genome_path, game_id)
                
                if winner != -1:
                    valid_games += 1
                    if winner == 0:
                        wins += 1
                
                # Accumulate stats across ALL games
                if 'penalties' in stats and isinstance(stats['penalties'], list) and len(stats['penalties']) > 0:
                    total_penalty += stats['penalties'][0]
                
                if 'player_scores' in stats and isinstance(stats['player_scores'], list):
                    if len(stats['player_scores']) > 0:
                        total_score += stats['player_scores'][0]
                    if len(stats['player_scores']) > 1:
                        # Average opponent scores for this game
                        game_opp_score = sum(stats['player_scores'][1:]) / (len(stats['player_scores']) - 1)
                        total_opponent_score += game_opp_score
                            
        finally:
            os.remove(genome_path)

        # Calculate averages
        win_rate = wins / valid_games if valid_games > 0 else 0
        avg_score = total_score / self.num_games
        avg_opponent_score = total_opponent_score / self.num_games
        avg_penalty = total_penalty / self.num_games
        
        # Adjusted fitness formula
        # Reduce penalty impact, increase score differential importance
        if win_rate > 0:
            fitness = win_rate * 10000 + (avg_score - avg_opponent_score) / 10 - avg_penalty
        else:
            # Even without wins, reward beating opponents
            fitness = (avg_score - avg_opponent_score) / 10 - avg_penalty
        
        return fitness
    
    def evaluate_tournament_match(self, genomes: List[Tuple[int, object]]) -> Dict[int, float]:
        # Save genomes to temp file
        genome_paths = {}
        for gid, genome in genomes:
            path = f'temp_genome_{gid}.pkl'
            with open(path, 'wb') as f:
                pickle.dump(genome, f)
            genome_paths[gid] = path
        
        # Track stats across ALL games for each genome
        genome_stats = {
            gid: {
                'wins': 0, 
                'games': 0, 
                'total_score': 0, 
                'total_opponent_score': 0,  # NEW: accumulate across all games
                'penalties': 0
            } 
            for gid, _ in genomes
        }
        
        try:
            # Play tournament
            for i in range(self.num_games):
                game_id = self.game_counter
                self.game_counter += 1
                
                paths = [genome_paths[gid] for gid, _ in genomes]
                winner, stats = self.run_tournament_game(paths, game_id)

                for index, (gid, _) in enumerate(genomes):
                    genome_stats[gid]['games'] += 1
                    if index == winner:
                        genome_stats[gid]['wins'] += 1

                    # Get player scores for THIS game
                    scores = stats.get('player_scores', [])
                    if isinstance(scores, list) and len(scores) == len(genomes):
                        # Add this genome's score
                        if len(scores) > index:
                            genome_stats[gid]['total_score'] += scores[index]
                        
                        # Calculate and accumulate opponent scores for THIS game
                        opponent_scores = [s for i, s in enumerate(scores) if i != index]
                        if opponent_scores:
                            game_avg_opponent_score = sum(opponent_scores) / len(opponent_scores)
                            genome_stats[gid]['total_opponent_score'] += game_avg_opponent_score
                    
                    # Accumulate penalties
                    if 'penalties' in stats and isinstance(stats['penalties'], list) and len(stats['penalties']) > index:
                        genome_stats[gid]['penalties'] += stats['penalties'][index]

        finally:
            for path in genome_paths.values():
                if os.path.exists(path):
                    os.remove(path)

        # Calculate fitness for each genome
        fitness_results = {}
        for gid, stats in genome_stats.items():
            win_rate = 0
            if stats['games'] > 0:
                win_rate = stats['wins'] / stats['games']
            
            avg_score = stats['total_score'] / self.num_games
            avg_opponent_score = stats['total_opponent_score'] / self.num_games  # FIXED
            avg_penalty = stats['penalties'] / self.num_games
            
            # Adjusted fitness formula (same as regular evaluation)
            if win_rate > 0:
                fitness_results[gid] = (
                    win_rate * 10000 + 
                    (avg_score - avg_opponent_score) / 5 
                    # Train without penalty
                     #- avg_penalty
                )
            else:
                fitness_results[gid] = (
                    (avg_score - avg_opponent_score) / 5 
                    # Train without penalty
                     #- avg_penalty
                )
            
            # Debug output
            print(f"    Genome {gid} detailed stats:")
            print(f"      Wins: {stats['wins']}/{stats['games']}, Win Rate: {win_rate:.2%}")
            print(f"      Avg Score: {avg_score:.1f} vs Opponents: {avg_opponent_score:.1f}")
            print(f"      Avg Penalty: {avg_penalty:.2f}")

        return fitness_results
                        
    # Tournament-style evaluation of genomes generated by Copilot
    def eval_genomes_tournament(self, genomes, config):
        """
        Tournament-style evaluation:
        - Randomly group genomes into sets of 4
        - Each group plays games_per_match games
        - Genomes earn fitness based on performance
        """
        print(f"\n=== Generation {self.generation} - Tournament Mode ===")
        
        genome_list = list(genomes)
        random.shuffle(genome_list)
        
        # Initialize all fitnesses to 0
        for genome_id, genome in genome_list:
            genome.fitness = 0
        
        # Group genomes into tournaments of 4
        num_complete_tournaments = len(genome_list) // 4
        
        print(f"Running {num_complete_tournaments} tournaments with 4 genomes each")
        
        for tournament_idx in range(num_complete_tournaments):
            start_idx = tournament_idx * 4
            tournament_genomes = genome_list[start_idx:start_idx + 4]
            
            print(f"\nTournament {tournament_idx + 1}/{num_complete_tournaments}")
            print(f"  Genomes: {[gid for gid, _ in tournament_genomes]}")
            
            # Run tournament and get fitness scores
            fitness_scores = self.evaluate_tournament_match(tournament_genomes)
            
            # Assign fitness to genomes
            for genome_id, genome in tournament_genomes:
                genome.fitness = fitness_scores[genome_id]
                print(f"  Genome {genome_id}: fitness = {genome.fitness:.2f}")
        
        # Handle remaining genomes (if population not divisible by 4)
        remaining = len(genome_list) % 4
        if remaining > 0:
            print(f"\n{remaining} genomes remaining - competing against each other")
            remaining_genomes = genome_list[-remaining:]
            
            # If only 1-3 remaining, pad with random genomes from completed tournaments
            while len(remaining_genomes) < 4:
                random_genome = random.choice(genome_list[:-remaining])
                remaining_genomes.append(random_genome)
            
            fitness_scores = self.evaluate_tournament_match(remaining_genomes[:4])
            
            # Only update fitness for the actual remaining genomes
            for genome_id, genome in genome_list[-remaining:]:
                genome.fitness = fitness_scores[genome_id]
                print(f"  Genome {genome_id}: fitness = {genome.fitness:.2f}")
        
        # Print generation statistics
        fitnesses = [g.fitness for _, g in genome_list]
        print(f"\nGeneration {self.generation} Stats:")
        print(f"  Max fitness: {max(fitnesses):.2f}")
        print(f"  Avg fitness: {sum(fitnesses)/len(fitnesses):.2f}")
        print(f"  Min fitness: {min(fitnesses):.2f}")
        
        self.generation += 1


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

    def train(self, generations: int = 50, checkpoint = None, parallel = False, tournament = False):
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
        if tournament:
            print("Running tournament-style genome evaluation...")
            best_genome = p.run(self.eval_genomes_tournament, generations)
        elif parallel:
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
        trainer = NeatTraining(num_opponents=3, num_games=num_games)
        wins = 0
        valid_games = 0
        for i in range(num_games):
            best_genome, stats = trainer.run_game(genome_path, i, test=True)
            if best_genome != -1:
                valid_games += 1
                if best_genome == 0:
                    wins += 1
                penalties = stats.get('penalties', {})
                #print stats based on output


        if valid_games > 0:
            win_rate = wins / valid_games
            print(f"Test Results over {valid_games} valid games:")
            print(f"  Wins: {wins}")
            print(f"  Win Rate: {win_rate * 100:.2f}%")
            print(f" Penalties: {penalties}")
        else:
            print("No valid games were played during testing.")

                

def main():
    parser = argparse.ArgumentParser(description="Train NEAT agent for MonopolyAI")
    parser.add_argument('--train', action='store_true', help='Train the NEAT agent')
    parser.add_argument('--test', action='store_true', help='Test the NEAT agent against naive opponents')
    parser.add_argument('--generations', type=int, default=50, help='Number of generations')
    parser.add_argument('--opponents', type=int, default=3, help='Number of opponent agents')
    parser.add_argument('--num_games', type=int, default=20, help='Number of games per genome evaluation')
    parser.add_argument('--checkpoint', type=str, default=None, help='Path to checkpoint file')
    parser.add_argument('--genome', type=str, default='genomes/best_genome.pkl', help='Path to genome file for testing')
    parser.add_argument('--parallel', action='store_true', default=False, help='Use parallel evaluation of genomes')
    parser.add_argument('--tournament', action='store_true', default=False, help='Run tournament mode')
    args = parser.parse_args()

    if args.train:
        # Train NEAT agent (parallel for faster training)
        trainer = NeatTraining(num_opponents=args.opponents, num_games=args.num_games)
        trainer.train(generations=args.generations, checkpoint=args.checkpoint, parallel=args.parallel, tournament=args.tournament)
    elif args.test:
        # Test against naive opponents
        NeatTraining.test_genome(genome_path=args.genome, num_games=args.num_games)
    else:
        print("Use --train to train or --test to test an agent")
        print(f"Example: python {__file__} --train --generations 30 --num_games 5")
        print(f"Example: python {__file__} --test --genome genomes/best_genome.pkl")
        
if __name__ == "__main__":
    main()


