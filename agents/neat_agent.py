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
    
    def game_start(self, agent_index: int):
        self.agent_index = agent_index
    
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
            avg_opp_cash = sum(p['cash'] for p in opponents) / len(opponents) / 2000.0
            avg_opp_properties = sum(
                1 for prop in state['properties'] 
                if prop['owner_index'] != self.agent_index and prop['is_owned']
            ) / (28.0 * len(opponents))
            max_opp_cash = max(p['cash'] for p in opponents) / 2000.0
            
            features.extend([
                avg_opp_cash,
                avg_opp_properties,
                max_opp_cash,
                len([p for p in opponents if not p['retired']]) / 3.0,
            ])
        else:
            features.extend([0.0, 0.0, 0.0, 0.0])
        
        # Property availability
        unowned_properties = sum(1 for prop in state['properties'] if not prop['is_owned'])
        features.append(unowned_properties / 28.0)
        
        # Resource availability
        features.extend([
            state['houses_remaining'] / 32.0,
            state['hotels_remaining'] / 12.0,
        ])
        
        # Game progress indicator
        total_wealth = sum(p['cash'] for p in state['players'])
        our_wealth_ratio = agent_player['cash'] / max(total_wealth, 1)
        features.append(our_wealth_ratio)
        
        # Pad to fixed size of 100
        features = features[:100]
        while len(features) < 100:
            features.append(0.0)
        
        return np.array(features, dtype=np.float32)
    
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
        # Cash difference
        cash_gain = offer['offer_to']['cash'] - offer['offer_from']['cash']
        
        # Properties offered to you
        props_to = offer['offer_to'].get('properties', [])
        props_from= offer['offer_from'].get('properties', [])
        
        num_props_to = len(props_to)
        num_props_from = len(props_from)
        
        # Total value of properties
        prop_values = {p['property_id']: p['purchase_price'] for p in state['properties']}
        value_to = sum(prop_values.get(pid, 0) for pid in props_to)
        value_from = sum(prop_values.get(pid, 0) for pid in props_from)
        
        # Railroads/utilities offered
        railroads_to = sum(1 for pid in props_to if state['properties'][pid]['type'] == 2)
        utilities_to = sum(1 for pid in props_to if state['properties'][pid]['type'] == 1)
        
        # Monopoly completion 
        monopoly_potential = 0
        for pid in props_to:
            prop = state['properties'][pid]
            same_color_owned = sum(
                1 for p in state['properties']
                if p['colour_id'] == prop['colour_id'] and p['owner_index'] == self.agent_index
            )
            total_in_group = sum(
                1 for p in state['properties']
                if p['colour_id'] == prop['colour_id']
            )
            if same_color_owned + 1 == total_in_group:
                monopoly_potential += 1
        
        # Normalize and pad/truncate to fixed size
        features = [
            cash_gain / 2000.0,
            num_props_to / 5.0,
            num_props_from / 5.0,
            value_to / 5000.0,
            value_from / 5000.0,
            railroads_to / 4.0,
            utilities_to / 2.0,
            monopoly_potential, # higher weight -> dont normalize
        ]
        while len(features) < 10:
            features.append(0.0)
        return np.array(features, dtype=np.float32)
    
    def evaluate_property(self, state: Dict, property_id: int) -> float:
        # Evaluate property value for auction bidding
        prop = None
        for p in state['properties']:
            if p['property_id'] == property_id:
                prop = p
                break
        
        if not prop:
            return 0.0
        
        agent_player = state['players'][self.agent_index]
        
        # if can't afford, no value
        if agent_player['cash'] < prop['purchase_price'] * 1.2:
            return 0.0
        
        score = 0.5  # Base score
        
        # Prefer completing color groups
        same_color_owned = sum(
            1 for p in state['properties']
            if p['colour_id'] == prop['colour_id'] and p['owner_index'] == self.agent_index
        )
        score += same_color_owned * 0.2
        return score
    
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
        
        # Handle debt
        if state['owed'] > agent_player['cash']:
            # Find property to mortgage
            for prop in state['properties']:
                if (prop['is_owned'] and 
                    prop['owner_index'] == self.agent_index and 
                    not prop['mortgaged']):
                    return {
                        'action_type': 3,  # ACTION_MORTGAGE
                        'mortgage_property': prop['property_id']
                    }
            # No properties to mortgage - bankruptcy
            return {'action_type': 5}  # ACTION_END_TURN
        
        # Check if on unowned property
        current_position = agent_player['position']
        
        property_at_position = None
        for prop in state['properties']:
            if prop['position'] == current_position and not prop['is_owned']:
                property_at_position = prop
                break
        
        if property_at_position:
            # Decide whether to buy with neural network
            features = self.extract_features(state)
            output = self.net.activate(features)
            buy_score = output[0] + self.evaluate_property(state, property_at_position['property_id'])
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
        
        features = self.extract_features(state)
        output = self.net.activate(features)
        
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
        
        # Bid based on neural network output and property value
        property_value = self.evaluate_property(state, property_id)
        bid_multiplier = output[1] * property_value
        bid_amount = int(prop['purchase_price'] * bid_multiplier * 0.8)
        
        # Only bid if we can afford it and it's reasonable
        if bid_amount > 0 and bid_amount < agent_player['cash'] * 0.3:
            return {
                'action_type': 4,  # ACTION_AUCTION_BID
                'auction_bid': bid_amount
            }
        
        return {'action_type': 5}
    
    def trade_offer(self, state: Dict, offer: Dict) -> Dict:
        # Use neural network to evaluate trade
        features = self.extract_features(state)
        trade_features = self.extract_trade_features(state, offer)
        features = np.concatenate([features, trade_features])
        output = self.net.activate(features)
        
        # output[2] is trade acceptance threshold
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