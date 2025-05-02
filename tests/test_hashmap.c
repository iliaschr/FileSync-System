#include "../include/hashmap.h"
#include "acutest.h"

void test_hashmap_insert_search(void) {
    hashInit(100); // Init a hashmap

    // Create a sync_info_t manually
    sync_info_t* info = malloc(sizeof(sync_info_t));
    strcpy(info->source_dir, "src");
    strcpy(info->target_dir, "dst");
    info->active = 1;
    info->last_sync_time = 0;
    info->error_count = 0;

    hashInsert(info); // Insert

    sync_info_t* found = hashSearch("src"); // Search by key

    TEST_ASSERT(found != NULL);
    TEST_ASSERT(strcmp(found->source_dir, "src") == 0);
    TEST_ASSERT(strcmp(found->target_dir, "dst") == 0);
    TEST_ASSERT(found->active == 1);

    hashDestroy(); // Clean up
}

void test_hashmap_delete(void) {
    hashInit(100);

    sync_info_t* info = malloc(sizeof(sync_info_t));
    strcpy(info->source_dir, "dirA");
    strcpy(info->target_dir, "dirB");
    info->active = 1;
    info->last_sync_time = 0;
    info->error_count = 0;

    hashInsert(info);

    // Now delete
    sync_info_t* found = hashSearch("dirA");
    TEST_ASSERT(found != NULL);

    hashDelete(found);

    found = hashSearch("dirA");
    TEST_ASSERT(found == NULL);

    hashDestroy();
}

TEST_LIST = {
    { "Insert and Search in Hashmap", test_hashmap_insert_search },
    { "Delete from Hashmap", test_hashmap_delete },
    { NULL, NULL }
};
