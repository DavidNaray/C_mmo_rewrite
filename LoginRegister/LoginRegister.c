
#include "LoginRegister.h"

#include "../MongoDBReadWriteCache/Cache.h"
#include "../MongoDBReadWriteCache/Schema/UserBreakdown.h"

#include "../MongoDBReadWriteCache/ReadUser.h"
#include "../MongoDBReadWriteCache/UserUtils.h"
#include "../MongoDBReadWriteCache/Cache.h"
#include <mongoc/mongoc.h>

#include "../noiseLib/TerrainGeneration.h"
#include "../serverComm/ReadWriteServ.h"


void RegisterTask(void *arg){
    char msg[256];
    pthread_mutex_lock(&GlobalCache->lock);

    RegisterArgs us=*(RegisterArgs *) arg;
    free(arg);

    User* u=cache_get_user(GlobalCache,us.username);

    //if user exists, notify that user exists
    if (u != NULL) {
        pthread_mutex_unlock(&GlobalCache->lock);
        return;
    }

    //not found in cache so look up in db, if user exists in db add them to cache
    mongoc_collection_t *collection = mongoc_client_get_collection(mongoClient, "CDB", "Users");

    bson_t query;
    bson_init(&query);
    BSON_APPEND_UTF8(&query, "username", us.username);

    mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection,&query,NULL,NULL);

    const bson_t *doc;

    if (mongoc_cursor_next(cursor, &doc)) {
        mongoc_cursor_destroy(cursor);
        bson_destroy(&query);

        pthread_mutex_unlock(&GlobalCache->lock);
        
        snprintf(msg, sizeof(msg),
            "{\"type\":\"RegisterResult\",\"username\":\"%s\",\"RId\":%d}\n",
            us.username,
            us.RId
        );
        send_message(msg);
        return; // user exists in db
    }

    mongoc_cursor_destroy(cursor);
    bson_destroy(&query);

    //create new user
    User* nu = malloc(sizeof(User));
    if (nu == NULL) {
        /*if malloc fails somehow, notify user server failed*/
        
        pthread_mutex_unlock(&GlobalCache->lock);
        snprintf(msg, sizeof(msg),
            "{\"type\":\"RegisterFail\",\"username\":\"%s\",\"RId\":%d}\n",
            us.username,
            us.RId
        );
        send_message(msg);
        return;
    }

    // printf("reqid: %d\n", us.RId);
    // printf("making user: %s\n", u->username);
    user_init(nu,us.username,us.password);
    // printf("made user: %s\n", nu->username);
    
    //send message to user to go to gamepage (should be a loading screen to get their tiles)
    snprintf(msg, sizeof(msg),
        "{\"type\":\"RegisterResult\",\"username\":\"%s\",\"RId\":%d}\n",
        us.username,
        us.RId
    );
    send_message(msg);
    
    GenerateTerrainTile(0,0);
    printf("generated terrain for user: %s\n", nu->username);
    

    //add user to cache
    cache_insert_user(GlobalCache, nu);

    pthread_mutex_unlock(&GlobalCache->lock);
}

void LoginTask(void *arg){
    char msg[256];
    pthread_mutex_lock(&GlobalCache->lock);

    RegisterArgs us=*(RegisterArgs *) arg;
    free(arg);

    // check if user exists
    User* u=cache_get_user(GlobalCache,us.username);

    if(u != NULL){
        if(strcmp(u->passwordHash, us.password) != 0){
            pthread_mutex_unlock(&GlobalCache->lock);

            snprintf(msg, sizeof(msg),
                "{\"type\":\"LoginResult\",\"username\":\"%s\",\"RId\":%d}\n",
                us.username,
                us.RId
            );
            send_message(msg);
            return;
        }
    }

    //check database, add to cache exists and password match
    else{
        mongoc_collection_t *collection = mongoc_client_get_collection(mongoClient, "CDB", "Users");;

        bson_t query;
        bson_init(&query);
        BSON_APPEND_UTF8(&query, "username", us.username);

        mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(collection,&query,NULL,NULL);

        const bson_t *doc;

        if (!mongoc_cursor_next(cursor, &doc)) {
            mongoc_cursor_destroy(cursor);
            bson_destroy(&query);

            pthread_mutex_unlock(&GlobalCache->lock);
            
            snprintf(msg, sizeof(msg),
                "{\"type\":\"LoginFailed\",\"username\":\"%s\",\"RId\":%d}\n",
                us.username,
                us.RId
            );
            send_message(msg);
            return; // user doesn't exist
        }

        u = malloc(sizeof(User));
        if (u == NULL) {//if malloc fails somehow
            mongoc_cursor_destroy(cursor);
            bson_destroy(&query);

            pthread_mutex_unlock(&GlobalCache->lock);
            snprintf(msg, sizeof(msg),
                "{\"type\":\"LoginFailed\",\"username\":\"%s\",\"RId\":%d}\n",
                us.username,
                us.RId
            );
            send_message(msg);
            return;
        }
        
        bson_to_user(doc,u);

        mongoc_cursor_destroy(cursor);
        bson_destroy(&query);

        
        if(strcmp(u->passwordHash, us.password) != 0){
            free(u);

            pthread_mutex_unlock(&GlobalCache->lock);
            snprintf(msg, sizeof(msg),
                "{\"type\":\"LoginFailed\",\"username\":\"%s\",\"RId\":%d}\n",
                us.username,
                us.RId
            );
            send_message(msg);
            return;
        }
        
        cache_insert_user(GlobalCache, u);

        pthread_mutex_unlock(&GlobalCache->lock);
        return;
    } 

    printf("Found user: %s\n", u->username);
    snprintf(msg, sizeof(msg),
        "{\"type\":\"LoginResult\",\"username\":\"%s\",\"RId\":%d}\n",
        us.username,
        us.RId
    );
    send_message(msg);
    pthread_mutex_unlock(&GlobalCache->lock);
}
