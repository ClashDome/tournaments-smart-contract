#include <clashdometrn.hpp>

/**
*  Initializes the config table. Only needs to be called once when first deploying the contract
*  @required_auth The contract itself
*/
ACTION clashdometrn::initconfig() 
{

   require_auth(get_self());

   config.get_or_create(get_self(), config_s{});
}

/**
*  Remove the config table
*  @required_auth The contract itself
*/
ACTION clashdometrn::removeconfig() 
{

   require_auth(get_self());

   config.remove();
}

/**
*  Adds a token that can then be used as fee or stake in tournaments
*  @required_auth The contract itself
*/
ACTION clashdometrn::addconftoken(
   name token_contract, 
   symbol token_symbol,
   string type) 
{
   require_auth(get_self());

   config_s current_config = config.get();

   if (type == "fee") {
      for (extended_symbol token : current_config.supported_tokens_fee) {
         check(token.get_symbol() != token_symbol, "A token with this symbol is already supported");
      }
      current_config.supported_tokens_fee.push_back(extended_symbol(token_symbol, token_contract));
   } else {
      for (extended_symbol token : current_config.supported_tokens_stake) {
         check(token.get_symbol() != token_symbol, "A token with this symbol is already supported");
      }
      current_config.supported_tokens_stake.push_back(extended_symbol(token_symbol, token_contract));
   }
   
   config.set(current_config, get_self());
}

/**
*  Creates a new tournament
*  @required_auth The tournament creator
*/
ACTION clashdometrn::createtrn(
   name creator,
   string name,
   uint64_t game,
   uint64_t timestamp_start, 
   uint64_t timestamp_end, 
   asset requeriment_fee,
   asset requeriment_stake,
   string requeriment_nft,
   asset prize_pot,
   string type_prize_pot,
   bool recreate
) {
   
   require_auth(creator);
   
   // check timestamps
   uint64_t current_timestamp = eosio::current_time_point().sec_since_epoch();

   check(game <= 5, "Invalid game ID.");

   check(timestamp_start >= current_timestamp, "The start time must be later than the current time.");
   check(timestamp_start < timestamp_end, "The end time must be later than the start time.");

   float duration = ((float)timestamp_end - (float)timestamp_start) / 3600.0;

   check(duration >= (float) MIN_DURATION && duration <= (float) MAX_DURATION, "Duration must be between " + to_string(MIN_DURATION) + " and " + to_string(MAX_DURATION) + " hours.");

   // check tournaments in progress
   checkPendingTournament(creator, timestamp_start, timestamp_end, game);

   // check fee and stake symbols
   checkFeeAndStake(creator, requeriment_fee, requeriment_stake);

   // check prizepot
   check(requeriment_fee.amount == 0 || requeriment_fee.symbol == prize_pot.symbol, "Entry fee and prize pot symbols are different");
   check(requeriment_fee.amount == 0 || type_prize_pot == POT_MINIMUM || type_prize_pot == POT_BONUS, "Invalid prize pot type.");

   auto cr_itr = creators.find(creator.value);

   // TODO: remove this after add permissions to create tournament with nft
   check(cr_itr != creators.end(), "Invalid creator username");

   // check requeriment stake, nfts y pot
   if (cr_itr == creators.end()) {
      check(requeriment_stake.amount == 0, "Requeriment stake is not allowed.");
      check(prize_pot.amount == 0, "Prize pot is not allowed.");
      check(requeriment_nft == "", "Requeriment NFT is not allowed.");
   } else {
      check(cr_itr->stake_available || requeriment_stake.amount == 0, "Requeriment stake is not allowed.");
      check(cr_itr->pot_available || prize_pot.amount == 0, "Prize pot is not allowed.");
      check(cr_itr->nft_available || requeriment_nft == "", "Requeriment NFT is not allowed.");
   }

   // check creator funds
   creators.modify(cr_itr, CONTRACTN, [&](auto& cr) {
      auto fund_itr = find_if(cr.funds.begin(), cr.funds.end(),[&](const asset& a) { 
         return a.symbol == prize_pot.symbol; 
      });

      check(fund_itr != cr.funds.end() && fund_itr->amount >= prize_pot.amount, "Insufficient funds.");
      *fund_itr -= prize_pot;
   });

   config_s current_config = config.get();

   uint64_t tournament_id = current_config.tournament_counter++;
   config.set(current_config, get_self());

   tournaments.emplace(CONTRACTN, [&](auto& trn) {
      trn.tournament_id = tournament_id;
      trn.creator = creator;
      trn.name = name;
      trn.game = game;
      trn.timestamp_start = timestamp_start;
      trn.timestamp_end = timestamp_end;
      trn.requeriment_fee = requeriment_fee;
      trn.requeriment_stake = requeriment_stake;
      trn.requeriment_nft = requeriment_nft;
      trn.prize_pot = prize_pot;
      trn.type_prize_pot = type_prize_pot;
      trn.recreate = recreate;
   });
}

/**
*  Edits a created tournament
*  @required_auth The tournament creator
*/
ACTION clashdometrn::edittrn(
   uint64_t id,
   name creator,
   string name,
   uint64_t game,
   uint64_t timestamp_start, 
   uint64_t timestamp_end, 
   asset requeriment_fee,
   asset requeriment_stake,
   string requeriment_nft,
   asset prize_pot,
   string type_prize_pot,
   bool recreate
) {
   
   require_auth(creator);

   check(game <= 5, "Invalid game ID.");

   auto trn_itr = tournaments.require_find(id, "No tournament with this id exists");
   check(trn_itr->creator == creator, "The specified account isn't the creator of the tournament.");
   
   // check timestamps
   uint64_t current_timestamp = eosio::current_time_point().sec_since_epoch();

   check(current_timestamp < trn_itr->timestamp_start, "You cannot cancel an initialized tournament.");

   check(timestamp_start >= current_timestamp, "The start time must be later than the current time.");
   check(timestamp_start < timestamp_end, "The end time must be later than the start time.");

   float duration = ((float)timestamp_end - (float)timestamp_start) / 3600.0;

   check(duration >= (float) MIN_DURATION && duration <= (float) MAX_DURATION, "Duration must be between " + to_string(MIN_DURATION) + " and " + to_string(MAX_DURATION) + " hours.");

   // check tournaments in progress
   checkPendingTournament(creator, timestamp_start, timestamp_end, game);

   // check fee and stake symbols
   checkFeeAndStake(creator, requeriment_fee, requeriment_stake);

   // check prizepot
   check(requeriment_fee.amount == 0 || requeriment_fee.symbol == prize_pot.symbol, "Entry fee and prize pot symbols are different");
   check(requeriment_fee.amount == 0 || type_prize_pot == POT_MINIMUM || type_prize_pot == POT_BONUS, "Invalid prize pot type.");

   auto cr_itr = creators.find(creator.value);

   // check requeriment stake, nfts y pot
   if (cr_itr == creators.end()) {
      check(requeriment_stake.amount == 0, "Requeriment stake is not allowed.");
      check(prize_pot.amount == 0, "Prize pot is not allowed.");
      check(requeriment_nft == "", "Requeriment NFT is not allowed.");
   } else {
      check(cr_itr->stake_available || requeriment_stake.amount == 0, "Requeriment stake is not allowed.");
      check(cr_itr->pot_available || prize_pot.amount == 0, "Prize pot is not allowed.");
      check(cr_itr->nft_available || requeriment_nft == "", "Requeriment NFT is not allowed.");
   }

   // add creator funds
   creators.modify(cr_itr, CONTRACTN, [&](auto& cr) {
      auto fund_itr = find_if(cr.funds.begin(), cr.funds.end(),[&](const asset& a) { 
         return a.symbol == trn_itr->prize_pot.symbol; 
      });

      check(fund_itr != cr.funds.end() && fund_itr->amount >= trn_itr->prize_pot.amount, "Insufficient funds.");
      *fund_itr += trn_itr->prize_pot;
   });
   
   // check creator funds
   creators.modify(cr_itr, CONTRACTN, [&](auto& cr) {
      auto fund_itr = find_if(cr.funds.begin(), cr.funds.end(),[&](const asset& a) { 
         return a.symbol == prize_pot.symbol; 
      });

      check(fund_itr != cr.funds.end() && fund_itr->amount >= prize_pot.amount, "Insufficient funds.");
      *fund_itr -= prize_pot;
   });

   creators.modify(cr_itr, CONTRACTN, [&](auto& cr) {
      auto fund_itr = find_if(cr.funds.begin(), cr.funds.end(),[&](const asset& a) { 
         return a.symbol == prize_pot.symbol; 
      });

      check(fund_itr != cr.funds.end() && fund_itr->amount >= prize_pot.amount, "Insufficient funds.");
      *fund_itr -= prize_pot;
   });

   tournaments.modify(trn_itr, CONTRACTN, [&](auto& trn) {
      trn.creator = creator;
      trn.name = name;
      trn.game = game;
      trn.timestamp_start = timestamp_start;
      trn.timestamp_end = timestamp_end;
      trn.requeriment_fee = requeriment_fee;
      trn.requeriment_stake = requeriment_stake;
      trn.requeriment_nft = requeriment_nft;
      trn.prize_pot = prize_pot;
      trn.type_prize_pot = type_prize_pot;
      trn.recreate = recreate;
   });
}

/**
*  Adds a token that can then be used in tournaments
*  @required_auth The tournament creator
*/
ACTION clashdometrn::canceltrn(
   name creator,
   uint64_t tournament_id
)
{

   require_auth(creator);

   auto trn_itr = tournaments.require_find(tournament_id, "No tournament with this id exists");
   check(trn_itr->creator == creator, "The specified account isn't the creator of the tournament.");

   uint64_t current_timestamp = eosio::current_time_point().sec_since_epoch();

   check(current_timestamp < trn_itr->timestamp_start, "You cannot cancel an initialized tournament.");

   auto cr_itr = creators.find(creator.value);

   // add creator funds
   creators.modify(cr_itr, CONTRACTN, [&](auto& cr) {
      auto fund_itr = find_if(cr.funds.begin(), cr.funds.end(),[&](const asset& a) { 
         return a.symbol == trn_itr->prize_pot.symbol; 
      });
      
      *fund_itr += trn_itr->prize_pot;
   });

   tournaments.erase(trn_itr);
}

/**
*  Adds a token that can then be used in tournaments
*  @required_auth The tournament creator
*/
ACTION clashdometrn::fcanceltrn(
   name creator,
   uint64_t tournament_id
)
{

   require_auth(get_self());

   auto trn_itr = tournaments.require_find(tournament_id, "No tournament with this id exists");
   check(trn_itr->creator == creator, "The specified account isn't the creator of the tournament.");

   tournaments.erase(trn_itr);
}

/**
*  Adds a token that can then be used in tournaments
*  @required_auth The tournament creator
*/
ACTION clashdometrn::rmusertrn(
   name creator,
   name account,
   uint64_t tournament_id,
   string type
)
{

   require_auth(creator);

   auto check_itr = checks.find(creator.value);

   uint64_t current_timestamp = eosio::current_time_point().sec_since_epoch();
   
   if (check_itr == checks.end()) {
      checks.emplace(CONTRACTN, [&](auto& check) {
         check.creator = creator;
         check.account = account;
         check.tournament_id = tournament_id;
         check.timestamp = current_timestamp;
         check.type = type;
      });
   } else {
      checks.modify(check_itr, CONTRACTN, [&](auto& check) {
         check.account = account;
         check.tournament_id = tournament_id;
         check.timestamp = current_timestamp;
         check.type = type;
      });
   }
}

/**
*  Adds a special creator
*  @required_auth self
*/
ACTION clashdometrn::addcreator(
   name creator,
   string img,
   vector <extended_symbol> supported_tokens_fee,
   bool stake_available,
   vector <extended_symbol> supported_tokens_stake,
   bool nft_available,
   bool pot_available
)
{

   require_auth(get_self());

   check(creators.find(creator.value) == creators.end(), "Creator " + creator.to_string() + " already exists.");

   creators.emplace(CONTRACTN, [&](auto& crt) {
      crt.creator = creator;
      crt.img = img;
      crt.supported_tokens_fee = supported_tokens_fee;
      crt.stake_available = stake_available;
      crt.supported_tokens_stake = supported_tokens_stake;
      crt.nft_available = nft_available;
      crt.pot_available = pot_available;
   });
}

/**
*  Removes a special creator
*  @required_auth self
*/
ACTION clashdometrn::rmcreator(
   name creator
)
{

   require_auth(get_self());

   auto cr_itr = creators.find(creator.value);

   check(cr_itr != creators.end(), "Creator " + creator.to_string() + " doesn't exists.");

   creators.erase(cr_itr);
}

void clashdometrn::addtrnfunds(
   asset quantity,
   name contract
) {

   require_auth(get_self());

   auto cr_itr = creators.find(get_self().value);

   check(cr_itr != creators.end(), "Invalid creator " + get_self().to_string());

   config_s current_config = config.get();

   bool is_fee_supported = false;
   
   for (extended_symbol supported_token_fee : current_config.supported_tokens_fee) {
      if (supported_token_fee.get_symbol() == quantity.symbol && supported_token_fee.get_contract() == contract) {
         is_fee_supported = true;
         break;
      }
   }

   for (extended_symbol supported_token_fee : cr_itr->supported_tokens_fee) {
      if (supported_token_fee.get_symbol() == quantity.symbol && supported_token_fee.get_contract() == contract) {
         is_fee_supported = true;
         break;
      }
   }

   check (is_fee_supported, "Token not supported.");

   creators.modify(cr_itr, CONTRACTN, [&](auto& cr) {
      auto fund_itr = find_if(cr.funds.begin(), cr.funds.end(),[&](const asset& a) { 
         return a.symbol == quantity.symbol; 
      });
      if (fund_itr != cr.funds.end()) {
         *fund_itr += quantity;
      } else {
         cr.funds.emplace_back(quantity);
      }
   });

   // TODO: send to clashdomepay
   action(
      permission_level{get_self(), name("active")},
      contract,
      name("transfer"),
      std::make_tuple(
         get_self(),
         name("clashdomepay"),
         quantity,
         "Add funds from " + get_self().to_string()
      )
   ).send();
}

// LOG ACTIONS

ACTION clashdometrn::logcreatetrn(
   uint64_t tournament_id, 
   name creator,
   string name,
   uint64_t game,
   uint64_t timestamp_start, 
   uint64_t timestamp_end,
   asset requeriment_fee,
   asset requeriment_stake,
   string requeriment_nft,
   asset prize_pot,
   string type_prize_pot,
   bool recreate
) {
   
   require_auth(get_self());
}

// NOTIFY
void clashdometrn::receive_nft(
   name from,
   name to,
   vector <uint64_t> asset_ids,
   string memo
) {
   return;
}

void clashdometrn::receive_transfer(
   name from,
   name to,
   asset quantity,
   string memo
) {

   if (to != get_self() || memo == "add-tokens") {
      return;
   }

   name contract = get_first_receiver();

   check(memo == "add-funds", "Memo has to be 'add-funds'.");

   auto cr_itr = creators.find(from.value);

   check(cr_itr != creators.end(), "Invalid creator " + from.to_string());

   config_s current_config = config.get();

   bool is_fee_supported = false;
   
   for (extended_symbol supported_token_fee : current_config.supported_tokens_fee) {
      if (supported_token_fee.get_symbol() == quantity.symbol && supported_token_fee.get_contract() == contract) {
         is_fee_supported = true;
         break;
      }
   }

   for (extended_symbol supported_token_fee : cr_itr->supported_tokens_fee) {
      if (supported_token_fee.get_symbol() == quantity.symbol && supported_token_fee.get_contract() == contract) {
         is_fee_supported = true;
         break;
      }
   }

   check (is_fee_supported, "Token not supported.");

   creators.modify(cr_itr, CONTRACTN, [&](auto& cr) {
      auto fund_itr = find_if(cr.funds.begin(), cr.funds.end(),[&](const asset& a) { 
         return a.symbol == quantity.symbol; 
      });
      if (fund_itr != cr.funds.end()) {
         *fund_itr += quantity;
      } else {
         cr.funds.emplace_back(quantity);
      }
   });

   // TODO: send to clashdomepay
   action(
      permission_level{get_self(), name("active")},
      contract,
      name("transfer"),
      std::make_tuple(
         get_self(),
         name("clashdomepay"),
         quantity,
         "Add funds from " + from.to_string()
      )
   ).send();

}

// AUXILIAR FUNCTIONS
void clashdometrn::checkPendingTournament(name creator, uint64_t timestamp_start, uint64_t timestamp_end, uint64_t game)
{

   auto trn_idx = tournaments.get_index<name("bycreator")>();
   auto trn_itr = trn_idx.lower_bound(creator.value);

   while (trn_itr != trn_idx.end() && trn_itr->creator == creator) {
      check(trn_itr->timestamp_end < timestamp_start || trn_itr->timestamp_start > timestamp_end || trn_itr->game != game, "Two tournaments at same time are not allowed.");
      trn_itr++;
   }
}


void clashdometrn::checkFeeAndStake(name creator, asset requeriment_fee, asset requeriment_stake)
{
   config_s current_config = config.get();

   bool is_fee_supported = false;
   
   for (extended_symbol supported_token_fee : current_config.supported_tokens_fee) {
      if (supported_token_fee.get_symbol() == requeriment_fee.symbol) {
         is_fee_supported = true;
         break;
      }
   }

   bool is_stake_supported = false;
   
   for (extended_symbol supported_token_stake : current_config.supported_tokens_stake) {
      if (supported_token_stake.get_symbol() == requeriment_stake.symbol) {
         is_stake_supported = true;
         break;
      }
   }

   auto cr_itr = creators.find(creator.value);

   if (cr_itr != creators.end()) {

      if (!is_fee_supported) {
         for (extended_symbol supported_token_fee : cr_itr->supported_tokens_fee) {
            if (supported_token_fee.get_symbol() == requeriment_fee.symbol) {
               is_fee_supported = true;
               break;
            }
         }
      }

      if (!is_stake_supported) {
         for (extended_symbol supported_token_stake : cr_itr->supported_tokens_stake) {
            if (supported_token_stake.get_symbol() == requeriment_stake.symbol) {
               is_stake_supported = true;
               break;
            }
         }
      }
   }

   check(is_fee_supported, "The specified stake symbol is not supported");
   check(is_stake_supported, "The specified stake symbol is not supported");
}