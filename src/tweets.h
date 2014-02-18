#ifndef TWEETS_H_
#define TWEETS_H_

#include <vector>

#include "lcommon/smartptr.h"
#include "mtwist.h"

#include "TimeDepRateTree.h"
#include "cat_classes.h"

#include "events.h"
#include "entity_properties.h"

//struct OriginalTweet {
//    Language language; // The language that the tweet is in
//    double humour; // The humour level of the tweet, 0 to 1
//    int author_id; // The entity that created the original content
//    OriginalTweet(Language language, double humour, int author_id) :
//            language(language), humour(humour), author_id(author_id) {
//    }
//};
//
// A tweet is an orignal tweet if tweeter_id == content.author_id
//struct Tweet {
//    // The entity broadcasting the tweet
//    int tweeter_id;
//    smartptr<OriginalTweet> content;
//
//    Tweet(int tweeter_id, const smartptr<OriginalTweet>& content) :
//            tweeter_id(tweeter_id), content(content) {
//    }
//
//    bool is_original_tweet() {
//        return (content->author_id == tweeter_id);
//    }
//
//    bool is_retweet() {
//        return !is_original_tweet();
//    }
//    OriginalTweet* operator->() {
//        return content.get();
//    }
//};

// information for when a user tweets
struct TweetContent { // AD: TEMPORARY NOTE was TweetInfo
    int entity_ID;
    double time_of_tweet;
    double starting_rate, updating_rate;
    UsedEntities used_entities;
    Language language;
    int author_id; // The entity that created the original content
    TweetContent() {
        entity_ID = -1;
        starting_rate = updating_rate = -1;
        time_of_tweet = -1;
        language = N_LANGS; // Set to invalid
        author_id = -1;
    }
};

// information for when a user retweets

// A tweet is an orignal tweet if tweeter_id == content.author_id
struct Tweet { // AD: TEMPORARY NOTE was RetweetInfo
    // The entity broadcasting the tweet
    int tweeter_id;
    double starting_rate, updating_rate;
    UsedEntities used_entities;
    // A tweet is an orignal tweet if tweeter_id == content.author_id
    smartptr<TweetContent> content;
    double tweet_time;
    double creation_time;
    Tweet(const smartptr<TweetContent>& content = smartptr<TweetContent>()) : content(content) {
        tweeter_id = -1;
        starting_rate = updating_rate = 0;
        tweet_time = creation_time = 0;
    }
};

typedef std::vector<Tweet> TweetList;

struct TweetRateDeterminer {
    double get_rate() {
        return 0.01;
    }
};

struct TweetBank {
    TweetList active_retweet_list;
    TimeDepRateTree<Tweet, 1 /*Just one rate for now*/, TweetRateDeterminer> tree;

    double half_life;
    double tolerance;
    std::vector<double> half_life_function;

    TweetBank(TweetRateDeterminer determiner, double initial_resolution = 1, int number_of_bins = 1) :
            tree(determiner, initial_resolution, number_of_bins) {
        half_life = 90; // 90 minutes
        tolerance = 0.01;
    }

    // gives the value for Omega(t) discussed
    double get_omega(double time_of_tweet, double actual_time) {
        double value = exp((time_of_tweet - actual_time) / half_life);
        if (value < tolerance) {
            return -1;
        }
        return value;
    }

};

#endif
