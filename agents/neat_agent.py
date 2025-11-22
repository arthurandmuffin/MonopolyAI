import neat
import numpy as np
import json
import pickle
from typing import Dict, List, Tuple, Optional

class NEATAgent:    
    def __init__(self, config_json: str):
        self.config = json.loads(config_json) if config_json else {}
        self.agent_index = 0
        self.genome = None
        self.net = None
        
        # Load genome if provided in config
        if 'genome_path' in self.config:
            with open(self.config['genome_path'], 'rb') as f:
                self.genome = pickle.load(f)
        
        # Load NEAT config if provided
        if 'neat_config_path' in self.config:
            self.neat_config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                self.config['neat_config_path']
            )
            if self.genome:
                self.net = neat.nn.FeedForwardNetwork.create(self.genome, self.neat_config)
    
    def game_start(self, agent_index: int, seed: int):
        self.agent_index = agent_index
        self.seed = seed
    
    def extract_features(self, state: Dict) -> np.ndarray:
        """
        Features include:
        - Player's own state (cash, position, properties owned, etc.)
        - Opponent states (aggregated)
        - Property ownership and development
        - Board position information
        """
        features = []
        
        # Find our player and opponents
        agent_player = None
        opponents = []
        for player in state['players']:
            if player['player_index'] == self.agent_index:
                agent_player = player
            else:
                opponents.append(player)
        
        if not agent_player:
            return np.zeros(100)  # Fallback
        
        # Player features (normalized)
        features.extend([
            agent_player['cash'] / 2000.0,  
            agent_player['position'] / 40.0,  
            1.0 if agent_player['in_jail'] else 0.0,
            agent_player['turns_in_jail'] / 3.0,
            agent_player['jail_free_cards'] / 2.0,
            agent_player['railroads_owned'] / 4.0,
            agent_player['utilities_owned'] / 2.0,
        ])
        
        # Count our properties by color (Normalized)
        color_counts = [0] * 9  # 9 color groups
        total_houses = 0
        total_hotels = 0
        total_property_value = 0
        
        for prop in state['properties']:
            if prop['owner_index'] == self.agent_index:
                color_counts[prop['colour_id']] += 1
                total_houses += prop['houses']
                total_hotels += 1 if prop['hotel'] else 0
                total_property_value += prop['purchase_price']
        
        features.extend([c / 3.0 for c in color_counts])  
        features.extend([
            total_houses / 32.0,
            total_hotels / 12.0,
            total_property_value / 10000.0,
        ])
        
        # Opponent features (aggregated)
        if opponents:
            avg_opp_cash = sum(p['cash'] for p in opponents) / len(opponents) 
            avg_opp_properties = sum(
                1 for prop in state['properties'] 
                if prop['owner_index'] != self.agent_index and prop['is_owned']
            ) / len(opponents)
            max_opp_cash = max(p['cash'] for p in opponents) 
            active_opponents = len([p for p in opponents if not p['retired']]) 
            
            features.extend([
                avg_opp_cash / 2000.0,
                avg_opp_properties / 28.0,
                max_opp_cash / 2000.0,
                active_opponents / 3.0,
            ])
        else:
            features.extend([0.0, 0.0, 0.0, 0.0])
        
        # Property availability
        unowned_properties = sum(1 for prop in state['properties'] if not prop['is_owned'])        
        
        # Game progress indicator
        total_wealth = sum(p['cash'] for p in state['players'])
        wealth_ratio = agent_player['cash'] / max(total_wealth, 1)
        
        features.extend([
            unowned_properties / 28.0,
            state['houses_remaining'] / 32.0,
            state['hotels_remaining'] / 12.0,
            wealth_ratio,
            state['owed'] / 2000.0
        ])
        # Pad to fixed size of 30
        while len(features) < 30:
            features.append(0.0)
        
        return np.array(features[:30], dtype=np.float32)
    
    def extract_trade_features(self, state: Dict, offer: Dict) -> np.ndarray:
        """
        Extract features specific to trade offers.
        Features include
        - Cash difference
        - Number of properties offered to each side
        - Total value of properties offered
        - Railroads/utilities offered
        - Monopoly completion potential
        """
        features = []

        # Cash difference
        cash_gain = offer['offer_to']['cash'] - offer['offer_from']['cash']
        
        # Properties offered
        props_to = offer['offer_to'].get('properties', [])
        props_from= offer['offer_from'].get('properties', [])
        
        num_props_to = len(props_to)
        num_props_from = len(props_from)
        
        # Total value of properties
        prop_values = {p['property_id']: p['purchase_price'] for p in state['properties']}
        value_to = sum(prop_values.get(pid, 0) for pid in props_to)
        value_from = sum(prop_values.get(pid, 0) for pid in props_from)
        
        # Railroads/utilities 
        props_by_id = {}
        for p in state['properties']:
            props_by_id[p['property_id']] = p

        railroads_to = sum(1 for pid in props_to if props_by_id.get(pid, {}).get('type') == 2)
        utilities_to = sum(1 for pid in props_to if props_by_id.get(pid, {}).get('type') == 1)
        
        # Monopoly completion 
        monopoly_potential = 0
        for pid in props_to:
            if pid not in props_by_id:
                continue
            prop = props_by_id[pid]
            same_color_owned = sum(
                1 for p in state['properties']
                if p['is_owned'] and p['colour_id'] == prop['colour_id']
                and p['owner_index'] == self.agent_index
            )
            total_in_color = sum(
                1 for p in state['properties']
                if p['colour_id'] == prop['colour_id']
            )
            if same_color_owned + 1 == total_in_color:
                monopoly_potential += 1
        
        # Normalize
        features.extend([
            cash_gain / 2000.0,
            num_props_to / 5.0,
            num_props_from / 5.0,
            value_to / 5000.0,
            value_from / 5000.0,
            railroads_to / 4.0,
            utilities_to / 2.0,
            monopoly_potential, # higher weight -> dont normalize
            offer['offer_to'].get('jail_cards', 0) / 2.0,
        ])
        while len(features) < 10:
            features.append(0.0)
        return np.array(features[:10], dtype=np.float32)
    
    def extract_property_features(self, state: Dict, property_id: int) -> np.ndarray:
        '''
        Extract features specific to a property.
        Features include:
        - Purchase price
        - Colour group
        - Type (property, utility, railroad)
        - Monopoly progress
        - Affordability
        - Rent potential
        '''
        features = []
        
        prop = None
        for p in state['properties']:
            if p['property_id'] == property_id:
                prop = p
                break
        
        if not prop:
            return np.zeros(10, dtype=np.float32)
        
        # Monopoly progress
        agent_player = state['players'][self.agent_index]
        same_colour_owned = sum(
            1 for p in state['properties']
            if p['is_owned'] and p['colour_id'] == prop['colour_id'] and p['owner_index'] == self.agent_index
        )
        total_in_group = sum(
            1 for p in state['properties']
            if p['colour_id'] == prop['colour_id']
        )
        monopoly_progress = same_colour_owned / total_in_group
        monopoly_complete = 1.0 if same_colour_owned == (total_in_group - 1)else 0.0

        # Affordability
        affordability = prop['purchase_price'] / max(agent_player['cash'], 1)
        cash_after_purchase = (agent_player['cash'] - prop['purchase_price']) 

        # Rent potential
        rent_potential = prop['base_rent'] / max(agent_player['cash'], 1)

        features.extend([
            prop['purchase_price'] / 400.0,
            prop['colour_id'] / 8.0,
            1.0 if prop['type'] == 0 else 0.0, # Property
            1.0 if prop['type'] == 1 else 0.0, # Utility
            1.0 if prop['type'] == 2 else 0.0, # Railroad
            monopoly_progress,
            monopoly_complete,
            affordability,
            cash_after_purchase / 2000.0,
            rent_potential,
        ])
        while len(features) < 10:
            features.append(0.0)
        return np.array(features[:10], dtype=np.float32)

    def agent_turn(self, state):
        # if no neural network, use heuristic
        if not self.net:
            return self.heuristic_turn(state)
        
        agent_player = state['players'][self.agent_index]
        
        # Handle jail
        if agent_player['in_jail']:
            if agent_player['jail_free_cards'] > 0:
                return {'action_type': 7}  # ACTION_USE_JAIL_CARD
            elif agent_player['cash'] >= 50:
                return {'action_type': 6}  # ACTION_PAY_JAIL_FINE
        
        # Check if on unowned property
        current_position = agent_player['position']
        
        property_at_position = None
        for prop in state['properties']:
            if prop['position'] == current_position and not prop['is_owned']:
                property_at_position = prop
                break
        
        if property_at_position:
            # Decide whether to buy with neural network
            base_features = self.extract_features(state)
            prop_features = self.extract_property_features(state, property_at_position['property_id'])
            features = np.concatenate([base_features, prop_features])
            output = self.net.activate(features)
            buy_score = output[0]
            if buy_score > 0.5 and agent_player['cash'] > property_at_position['purchase_price']:
                return {
                    'action_type': 0,  # ACTION_LANDED_PROPERTY
                    'buying_property': True
                }
            else:
                return {
                    'action_type': 0,
                    'buying_property': False
                }
        
        return {'action_type': 5} # ACTION_END_TURN
    
    def auction(self, state: Dict, auction: Dict) -> Dict:
        # if no neural network, use heuristic
        if not self.net:
            return self.heuristic_auction(state, auction)
        
        agent_player = state['players'][self.agent_index]
        property_id = auction['property_id']

        # Find the property
        prop = None
        for p in state['properties']:
            if p['property_id'] == property_id:
                prop = p
                break
        
        if not prop:
            return {'action_type': 5}  # END_TURN (no bid)
        
        features = np.concatenate([self.extract_features(state), self.extract_property_features(state, property_id)])
        output = self.net.activate(features)
        
        # Bid based on neural network output and property value
        bid_multiplier = output[1]
        max_bid = int(prop['purchase_price'] * bid_multiplier * 0.8)
        cash_limit = int(agent_player['cash'] * 0.3)
        bid = min(max_bid, cash_limit)

        if bid >= 10 and bid_multiplier > 0.3:
            return {
                'action_type': 4,  # ACTION_AUCTION_BID
                'auction_bid': bid
            }

        return {'action_type': 5}
    
    def trade_offer(self, state: Dict, offer: Dict) -> Dict:
        # if no neural network, use heuristic
        if not self.net:
            return self.heuristic_trade_offer(state, offer)
        
        # Use neural network to evaluate trade
        features = self.extract_features(state)
        trade_features = self.extract_trade_features(state, offer)
        features = np.concatenate([features, trade_features])
        output = self.net.activate(features)
        
        # output[2] -> trade acceptance score
        cash_gain = offer['offer_to']['cash'] - offer['offer_from']['cash']
        accept = output[2] > 0.5 and cash_gain > 0
        
        return {
            'action_type': 2,  # ACTION_TRADE_RESPONSE
            'trade_response': accept
        }
    
    def heuristic_turn(self, state: Dict) -> Dict:
        # Fallback heuristic when no network is available
        agent_player = state['players'][self.agent_index]
        current_position = agent_player['position']
        
        for prop in state['properties']:
            if prop['position'] == current_position and not prop['is_owned']:
                if agent_player['cash'] > prop['purchase_price'] * 1.5:
                    return {'action_type': 0, 'buying_property': True} # ACTION_LANDED_PROPERTY
                else:
                    return {'action_type': 0, 'buying_property': False} # ACTION_LANDED_PROPERTY
        
        return {'action_type': 5} # ACTION_END_TURN
    
    def heuristic_auction(self, state: Dict, auction: Dict) -> Dict:
        # Fallback auction heuristic
        agent_player = state['players'][self.agent_index]
        property_id = auction['property_id']
        
        for prop in state['properties']:
            if prop['property_id'] == property_id:
                if agent_player['cash'] > prop['purchase_price']:
                    return {
                        'action_type': 4, # ACTION_AUCTION_BID
                        'auction_bid': prop['purchase_price'] // 2
                    }
        
        return {'action_type': 5} # ACTION_END_TURN
    
    def heuristic_trade_offer(self, state: Dict, offer: Dict) -> Dict:
        # Fallback trade heuristic
        cash_gain = offer['offer_to']['cash'] - offer['offer_from']['cash']
        accept = cash_gain > 0
        
        return {
            'action_type': 2,  # ACTION_TRADE_RESPONSE
            'trade_response': accept
        }