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
        self.neat_config = None

        # Load NEAT config if provided
        if 'config_path' in self.config:
            self.neat_config = neat.Config(
                neat.DefaultGenome,
                neat.DefaultReproduction,
                neat.DefaultSpeciesSet,
                neat.DefaultStagnation,
                self.config['config_path']
            )

        # Load genome if provided in config
        if 'genome_path' in self.config and self.config.get('config_path'):
            # Normal case: load genome from file
            with open(self.config['genome_path'], 'rb') as f:
                self.genome = pickle.load(f)
            self.net = neat.nn.FeedForwardNetwork.create(self.genome, self.neat_config)
        else:
            # Fallback: no genome provided
            self.random_net()

    def random_net(self):
        if not self.config.get('config_path'):
            raise RuntimeError("No NEAT config path provided for random init")

        # Create a fresh genome with random weights
        genome = neat.DefaultGenome(0)
        genome.configure_new(self.neat_config.genome_config)
        self.genome = genome

        # Build the network
        self.net = neat.nn.FeedForwardNetwork.create(self.genome, self.neat_config)

    
    def game_start(self, agent_index: int, seed: int):
        self.agent_index = agent_index
        self.seed = seed
    
    def extract_features(self, state: Dict, trade_offer: Optional[Dict] = None) -> np.ndarray:
        """
        Extract comprehensive game state features.
        Returns 70-dimensional feature vector.
        Includes optional trade offer features.
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
            return np.zeros(70)
        
        # Player features
        features.extend([
            self.agent_index / 3.0,  # For 4 players: indices 0,1,2,3 → 0.0, 0.33, 0.67, 1.0
            agent_player['cash'] / 2000.0,  
            agent_player['position'] / 40.0,  
            1.0 if agent_player['in_jail'] else 0.0,
            agent_player['turns_in_jail'] / 3.0,
            agent_player['jail_free_cards'] / 2.0,
            agent_player['railroads_owned'] / 4.0,
            agent_player['utilities_owned'] / 2.0,
        ])
        
        # Property ownership by color
        color_counts = [0] * 9
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
        
        # Development features
        features.extend([
            total_houses / 32.0,
            total_hotels / 12.0,
            total_property_value / 10000.0,
        ])
        
        # Opponent features
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
        
        # Game state features
        unowned_properties = sum(1 for prop in state['properties'] if not prop['is_owned'])
        total_wealth = sum(p['cash'] for p in state['players'])
        wealth_ratio = agent_player['cash'] / max(total_wealth, 1)
        
        features.extend([
            unowned_properties / 28.0,
            state['houses_remaining'] / 32.0,
            state['hotels_remaining'] / 12.0,
            wealth_ratio,
            state['owed'] / 2000.0
        ])
        
        # Position Property features - property at agent's position 
        property_at_position = None
        for prop in state['properties']:
            if prop['position'] == agent_player['position']:
                property_at_position = prop
                break
        
        if property_at_position:
            same_colour_owned = sum(
                1 for p in state['properties']
                if p['is_owned'] and p['colour_id'] == property_at_position['colour_id'] 
                and p['owner_index'] == self.agent_index
            )
            total_in_group = sum(
                1 for p in state['properties']
                if p['colour_id'] == property_at_position['colour_id']
            )
            
            features.extend([
                property_at_position['purchase_price'] / 400.0,
                property_at_position['colour_id'] / 8.0,
                1.0 if property_at_position['type'] == 0 else 0.0,  # Property
                1.0 if property_at_position['type'] == 1 else 0.0,  # Utility
                1.0 if property_at_position['type'] == 2 else 0.0,  # Railroad
                1.0 if property_at_position['is_owned'] else 0.0,
                1.0 if property_at_position.get('owner_index', -1) == self.agent_index else 0.0,
                same_colour_owned / max(total_in_group, 1),
                (agent_player['cash'] - property_at_position['purchase_price']) / 2000.0,
                property_at_position['current_rent'] / 2000.0,
                property_at_position.get('rent0', 0) / 2000.0,
                property_at_position.get('rent1', 0) / 2000.0,
                property_at_position.get('rent2', 0) / 2000.0,
                property_at_position.get('rent3', 0) / 2000.0,
                property_at_position.get('rent4', 0) / 2000.0,
                property_at_position.get('rentH', 0) / 2000.0,
                property_at_position.get('houses', 0) / 4.0,
                1.0 if property_at_position.get('hotel', False) else 0.0,
                1.0 if property_at_position.get('mortgaged', False) else 0.0,
                1.0 if property_at_position.get('is_monopoly', False) else 0.0,
                1.0 if property_at_position.get('auctioned_this_turn', False) else 0.0,
                property_at_position.get('house_price', 0) / 200.0,
            ])
        else:
            features.extend([0.0] * 22)
        
        # Trade offer features - only populated when evaluating a trade
        if trade_offer:
            cash_gain = trade_offer['offer_to']['cash'] - trade_offer['offer_from']['cash']
            
            props_to = trade_offer['offer_to'].get('property_ids', [])
            props_from = trade_offer['offer_from'].get('property_ids', [])
            
            # Calculate property values
            prop_values = {p['property_id']: p['purchase_price'] for p in state['properties']}
            value_to = sum(prop_values.get(pid, 0) for pid in props_to)
            value_from = sum(prop_values.get(pid, 0) for pid in props_from)
            
            # Property types in trade
            props_by_id = {p['property_id']: p for p in state['properties']}
            railroads_to = sum(1 for pid in props_to if props_by_id.get(pid, {}).get('type') == 2)
            utilities_to = sum(1 for pid in props_to if props_by_id.get(pid, {}).get('type') == 1)
            
            # Monopoly completion potential
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
            
            features.extend([
                cash_gain / 2000.0,
                len(props_to) / 5.0,
                len(props_from) / 5.0,
                value_to / 5000.0,
                value_from / 5000.0,
                (value_to - value_from) / 5000.0,  # Net value
                railroads_to / 4.0,
                utilities_to / 2.0,
                monopoly_potential / 3.0,
                trade_offer['offer_to'].get('jail_cards', 0) / 2.0,
                trade_offer['offer_from'].get('jail_cards', 0) / 2.0,
                1.0 if cash_gain > 0 else 0.0,  # Receiving cash
                1.0 if len(props_to) > len(props_from) else 0.0,  # Receiving more properties
                1.0 if monopoly_potential > 0 else 0.0,  # Completes monopoly
                (agent_player['cash'] + cash_gain) / 2000.0,  # Cash after trade
            ])
        else:
            features.extend([0.0] * 15)
        
        # House Building features
        monopoly_count = 0
        total_monopoly_value = 0
        developable_properties = 0
        max_houses_on_monopoly = 0
        
        color_groups = {}
        for prop in state['properties']:
            color = prop['colour_id']
            if color not in color_groups:
                color_groups[color] = {'total': 0, 'ours': 0, 'props': []}
            color_groups[color]['total'] += 1
            color_groups[color]['props'].append(prop)
            if prop['is_owned'] and prop['owner_index'] == self.agent_index:
                color_groups[color]['ours'] += 1
        
        for color_id, info in color_groups.items():
            if info['ours'] == info['total']:  # We have a monopoly
                monopoly_count += 1
                for prop in info['props']:
                    total_monopoly_value += prop['purchase_price']
                    if prop['houses'] < 5:
                        developable_properties += 1
                    max_houses_on_monopoly = max(max_houses_on_monopoly, prop['houses'])
        
        features.extend([
            monopoly_count / 8.0,
            total_monopoly_value / 10000.0,
            developable_properties / 12.0,
            max_houses_on_monopoly / 5.0,
            1.0 if state['houses_remaining'] > 0 else 0.0,
        ])
        while len(features) < 70:
            features.append(0.0)
        
        return np.array(features[:70], dtype=np.float32)
    

    def construct_trade_offer(self, state: Dict, high_value_properties: List[Dict]) -> Optional[Dict]:
        agent_player = state['players'][self.agent_index]

        # Group by owner
        offers_by_owner = {}
        our_properties = []
        for prop in high_value_properties:
            owner = prop['owner']
            if owner == self.agent_index:
                our_properties.append(prop)
            elif owner is not None:
                if owner not in offers_by_owner:
                    offers_by_owner[owner] = []
                offers_by_owner[owner].append(prop)
        
        if not offers_by_owner or not our_properties:
            return None
        
        # Find highest average score of trade partners
        best_opponent = None
        best_avg_score = 0.0

        for owner_id, props in offers_by_owner.items():
            avg_score = sum(p['score'] for p in props) / len(props)
            if avg_score > best_avg_score:
                best_avg_score = avg_score
                best_opponent = owner_id
        
        if best_opponent is None:
            return None
        
        # Create trade offer
        our_property_ids = [p['property_id'] for p in our_properties]
        their_property_ids = [p['property_id'] for p in offers_by_owner[best_opponent]]
        trade_offer = {
            'action_type': 1,  # ACTION_TRADE
            'trade_offer': {
                'prop_player_index': best_opponent,
                'offer_from': {
                    'property_ids': our_property_ids,
                    'cash': 0,
                    'jail_cards': 0
                },
                'offer_to': {
                    'property_ids': their_property_ids,
                    'cash': 0,
                    'jail_cards': 0
                }
            }
        }
        
        return trade_offer

    def agent_turn(self, state) -> Dict:
        agent_player = state['players'][self.agent_index]
        
        features = self.extract_features(state)
        output = self.net.activate(features)
        ''' 
        output[0] -> buy property score, 
        output[1] -> bid score, 
        output[2] -> trade acceptance score, 
        output[3] = jail use card, output[4] = pay fine, output[5] = roll doubles,
        output[6] = trade proposal score, 
        output[7] = build houses score, 
        output[8-35] = property scores

        '''
        highest_score = 0.0
        for score in output[0:8]:
            if score > highest_score:
                highest_score = score
        
        # Jail Decisions:
        if highest_score == output[3] and output[3] > 0.5:
            return {'action_type': 10}  # ACTION_USE_JAIL_CARD
        elif highest_score == output[4] and output[4] > 0.5:
            return {'action_type': 9}  # ACTION_PAY_JAIL_FINE
        elif highest_score == output[5] and output[5] > 0.5:
            return {'action_type': 11}  # ACTION_JAIL_ROLL_DOUBLE

        # Buying Property
        if highest_score == output[0] and output[0] > 0.5:
            #print("Deciding to buy property with score:", output[0])
            return{
                'action_type': 0,  # ACTION_LANDED_PROPERTY
                'buying_property': True
            }
        elif highest_score == output[0] and output[0] <= 0.5:
            print("Deciding not to buy property with score:", output[0])
            return{
                'action_type': 0,  # ACTION_LANDED_PROPERTY
                'buying_property': False
            }

        # Building Houses:
        if highest_score == output[7] and output[7] > 0.7:
            #print("Deciding to build houses with score:", output[7])
            return {
                'action_type': 6,  # ACTION_BUILD_HOUSES
                'property_position': np.argmax(output[8:36])  # Choose property with highest score
            }

        # Trade Proposal
        if highest_score == output[6] and highest_score > 0.5:
            property_outputs = output[8:36]
            high_value_properties = []
            for i, score in enumerate(property_outputs):
                if score > 0.75 and i < len(state['properties']):
                    prop = state['properties'][i]
                    high_value_properties.append({
                        'property_id': prop['property_id'],
                        'position': prop['position'],
                        'owner': prop['owner_index'] if prop['is_owned'] else None,
                        'score': score
                    })
            if high_value_properties:
                trade_offer = self.construct_trade_offer(state, high_value_properties)
                if trade_offer:
                    print("Proposing trade offer:", trade_offer)
                    return trade_offer
        
        return {'action_type': 8 } # ACTION_END_TURN


        '''
        # Handle jail
        if agent_player['in_jail']:
            features = np.concatenate([self.extract_features(state), self.extract_jail_features(state)])
            output = self.net.activate(features)
            # Outputs [3-5] for jail decisions
            use_card_score = output[3] if len(output) > 3 else 0.0
            pay_fine_score = output[4] if len(output) > 4 else 0.0
            roll_doubles_score = output[5] if len(output) > 5 else 0.0
            
            # Choose best option
            if agent_player['jail_free_cards'] > 0 and use_card_score > pay_fine_score and use_card_score > roll_doubles_score:
                return {'action_type': 10}  # ACTION_USE_JAIL_CARD
            elif agent_player['cash'] >= 50 and pay_fine_score > roll_doubles_score:
                return {'action_type': 9}  # ACTION_PAY_JAIL_FINE
            else:
                return {'action_type': 11}  # ACTION_JAIL_ROLL_DOUBLE
            
        # Check if on unowned property
        property_at_position = None
        for prop in state['properties']:
            if prop['position'] == agent_player['position'] and not prop['is_owned']:
                property_at_position = prop
                #print("Landed on unowned property:", property_at_position['property_id'])
                break
        
        if property_at_position:
            # Decide whether to buy with neural network
            features = np.concatenate([self.extract_features(state), self.extract_property_features(state, property_at_position['property_id'])])
            output = self.net.activate(features)
            buy_score = output[0]
            if buy_score > 0.5 and agent_player['cash'] > property_at_position['purchase_price']:
                #print("Deciding to buy property:", property_at_position['property_id'], "with score:", buy_score)
                return {
                    'action_type': 0,  # ACTION_LANDED_PROPERTY
                    'buying_property': True
                }
            else:
                #print("Deciding not to buy property:", property_at_position['property_id'], "with score:", buy_score)
                return {
                    'action_type': 0,
                    'buying_property': False
                }
            
        # Trade Proposal: Outputs [6-33]
        features = np.concatenate([self.extract_features(state), self.extract_trade_proposal_features(state)])
        output = self.net.activate(features)
        trade_outputs = output[6:33]
        if len(trade_outputs) == 28:
            high_value_properties = []
            for i, score in enumerate(trade_outputs):
                if score > 0.75 and i < len(state['properties']):
                    prop = state['properties'][i]
                    high_value_properties.append({
                        'property_id': prop['property_id'],
                        'position': prop['position'],
                        'owner': prop['owner_index'] if prop['is_owned'] else None,
                        'score': score
                    })
            
            if high_value_properties:
                trade_offer = self.construct_trade_offer(state, high_value_properties)
                if trade_offer:
                    print("Proposing trade offer:", trade_offer)
                    return trade_offer
                    '''
            
    def auction(self, state: Dict, auction: Dict) -> Dict:
        agent_player = state['players'][self.agent_index]
        property_id = auction['property_id']

        # Find the property
        prop = None
        for p in state['properties']:
            if p['property_id'] == property_id:
                prop = p
                break
        
        if not prop:
            return {'action_type': 8}  # END_TURN (no bid)
        
        features = self.extract_features(state)
        output = self.net.activate(features)
        
        # Bid based on neural network output and property value (output[1])
        bid_multiplier = output[1]
        max_bid = int(prop['purchase_price'] * bid_multiplier * 0.8)
        cash_limit = int(agent_player['cash'] * 0.3)
        bid = min(max_bid, cash_limit)
        if bid >= 10 and bid_multiplier > 0.3:
            #print("Bidding", bid, "on property", property_id)
            return {
                'action_type': 7,  # ACTION_AUCTION_BID
                'auction_bid': bid
            }

        return {'action_type': 8} # ACTION_END_TURN
    
    def trade_offer(self, state: Dict, offer: Dict) -> Dict:
        # Respond to trade offer
        # Use neural network to evaluate trade

        features = self.extract_features(state, trade_offer=offer)
        output = self.net.activate(features)
        
        # output[2] -> trade acceptance score
        cash_gain = offer['offer_to']['cash'] - offer['offer_from']['cash']
        accept = output[2] > 0.7 and cash_gain > 0
        print("Trade offer decision with acceptance score:", output[2], "cash gain:", cash_gain, "accept:", accept)
        return {
            'action_type': 2,  # ACTION_TRADE_RESPONSE
            'trade_response': accept
        }