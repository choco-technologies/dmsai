#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmsai.h"

static dmsai_t g_handle = NULL;

void dmod_test_setup(void)
{
    g_handle = dmsai_create();
}

void dmod_test_teardown(void)
{
    dmsai_destroy(g_handle);
    g_handle = NULL;
}

DMOD_TEST_STEP(dmsai_create)
{
    DMOD_TEST_EXPECT_NOT_NULL(g_handle);
}

DMOD_TEST_STEP(dmsai_is_valid)
{
    DMOD_TEST_EXPECT_TRUE(dmsai_is_valid(g_handle));
}

DMOD_TEST_STEP(dmsai_destroy_null)
{
    /* Destroying NULL must not crash. */
    dmsai_destroy(NULL);
}
