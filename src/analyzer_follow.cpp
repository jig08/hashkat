#include <iostream>

#include "lcommon/perf_timer.h"

#include "analyzer.h"

using namespace std;

struct AnalyzerFollow {
    //** Note: Only use reference types here!!
    Network& network;
    ParsedConfig& config;
    //** Mind the StatE/StatS difference. They are entirely different structs.
    AnalysisState& state;
    AnalysisStats& stats;
    CategoryGrouper& follow_ranks;
    vector<double>& follow_probabilities, updating_follow_probabilities;

    EntityTypeVector& entity_types;
    MTwist& rng;
    // There are multiple 'Analyzer's, they each operate on parts of AnalysisState.
    AnalyzerFollow(AnalysisState& state) :
            network(state.network), state(state), stats(state.stats),
            config(state.config), follow_ranks(state.follow_ranks),
            follow_probabilities(state.follow_probabilities),
            updating_follow_probabilities(state.updating_follow_probabilities),
            entity_types(state.entity_types), rng(state.rng) {
    }

   /***************************************************************************
    * Entity mutation routines
    ***************************************************************************/
 
    void flag_chatty_entity(Entity& actor, int id_target) {
        actor.chatty_entities.push_back(id_target);
    }
    
    void update_chatiness(Entity& actor, int id_target) {
       double targets_chatiness = entity_types[network[id_target].entity_type].RF[1].const_val;
       // arbitrary factor greater than the average chatiness
       if (actor.avg_chatiness*2 < targets_chatiness) {
           flag_chatty_entity(actor, id_target);
       }
        actor.avg_chatiness = (actor.avg_chatiness*(actor.following_set.size() - 1) + targets_chatiness) / (double) actor.following_set.size();
    }
    // Returns true if a follow is added that was not already added
   bool handle_follow(int id_actor, int id_target) {
       Entity& A = network[id_actor];
       Entity& T = network[id_target];
       bool was_added = A.following_set.add(state, id_target);
       // if the follow is possible
       if (was_added) {
           FollowerSet::Context context(state, id_target);
           bool was_added = T.follower_set.add(context, id_actor);
           ASSERT(was_added, "Follow/follower-set asymmetry detected!");
           if (config.stage1_unfollow) {
               update_chatiness(A, id_target);
           }
           return true;
       }
       return false;
   }

   /***************************************************************************
    * Entity observation routines
    ***************************************************************************/
    
   int random_follow_method(int n_entities) {
       return rng.rand_int(n_entities);
   }
   int preferential_barabasi_follow_method() {
       double rand_num = rng.rand_real_not0();
       // if we want to use a preferential follow method
       int entity_to_follow = -1;
       double sum_of_weights = 0;
       updating_follow_probabilities.resize(follow_probabilities.size());
       /* search through the probabilities for each threshold and find
          the right bin to land in */
       for (int i = 0; i < follow_probabilities.size(); i ++){
           // look at each category
           CategoryEntityList& C = follow_ranks.categories[i];
           updating_follow_probabilities[i] = follow_probabilities[i]*C.entities.size();
           sum_of_weights += C.entities.size()*follow_probabilities[i];
       }
       for (int i = 0; i < follow_probabilities.size(); i ++ ){
           updating_follow_probabilities[i] /= sum_of_weights;
       }
       for (int i = 0; i < updating_follow_probabilities.size(); i ++) {
           if (rand_num <= updating_follow_probabilities[i]) {
               // point to the category we landed in
               CategoryEntityList& C = follow_ranks.categories[i];
               // make sure we're not pulling a entity from an empty list
               if (C.entities.size() != 0) {
                   // pull a random entity from whatever bin we landed in and break so we do not continue this loop
                   entity_to_follow = C.entities[rng.rand_int(C.entities.size())];
                   return entity_to_follow;
               }
           }
           // part of the above search
           rand_num -= updating_follow_probabilities[i];
       }
       return -1;
   }
   int preferential_follow_method() {
       double rand_num = rng.rand_real_not0();
       double prob_sum = 0;
       auto& updating_probs = updating_follow_probabilities;
       updating_probs.resize(follow_ranks.categories.size());
       for (int i = 0; i < follow_ranks.categories.size(); i ++) {
           CategoryEntityList& C = follow_ranks.categories[i];
           updating_probs[i] = follow_ranks.categories[i].prob * C.entities.size();
           prob_sum += follow_ranks.categories[i].prob * C.entities.size();
       }
       for (auto& p : updating_probs) {
           p /= prob_sum;
       }
       int i = 0;
       for (auto& p : updating_probs) {
           if (rand_num <= p) {
               CategoryEntityList& C = follow_ranks.categories[i];
               // pull a random entity from whatever bin we landed in and break so we do not continue this loop
               int entity_to_follow = C.entities[rng.rand_int(C.entities.size())];
               return entity_to_follow;
           }
           rand_num -= p;
           i++;
       }
       return -1;
   }
   int entity_follow_method() {
       // if we want to follow by entity class
       /* search through the probabilities for each entity and find the right bin to land in */
       double rand_num = rng.rand_real_not0();
       for (auto& type : entity_types) {
           if (rand_num <= type.prob_follow) {
               // make sure we're not pulling from an empty list
               if (type.entity_list.size() != 0) {
                   // pull the entity from whatever bin we landed in and break so we dont continue this loop
                   int n = rng.rand_int(type.entity_list.size());
                   int entity_to_follow = type.entity_list[n];
                   return entity_to_follow;
               }
           }
           // part of the above search
           rand_num -= type.prob_follow;
       }
       return -1;
   }
   int preferential_entity_follow_method() {
       int entity_to_follow = -1;
       double rand_num = rng.rand_real_not0();
       for (int i = 0; i < entity_types.size(); i++) {
           if (rand_num <= entity_types[i].prob_follow) {

               double another_rand_num = rng.rand_real_not0();
               // make sure we're not pulling from an empty list
               EntityType& et = entity_types[i];
               vector<double>& up = et.updating_probs;
               up.resize(et.follow_ranks.categories.size());
               double prob_sum = 0;
               for (int j = 0; j < et.follow_ranks.categories.size(); j ++) {
                   CategoryEntityList& C = et.follow_ranks.categories[j];
                   up[j] = et.follow_ranks.categories[j].prob * C.entities.size();
                   prob_sum += et.follow_ranks.categories[j].prob * C.entities.size();
               }
               for (int j = 0; j < up.size(); j ++) {                        
                   up[j] /= prob_sum;
               }
               for (int j = 0; j < up.size(); j ++) {
                   if (another_rand_num <= up[j]) {
                       CategoryEntityList& C = et.follow_ranks.categories[j];
                       // pull a random entity from whatever bin we landed in and break so we do not continue this loop                            
                       entity_to_follow = C.entities[rng.rand_int(C.entities.size())];
                       break;
                   }
                   another_rand_num -= up[j];
               }
           }
           if (entity_to_follow != -1){
               return entity_to_follow;
           }
           // part of the above search
           rand_num -= entity_types[i].prob_follow;
       }
       return -1;
   }
   int twitter_follow_model() {
       double rand_num = rng.rand_real_not0();
       /* different follow models:
           0 - random follow
           1 - preferential follow
           2 - entity type follow
           3 - preferential entity follow
       */
       int n_follow_models = 4;
       vector<double> follow_weights(n_follow_models);
       // all even weights for now, better organization after
       follow_weights[0] = 25.0;
       follow_weights[1] = 25.0;
       follow_weights[2] = 25.0;
       follow_weights[3] = 25.0;
       double sum_of_weights = 0;
       for (auto& weight : follow_weights) {
           sum_of_weights += weight;
       } for (auto& weight : follow_weights) {
           weight /= sum_of_weights;
       } 
       int follow_method = -1;
       for (int i = 0; i < follow_weights.size(); i ++) {
           if (rand_num <= follow_weights[i]) {
           follow_method = i;
           break;    
           } 
           rand_num -= follow_weights[i];
       }
       if (follow_method == 0) {
           return random_follow_method(network.n_entities);
       } else if (follow_method == 1) {
           return preferential_follow_method();
       } else if (follow_method == 2) {
           return entity_follow_method();
       } else {
           return preferential_entity_follow_method();
       }
       return -1;
    }
    
   // Returns false to signify that we must retry the KMC event
    bool follow_entity(int entity, int n_entities, double time_of_follow) {
        Entity& e1 = network[entity];
        int entity_to_follow = -1;
        double rand_num = rng.rand_real_not0();
        // if we want to do random follows
        if (config.follow_model == RANDOM_FOLLOW) {
            // find a random entity within [0:number of entities - 1]
            entity_to_follow = random_follow_method(n_entities);
        } else if (config.follow_model == PREFERENTIAL_FOLLOW && config.use_barabasi) {
            perf_timer_begin("AnalyzerFollower.follow_entity(barabasi_preferential_follow)");
            entity_to_follow = preferential_barabasi_follow_method();
            perf_timer_end("AnalyzerFollower.follow_entity(barabasi_preferential_follow)");
        } else if(config.follow_model == PREFERENTIAL_FOLLOW && !config.use_barabasi) {
            perf_timer_begin("AnalyzerFollower.follow_entity(preferential_follow)");
            entity_to_follow = preferential_follow_method();
            perf_timer_end("AnalyzerFollower.follow_entity(preferential_follow)");
        } else if (config.follow_model == ENTITY_FOLLOW) {
            perf_timer_begin("AnalyzerFollower.follow_entity(entity_follow)");
            entity_to_follow = entity_follow_method();
            perf_timer_end("AnalyzerFollower.follow_entity(entity_follow)");
        } else if (config.follow_model == PREFERENTIAL_ENTITY_FOLLOW) {
            perf_timer_begin("AnalyzerFollower.follow_entity(preferential_entity_follow)");
            entity_to_follow = preferential_entity_follow_method();
            perf_timer_end("AnalyzerFollower.follow_entity(preferential_entity_follow)");
        } else if (config.follow_model == TWITTER_FOLLOW) {
            perf_timer_begin("AnalyzerFollower.follow_entity(Twitter_model)");
            entity_to_follow = twitter_follow_model();
            perf_timer_end("AnalyzerFollower.follow_entity(Twitter_model)");
        }
        // if the stage1_follow is set to true in the inputfile
        if (config.stage1_unfollow) {
            vector<int>& chatties = e1.chatty_entities;
            if (chatties.size() > 0) {
                int index = rng.rand_int(chatties.size());
                int entity_unfollowed = chatties[index];
                if (action_unfollow(entity_unfollowed, entity)) {
                    chatties.erase(chatties.begin() + index);
                } else {
                    DEBUG_CHECK(action_unfollow(entity_unfollowed, entity), "Error in stage1_unfollow method.");
                }
            }
        }

        // check and make sure we are not following ourself, or we are following entity -1
        if (LIKELY(entity != entity_to_follow && entity_to_follow != -1)) {

            perf_timer_begin("AnalyzerFollower.follow_entity(handle_follow)");
            // point to the entity who is being followed
            if (handle_follow(entity, entity_to_follow)) {
                /* FEATURE: Follow-back based on target's prob_followback.
                 * Set in INFILE.yaml as followback_probability. */
                int et_id = network[entity_to_follow].entity_type;
                EntityType& et = entity_types[et_id];
                
                // TODO this followback process has to be another follow method that happens naturally at some other time, possibly another 'spike' in the rate
                if (config.use_flawed_followback && rng.random_chance(et.prob_followback)) {
                    analyzer_followback(state, entity, entity_to_follow);
                }
                // based on the number of followers the followed-entity has, check to make sure we're still categorized properly
                Entity& target = network[entity_to_follow];
                et.follow_ranks.categorize(entity_to_follow, target.follower_set.size());
                follow_ranks.categorize(entity_to_follow, target.follower_set.size());
                stats.n_follows++; // We were able to add the follow; almost always the case.
                entity_types[e1.entity_type].n_follows ++;
                entity_types[target.entity_type].n_followers ++;
                return true;
            }
            perf_timer_end("AnalyzerFollower.follow_entity(handle_follow)");
        }

        return false; // Completion failure: Restart the event
    }

    bool followback(int prev_actor_id, int prev_target_id) {
        // now the previous target will follow the previous actor back
        if (handle_follow(prev_target_id, prev_actor_id)) {
            Entity& prev_actor = network[prev_actor_id];
            Entity& prev_target = network[prev_target_id];
            int et_id = network[prev_actor_id].entity_type;
            EntityType& et = entity_types[et_id];
            et.follow_ranks.categorize(prev_actor_id, prev_actor.follower_set.size());
            follow_ranks.categorize(prev_actor_id, prev_actor.follower_set.size());
            stats.n_follows++; // We were able to add the follow; usually the case.
            state.event_stat_log(prev_target_id, EV_FOLLOWBACK);
            return true;
        }
        return false; // Completion failure: Can be ignored for followback, largely
    }
        
	bool action_unfollow(int id_unfollowed, int id_unfollower) {
	    perf_timer_begin("action_unfollow");
		FollowerSet& candidate_followers = network.follower_set(id_unfollowed);

		DEBUG_CHECK(id_unfollower != -1, "Should not be -1 after choice!");

		// Necessary for use with follower set:
        FollowerSet::Context context(state, id_unfollowed);
        // Remove our target from our actor's follows:
        bool had_follower = candidate_followers.remove(context, id_unfollower);
		DEBUG_CHECK(had_follower, "unfollow: Did not exist in follower list");

        // Remove our unfollowed person from our target's followers:
		Entity& e_lost_follower = network[id_unfollower];
		bool had_follow = e_lost_follower.following_set.remove(state, id_unfollowed);
		DEBUG_CHECK(had_follow, "unfollow: Did not exist in follow list");
		perf_timer_end("action_unfollow");
        stats.n_unfollows ++;
		return true;
	}
};

bool analyzer_follow_entity(AnalysisState& state, int entity, int n_entities, double time_of_follow) {
    AnalyzerFollow analyzer(state);
    return analyzer.follow_entity(entity, n_entities, time_of_follow);
}

bool analyzer_followback(AnalysisState& state, int follower, int followed) {
    AnalyzerFollow analyzer(state);
    return analyzer.followback(follower, followed);
}
