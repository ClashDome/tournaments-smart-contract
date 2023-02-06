#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>


using namespace eosio;
using namespace std;

#define EOSIO eosio::name("eosio")
#define CONTRACTN eosio::name("clashdometrn")

CONTRACT clashdometrn : public contract {

   public:

      using contract::contract;

      ACTION initconfig();

      ACTION removeconfig();

      ACTION addconftoken(
         name token_contract, 
         symbol token_symbol,
         string type
      );

      ACTION createtrn(
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
      );

      ACTION canceltrn(
         name creator,
         uint64_t tournament_id
      );

      ACTION addcreator(
         name creator,
         string img,
         vector <extended_symbol> supported_tokens_fee,
         bool stake_available,
         vector <extended_symbol> supported_tokens_stake,
         bool nft_available,
         bool pot_available
      );

      ACTION logcreatetrn(
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
      );

   private:

      // TABLES 

      // tournaments
      TABLE tournaments_s {
        
         uint64_t tournament_id;
         name creator;
         string name;
         uint64_t game;
         uint64_t timestamp_start; 
         uint64_t timestamp_end;
         asset requeriment_fee;
         asset requeriment_stake;
         string requeriment_nft;
         asset prize_pot;
         string type_prize_pot;
         bool recreate;

         uint64_t primary_key() const { return tournament_id; }
         uint64_t by_creator() const { return creator.value; }
      };

      typedef multi_index<name("tournaments"), tournaments_s,
         indexed_by < name("bycreator"), const_mem_fun < tournaments_s, uint64_t, &tournaments_s::by_creator>>> 
      tournaments_t;
    
      tournaments_t tournaments = tournaments_t(get_self(), get_self().value);

      // creators
      TABLE creators_s {
        
         name creator;
         string img;
         vector <extended_symbol> supported_tokens_fee;
         bool stake_available;
         vector <extended_symbol> supported_tokens_stake;
         bool nft_available;
         bool pot_available;

         uint64_t primary_key() const { return creator.value; }
      };

      typedef multi_index<name("creators"), creators_s> creators_t;
    
      creators_t creators = creators_t(get_self(), get_self().value);

      // config
      TABLE config_s {
         uint64_t tournament_counter = 1;
         vector <extended_symbol> supported_tokens_fee = {};
         vector <extended_symbol> supported_tokens_stake = {};
      };

      typedef singleton <name("config"), config_s> config_t;
      config_t config = config_t(get_self(), get_self().value);
      typedef multi_index <name("config"), config_s> config_t_for_abi;

      // AUXILIAR FUNCTIONS

      void checkPendingTournament(
         name creator, 
         uint64_t timestamp_start, 
         uint64_t timestamp_end
      );

      void checkFeeAndStake(
         name creator, 
         asset requeriment_fee, 
         asset requeriment_stake
      );

      // VARIABLES

      // wax
      const string EOSIO_CONTRACT = "eosio.token";
      static constexpr symbol WAX_SYMBOL = symbol(symbol_code("WAX"), 8);

      // tlm
      const string ALIEN_WORLDS_CONTRACT = "alien.worlds";
      static constexpr symbol TLM_SYMBOL = symbol(symbol_code("TLM"), 4);

      // timestamps (hours)
      const uint64_t MAX_DURATION = 168; // 7 day * 24 hours
      const uint64_t MIN_DURATION = 1; // 1 hour

      // games
      enum GameType {CANDY_FIESTA = 1, TEMPLOK, RINGY_DINGY, ENDLESS_SIEGE_2, RUG_POOL, PAC_MAN};
};