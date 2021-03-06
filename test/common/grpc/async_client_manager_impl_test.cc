#include "envoy/config/core/v3/grpc_service.pb.h"

#include "common/api/api_impl.h"
#include "common/grpc/async_client_manager_impl.h"

#include "test/mocks/stats/mocks.h"
#include "test/mocks/thread_local/mocks.h"
#include "test/mocks/upstream/cluster_manager.h"
#include "test/mocks/upstream/cluster_priority_set.h"
#include "test/test_common/test_time.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::Eq;
using ::testing::Return;

namespace Envoy {
namespace Grpc {
namespace {

class AsyncClientManagerImplTest : public testing::Test {
public:
  AsyncClientManagerImplTest()
      : api_(Api::createApiForTest()), stat_names_(scope_.symbolTable()),
        async_client_manager_(cm_, tls_, test_time_.timeSystem(), *api_, stat_names_) {}

  Upstream::MockClusterManager cm_;
  NiceMock<ThreadLocal::MockInstance> tls_;
  Stats::MockStore scope_;
  DangerousDeprecatedTestTime test_time_;
  Api::ApiPtr api_;
  StatNames stat_names_;
  AsyncClientManagerImpl async_client_manager_;
};

TEST_F(AsyncClientManagerImplTest, EnvoyGrpcOk) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");

  std::shared_ptr<Upstream::MockThreadLocalCluster> cluster =
      std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();
  EXPECT_CALL(cm_, getThreadLocalCluster(Eq("foo"))).WillOnce(Return(cluster.get()));
  EXPECT_CALL(*cluster, info());
  EXPECT_CALL(*cluster->cluster_.info_, addedViaApi());

  async_client_manager_.factoryForGrpcService(grpc_service, scope_,
                                              AsyncClientFactoryClusterChecks::ValidateStatic);
}

TEST_F(AsyncClientManagerImplTest, EnvoyGrpcUnknown) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");

  EXPECT_CALL(cm_, getThreadLocalCluster("foo"));
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_,
                                                  AsyncClientFactoryClusterChecks::ValidateStatic),
      EnvoyException, "Unknown gRPC client cluster 'foo'");
}

TEST_F(AsyncClientManagerImplTest, EnvoyGrpcDynamicCluster) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");

  std::shared_ptr<Upstream::MockThreadLocalCluster> cluster =
      std::make_shared<NiceMock<Upstream::MockThreadLocalCluster>>();
  EXPECT_CALL(cm_, getThreadLocalCluster(Eq("foo"))).WillOnce(Return(cluster.get()));
  EXPECT_CALL(*cluster, info());
  EXPECT_CALL(*cluster->cluster_.info_, addedViaApi()).WillOnce(Return(true));
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_,
                                                  AsyncClientFactoryClusterChecks::ValidateStatic),
      EnvoyException, "gRPC client cluster 'foo' is not static");
}

TEST_F(AsyncClientManagerImplTest, GoogleGrpc) {
  EXPECT_CALL(scope_, createScope_("grpc.foo."));
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("foo");

#ifdef ENVOY_GOOGLE_GRPC
  EXPECT_NE(nullptr, async_client_manager_.factoryForGrpcService(
                         grpc_service, scope_, AsyncClientFactoryClusterChecks::ValidateStatic));
#else
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_,
                                                  AsyncClientFactoryClusterChecks::ValidateStatic),
      EnvoyException, "Google C++ gRPC client is not linked");
#endif
}

TEST_F(AsyncClientManagerImplTest, GoogleGrpcIllegalChars) {
  EXPECT_CALL(scope_, createScope_("grpc.foo."));
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("foo");

  auto& metadata = *grpc_service.mutable_initial_metadata()->Add();
  metadata.set_key("illegalcharacter;");
  metadata.set_value("value");

#ifdef ENVOY_GOOGLE_GRPC
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_,
                                                  AsyncClientFactoryClusterChecks::ValidateStatic),
      EnvoyException, "Illegal characters in gRPC initial metadata.");
#else
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_,
                                                  AsyncClientFactoryClusterChecks::ValidateStatic),
      EnvoyException, "Google C++ gRPC client is not linked");
#endif
}

TEST_F(AsyncClientManagerImplTest, LegalGoogleGrpcChar) {
  EXPECT_CALL(scope_, createScope_("grpc.foo."));
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_google_grpc()->set_stat_prefix("foo");

  auto& metadata = *grpc_service.mutable_initial_metadata()->Add();
  metadata.set_key("_legal-character.");
  metadata.set_value("value");

#ifdef ENVOY_GOOGLE_GRPC
  EXPECT_NE(nullptr, async_client_manager_.factoryForGrpcService(
                         grpc_service, scope_, AsyncClientFactoryClusterChecks::ValidateStatic));
#else
  EXPECT_THROW_WITH_MESSAGE(
      async_client_manager_.factoryForGrpcService(grpc_service, scope_,
                                                  AsyncClientFactoryClusterChecks::ValidateStatic),
      EnvoyException, "Google C++ gRPC client is not linked");
#endif
}

TEST_F(AsyncClientManagerImplTest, EnvoyGrpcUnknownOk) {
  envoy::config::core::v3::GrpcService grpc_service;
  grpc_service.mutable_envoy_grpc()->set_cluster_name("foo");

  EXPECT_CALL(cm_, getThreadLocalCluster(Eq("foo"))).Times(0);
  ASSERT_NO_THROW(async_client_manager_.factoryForGrpcService(
      grpc_service, scope_, AsyncClientFactoryClusterChecks::Skip));
}

} // namespace
} // namespace Grpc
} // namespace Envoy
