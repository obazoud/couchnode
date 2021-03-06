#include "config.h"
#include "internal.h"
#include "auth-priv.h"
#include <gtest/gtest.h>
#define LIBCOUCHBASE_INTERNAL 1
#include <libcouchbase/couchbase.h>

class CredsTest : public ::testing::Test
{
};

static lcb_t create(const char *connstr = NULL) {
    lcb_create_st crst;
    memset(&crst, 0, sizeof crst);
    crst.version = 3;
    crst.v.v3.connstr = connstr;
    lcb_t ret;
    lcb_error_t rc = lcb_create(&ret, &crst);
    EXPECT_EQ(LCB_SUCCESS, rc);
    return ret;
}

TEST_F(CredsTest, testLegacyCreds)
{
    lcb_t instance;
    lcb_BUCKETCRED cred;
    ASSERT_EQ(LCB_SUCCESS, lcb_create(&instance, NULL));
    lcb::Authenticator& auth = *instance->settings->auth;
    ASSERT_TRUE(auth.username().empty());
    ASSERT_EQ(LCBAUTH_MODE_CLASSIC, auth.mode());

    ASSERT_EQ(1, auth.buckets().size());
    ASSERT_TRUE(auth.buckets().find("default")->second.empty());
    ASSERT_EQ("", auth.password_for("default"));
    ASSERT_EQ("default", auth.username_for("default"));

    // Try to add another user/password:
    lcb_BUCKETCRED creds = { "user2", "pass2" };
    ASSERT_EQ(LCB_SUCCESS, lcb_cntl(instance, LCB_CNTL_SET, LCB_CNTL_BUCKET_CRED, creds));
    ASSERT_EQ(2, auth.buckets().size());
    ASSERT_EQ("pass2", auth.buckets().find("user2")->second);
    ASSERT_EQ("user2", auth.username_for("user2"));
    ASSERT_EQ("pass2", auth.password_for("user2"));

    ASSERT_TRUE(auth.username().empty());
    ASSERT_TRUE(auth.password().empty());
    lcb_destroy(instance);
}

TEST_F(CredsTest, testRbacCreds) {
    lcb_t instance = create("couchbase://localhost/default?username=mark");
    lcb::Authenticator& auth = *instance->settings->auth;
    ASSERT_EQ("mark", auth.username());
    ASSERT_EQ(LCBAUTH_MODE_RBAC, auth.mode());
    ASSERT_TRUE(auth.buckets().empty());
    ASSERT_EQ("mark", auth.username_for("default"));
    ASSERT_EQ("", auth.password_for("default"));
    ASSERT_EQ("mark", auth.username_for("jane"));
    ASSERT_EQ("", auth.password_for("jane"));

    // Try adding a new bucket, it should fail
    ASSERT_EQ(LCB_OPTIONS_CONFLICT, auth.add("users", "secret", LCBAUTH_F_BUCKET));

    // Try using "old-style" auth. It should fail:
    ASSERT_EQ(LCB_OPTIONS_CONFLICT, auth.add("users", "secret", LCBAUTH_F_BUCKET|LCBAUTH_F_CLUSTER));
    // Username/password should remain unchanged:
    ASSERT_EQ("mark", auth.username());
    ASSERT_EQ("", auth.password());

    // Try *changing* the credentials
    ASSERT_EQ(LCB_SUCCESS, auth.add("jane", "seekrit", LCBAUTH_F_CLUSTER));
    ASSERT_EQ("jane", auth.username_for("default"));
    ASSERT_EQ("seekrit", auth.password_for("default"));
    lcb_destroy(instance);
}

TEST_F(CredsTest, testSharedAuth)
{
    lcb_t instance1, instance2;
    ASSERT_EQ(LCB_SUCCESS, lcb_create(&instance1, NULL));
    ASSERT_EQ(LCB_SUCCESS, lcb_create(&instance2, NULL));

    lcb_AUTHENTICATOR *auth = lcbauth_new();
    ASSERT_EQ(1, auth->refcount());

    lcb_set_auth(instance1, auth);
    ASSERT_EQ(2, auth->refcount());

    lcb_set_auth(instance2, auth);
    ASSERT_EQ(3, auth->refcount());

    ASSERT_EQ(instance1->settings->auth, instance2->settings->auth);
    lcb_destroy(instance1);
    lcb_destroy(instance2);
    ASSERT_EQ(1, auth->refcount());
    lcbauth_unref(auth);
}
